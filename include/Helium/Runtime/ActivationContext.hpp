#pragma once

#include <Helium/Config.hpp>
#include <Helium/Runtime/InlineStack.hpp>
#include <Helium/Runtime/Value.hpp>

#include <deque>
#include <functional>
#include <stack>
#include <vector>

namespace Helium
{
    struct Instruction;
    struct InstructionOrigin;
    struct VMModule;
    struct ScriptFunction;
    class VM;

    typedef unsigned int ModuleIndex_t;

#ifdef HELIUM_MIXED_MODE
    union Register {
        Int_t int_;
    };
#endif

    // A stack frame. Always corresponds to a script function.
    struct Frame {
        const ScriptFunction* scriptFunction;

        size_t stackBase;

        // FIXME: this needs to be fixed-size! (for perf)
        std::vector<ValueRef> locals;

#ifdef HELIUM_MIXED_MODE
        std::vector<Register> registers;
#endif

        // These variables are cached in ActivationContext and only flushed on function calls!
        VMModule* module;
        ModuleIndex_t moduleIndex;
        CodeAddr_t pc;

        Frame() {}

#if HELIUM_TRACE_VALUES
        ~Frame();
#else
        ~Frame() {
        }
#endif

        Frame(const Frame&) = delete;
        void operator=(const Frame&) = delete;

        Value getLocal(size_t index)
        {
            if (index >= locals.size())
                locals.resize(index + 1);

            return locals[index];
        }

        void setLocal(size_t index, ValueRef&& value)
        {
            if (index >= locals.size()) {
                size_t oldSize = locals.size();
                locals.resize(index + 1);
            }
            
            locals[index] = std::move(value);
        }

#ifdef HELIUM_MIXED_MODE
        Int_t getRegisterInt(size_t index) {
            return registers[index].int_;
        }

        void setRegisterInt(size_t index, Int_t value) {
            if (index >= registers.size())
                registers.resize(index + 1);

            registers[index].int_ = value;
        }
#endif
    };

    class ActivationContext {
        public:
            enum State { ready, suspended, returnedValue, raisedException };

            explicit ActivationContext(VM* vm) : vm(vm) {}
            ~ActivationContext();
            ActivationContext(const ActivationContext&) = delete;
            void operator=(const ActivationContext&) = delete;

            static ActivationContext& getCurrent();
            static ActivationContext* getCurrentOrNull();

            // returns previous AC (or nullptr)
            static ActivationContext* setCurrent(ActivationContext* ac_or_null);

            Value getException() { return exception; }
            Instruction* getLastExecutedInstruction();          // return nullptr if none applicable
            State getState() const { return state; }
            VM* getVM() { return vm; }

            void resume() { state = ready; }
            void suspend() { state = suspended; }

            void callNativeFunction(NativeFunction func, size_t numArgs);
            void callNativeFunctionWithSelf(NativeFunction func, size_t numArgs, Value self);

            bool callMainFunction(ModuleIndex_t moduleIndex);
            bool callScriptFunction(ModuleIndex_t moduleIndex, CodeAddr_t functionIndex, size_t numArgs, ValueRef&& self);

            void invoke(Value callable, size_t numArgs);
            void invokeWithSelf(Value callable, Value self, size_t numArgs);

            void raiseException(ValueRef&& val);
            void raiseOutOfMemoryException(const char* where);

            void walkStack(std::function<void(InstructionOrigin*)> callback);

        private:
            bool enterFunction(const ScriptFunction& function, size_t numArgs);

            State state = ready;
            VM* vm;

            std::deque<Frame> frames;
            Frame* frame = nullptr;

            InlineStack<Value> stack;
            VMModule* activeModule = nullptr;
            ModuleIndex_t activeModuleIndex = -1;
            CodeAddr_t pc = -1;
            size_t numArgs = -1;

            ValueRef exception;

        friend class NativeFunctionContext;
        friend class VM;
    };

    class ActivationScope {
    public:
        explicit ActivationScope(ActivationContext& ctx);
        ~ActivationScope();

    private:
        ActivationContext* prev_ac;
    };

    class NativeFunctionContext {
        public:
            NativeFunctionContext(ActivationContext& ctx, size_t numArgs, ValueRef&& returnValue)
                    : ctx{ctx}, numArgs{numArgs}, returnValue{std::move(returnValue)} {
            }

            ActivationContext&  getActivationContext() { return ctx; }
            Value               getArg(size_t index) { return ctx.stack.getBelowTop(index); }
            size_t              getNumArgs() const { return numArgs; }
            ValueRef            moveReturnValue() { return std::move(returnValue); }
            void                setReturnValue(ValueRef&& value) { returnValue = std::move(value); }

        private:
            ActivationContext& ctx;
            size_t numArgs;

            ValueRef returnValue;
    };
}
