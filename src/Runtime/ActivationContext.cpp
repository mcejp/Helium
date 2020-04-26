#include <Helium/Assert.hpp>
#include <Helium/Config.hpp>
#include <Helium/Runtime/ActivationContext.hpp>
#include <Helium/Runtime/NativeListFunctions.hpp>
#include <Helium/Runtime/RuntimeFunctions.hpp>
#include <Helium/Runtime/NativeObjectFunctions.hpp>
#include <Helium/Runtime/VM.hpp>

#include <cstdarg>

#if HELIUM_TRACE_VALUES
#include <Helium/Runtime/Debug/ValueTrace.hpp>
#endif

namespace Helium
{
    using std::move;

    static ActivationContext* current_ac;

#if HELIUM_TRACE_VALUES
    Frame::~Frame() {
        ValueTraceCtx tracking_ctx("~Frame", this);

        for (size_t i = 0; i < locals.size(); i++)
            locals[i].reset();
    }
#endif

    ActivationContext::~ActivationContext() {
        while (!stack.isEmpty())
            stack.pop();
    }

    bool ActivationContext::callMainFunction(ModuleIndex_t moduleIndex) {
        auto mainFunctionIndex = vm->getModuleByIndex(moduleIndex)->findMainFunction();

        if (mainFunctionIndex.has_value()) {
            return this->callScriptFunction(moduleIndex, *mainFunctionIndex, 0, ValueRef());
        }
        else {
            RuntimeFunctions::raiseException("Module is not executable");
            return false;
        }
    }

    bool ActivationContext::callScriptFunction(ModuleIndex_t moduleIndex, CodeAddr_t functionIndex, size_t numArgs,
                                               ValueRef&& self) {
        if (frame) {
            frame->module = this->activeModule;
            frame->moduleIndex = this->activeModuleIndex;
            frame->pc = this->pc;
        }

        frames.resize(frames.size() + 1);
        frame = &frames.back();

        this->activeModule = vm->getModuleByIndex(moduleIndex);
        this->activeModuleIndex = moduleIndex;

        const auto& function = this->activeModule->functions[functionIndex];
        this->pc = function.start;
        this->numArgs = numArgs;

        frame->scriptFunction = &function;
        frame->stackBase = stack.getHeight();

        // TODO: the goal is to pass 'self' as an argument instead
        frame->setLocal(0, std::move(self));

        return enterFunction(function, numArgs);
    }

    void ActivationContext::callNativeFunction(NativeFunction func, size_t numArgs) {
        NativeFunctionContext ctx{ *this, numArgs, ValueRef::makeNil() };
        func(ctx);

        for (size_t i = 0; i < numArgs; i++)
            this->stack.pop();

        this->stack.push(ctx.moveReturnValue());
    }

    void ActivationContext::callNativeFunctionWithSelf(NativeFunction func, size_t numArgs, Value self) {
        this->stack.push(ValueRef::makeReference(self));
        numArgs++;

        NativeFunctionContext ctx{ *this, numArgs, ValueRef::makeNil() };
        func(ctx);

        for (size_t i = 0; i < numArgs; i++)
            this->stack.pop();

        this->stack.push(ctx.moveReturnValue());
    }

    bool ActivationContext::enterFunction(const ScriptFunction& function, size_t numArgs) {
        switch (function.argumentListType) {
        case ScriptFunction::ArgumentListType::explicit_: {
            auto expected = function.numExplicitArguments;
            auto obtained = numArgs;

            if (obtained != expected) {
                // TODO: better diagnostic please
                RuntimeFunctions::raiseException("Incorrect number of arguments in function call");
                return false;
            }

            for ( unsigned i = 0; i < expected; i++ ) {
                frame->setLocal( i + 1, stack.pop() );
            }

            return true;
        }
        }
    }

    ActivationContext& ActivationContext::getCurrent() {
        helium_assert(current_ac != nullptr);

        return *current_ac;
    }

    ActivationContext* ActivationContext::getCurrentOrNull() {
        return current_ac;
    }

    Instruction* ActivationContext::getLastExecutedInstruction() {
        if (this->activeModule != nullptr && this->pc > 0) {
            return &this->activeModule->instructions[this->pc - 1];
        }
        else {
            return nullptr;
        }
    }

    // TODO: should be bool (callScriptFunction can fail)
    void ActivationContext::invoke(Value callable, size_t numArgs) {
        if (callable.type == ValueType::nativeFunction) {
            this->callNativeFunction(callable.nativeFunction, numArgs);
        }
        else if (callable.type == ValueType::scriptFunction) {
            this->callScriptFunction(callable.getScriptFunctionModuleIndex(), callable.functionIndex, numArgs,
                                     ValueRef());
        }
        else {
            RuntimeFunctions::raiseException("Attempting to call a non-function");
        }
    }

    // TODO: should be bool (callScriptFunction can fail)
    void ActivationContext::invokeWithSelf(Value callable, Value self, size_t numArgs) {
        if (callable.type == ValueType::nativeFunction) {
            this->callNativeFunctionWithSelf(callable.nativeFunction, numArgs, self);
        }
        else if (callable.type == ValueType::scriptFunction) {
            this->callScriptFunction(callable.getScriptFunctionModuleIndex(), callable.functionIndex, numArgs,
                                     ValueRef::makeReference(self));
        }
        else {
            RuntimeFunctions::raiseException("Attempting to call a non-function");
        }
    }

    void ActivationContext::raiseException(ValueRef&& val) {
        // FIXME: need to prevent infinite recursion e.g. if stacktrace.listAddItem fails

        if (val->isObject()) {
            ValueRef stacktrace;

            if (!NativeListFunctions::newList(5, &stacktrace))
                return;

            this->walkStack([&stacktrace, this](InstructionOrigin *origin) {
                const auto &function = *origin->function.get();
                const auto &unit = *origin->unit.get();
                auto str = function + " (" + unit + ":" + std::to_string(origin->line) + ")";
                ValueRef entry(Value::newStringWithLength(str.c_str(), str.size()));

                // Might raise OOM exception. Tough shit.
                NativeListFunctions::addItem(stacktrace, move(entry));
            });

            NativeObjectFunctions::setProperty(val, "stacktrace", move(stacktrace), false);
        }

        this->exception = std::move(val);
        this->state = raisedException;
    }

    void ActivationContext::raiseOutOfMemoryException(const char* where) {
        // TODO...
        abort();
    }

    ActivationContext* ActivationContext::setCurrent(ActivationContext* ac_or_null) {
        auto prev = current_ac;
        current_ac = ac_or_null;
        return prev;
    }

    void ActivationContext::walkStack(std::function<void(InstructionOrigin*)> callback) {
        for (auto it = frames.rbegin(); it != frames.rend(); it++) {
            // Current frame? Fetch instruction from activeModule
            // TODO: Why is this a special case?
            if (&(*it) == frame) {
                // Dangerous
                Instruction* next = &this->activeModule->instructions[this->pc - 1];

                if (next && next->origin) {
                    callback(next->origin);
                    continue;
                }
            }

            // Previous frame? Fetch instruction from its module.
            // Also dangerous
            Instruction* next = &(*it).module->instructions[(*it).pc - 1];

            if (next && next->origin) {
                callback(next->origin);
                continue;
            }
        }
    }

    ActivationScope::ActivationScope(ActivationContext& ctx) {
        this->prev_ac = ActivationContext::setCurrent(&ctx);
    }

    ActivationScope::~ActivationScope() {
        ActivationContext::setCurrent(this->prev_ac);
    }
}
