#include <Helium/Platform/ScriptContainer.hpp>
#include <Helium/Runtime/BindingHelpers.hpp>
#include <Helium/Runtime/NativeListFunctions.hpp>

#include <filesystem>
#include <fstream>

namespace Helium
{
    using std::move;
    using std::string;

    // print(...)
    static void print(NativeFunctionContext& ctx) {
        size_t count = ctx.getNumArgs();

        for (size_t i = 0; i < count; i++) {
            Value next = ctx.getArg(i);

            if (next.type == ValueType::object) {
                static const VMString stacktrace_vms = VMString::fromCString("stacktrace");

                ValueRef stacktrace;

                if (RuntimeFunctions::getProperty(next, stacktrace_vms, &stacktrace, false) && stacktrace->isList()) {
                    printf("====================================================================================\n");
                    printf("Exception: ");

                    static const VMString desc_vms = VMString::fromCString("desc");

                    ValueRef desc;

                    if (RuntimeFunctions::getProperty(next, desc_vms, &desc, false) && !desc->isUndefined())
                        desc->print();

                    printf("\n");
                    printf("Stack trace:");

                    // TODO: native_iterate_list
                    for (size_t i = 0; i < stacktrace->list->length; i++) {
                        printf("\t- ");
                        stacktrace->list->items[i].print();
                        printf("\n");
                    }

                    printf("\n");
                }
                else
                    next.print();
            }
            else
                next.print();
        }

        printf("\n");
        ctx.setReturnValue(ValueRef::makeInteger(count));
    }

    // Script
    template <>
    std::pair<const std::pair<const char*, NativeFunction>*, size_t> getMethods<Module>() {
        return {nullptr, 0};
    }

    template <>
    bool wrap(Module*&& value, ValueRef* value_out) { return wrapNewDelete(value, value_out); }

    template <>
    bool unwrap(Value var, Module** value_out) { return unwrapClass(var, value_out); }

    // ActivationContext

    // ActivationContext.callMainFunction(moduleIndex: int): void
    static void ActivationContext_callMainFunction(NativeFunctionContext& ctx, ActivationContext* activationContext, int moduleIndex) {
        activationContext->callMainFunction(moduleIndex);
    }

    // ActivationContext.getException(): any
    static void ActivationContext_getException(NativeFunctionContext& ctx, ActivationContext* activationContext) {
        // FIXME: undesired VM coupling
        ctx.setReturnValue(ValueRef::makeReference(activationContext->getException()));
    }

    // ActivationContext.getState(): int
    static int ActivationContext_getState(ActivationContext* activationContext) {
        return activationContext->getState();
    }

    // TODO: return a span, use a std::array
    template <>
    std::pair<const std::pair<const char*, NativeFunction>*, size_t> getMethods<ActivationContext>() {
        static const std::pair<const char*, NativeFunction> methods[]{
            { "callMainFunction",   wrapFunctionVoid<ActivationContext*, int, &ActivationContext_callMainFunction> },
            { "getException",       wrapFunctionVoid<ActivationContext*, &ActivationContext_getException> },
            { "getState",           wrapFunction<int, ActivationContext*, &ActivationContext_getState> },
            { "suspend",            wrapMethodVoid<ActivationContext, &ActivationContext::suspend> },
            { "resume",             wrapMethodVoid<ActivationContext, &ActivationContext::resume> },
        };

        return std::make_pair(methods, std::size(methods));
    }

    template <>
    bool wrap(ActivationContext*&& value, ValueRef* value_out) { return wrapNewDelete(value, value_out); }

    template <>
    bool unwrap(Value var, ActivationContext** value_out) { return unwrapClass(var, value_out); }

    // new ActivationContext(vm: VM)
    static void new_ActivationContext(NativeFunctionContext& ctx, VM* vm) {
        ValueRef object;

        if (!wrapNewDelete(new ActivationContext(vm), &object))
            return;

        // TODO: better way to do this
        NativeObjectFunctions::setProperty(object, "raisedException",
                                           ValueRef::makeInteger(ActivationContext::raisedException), true);
        NativeObjectFunctions::setProperty(object, "ready",
                                           ValueRef::makeInteger(ActivationContext::ready), true);
        NativeObjectFunctions::setProperty(object, "returnedValue",
                                           ValueRef::makeInteger(ActivationContext::returnedValue), true);
        NativeObjectFunctions::setProperty(object, "suspended",
                                           ValueRef::makeInteger(ActivationContext::suspended), true);

        ctx.setReturnValue(move(object));
    }

    // VM

    // VM.execute(): Variable
    static void VM_execute(VM* vm, ActivationContext* activationContext) {
        vm->execute(*activationContext);
    }

    // VM.run(): Variable
    /*static void VM_run(NativeFunctionContext& ctx, VM* vm, size_t entry) {
        Variable result;
        auto rc = vm->run(entry, &result);

        // WOAH! This coulpes 2 separate ActivationContexts.
        if (rc == ActivationContext::returnedValue)
            ctx.setReturnValue(result);
        else
            ctx.getActivationContext().raiseException(result);
    }*/

    template <>
    std::pair<const std::pair<const char*, NativeFunction>*, size_t> getMethods<VM>() {
        static constexpr std::pair<const char*, NativeFunction> methods[]{
            { "execute",            wrapFunctionVoid<VM*, ActivationContext*, VM_execute> },
            { "loadModule",         wrapMethod<ModuleIndex_t, VM, Module*, &VM::loadModule> },
            //{ "run",                wrapFunction<VM*, size_t, VM_run> },
        };

        return std::make_pair(methods, std::size(methods));
    }

    template <>
    bool wrap(VM*&& value, ValueRef* value_out) { return wrapNonOwning(value, value_out); }

    template <>
    bool unwrap(Value var, VM** value_out) { return unwrapClass(var, value_out); }

    static void new_VM(NativeFunctionContext& ctx) {
        ValueRef object;

        if (!wrapNewDelete(new VM(), &object))
            return;

        ctx.setReturnValue(move(object));
    }

    // Compiler.compileFile(fileName: string): Script?
    static void Compiler_compileFile(NativeFunctionContext& ctx, Compiler* compiler, StringPtr fileName) {
        try {
            auto script = compiler->compileFile(fileName.operator const char *());

            ValueRef wrapped;

            if (wrap(script.release(), &wrapped))
                ctx.setReturnValue(move(wrapped));
        }
        catch (CompileException const& ex) {
            RuntimeFunctions::raiseException(ex.desc.c_str());
        }
    }

    // Compiler.compileString(unitName: string, string: string): Script?
    static Module* Compiler_compileString(Compiler* compiler, StringPtr unitName, StringPtr string) {
        return compiler->compileString(unitName.operator const char*(), string.operator const char *()).release();
    }

    // Compiler.getVersion(): int
    static int Compiler_getVersion(Compiler* compiler) {
        return Compiler::version;
    }

    template <>
    std::pair<const std::pair<const char*, NativeFunction>*, size_t> getMethods<Compiler>() {
        static constexpr std::pair<const char*, NativeFunction> methods[] {
            { "compileFile",        wrapFunctionVoid<Compiler*, StringPtr, Compiler_compileFile> },
            { "compileString",      wrapFunction<Module*,       Compiler*, StringPtr, StringPtr, Compiler_compileString> },
            { "getVersion",         wrapFunction<int,           Compiler*, Compiler_getVersion> },
        };

        return std::make_pair(methods, std::size(methods));
    }

    template <>
    bool unwrap(Value var, Compiler** value_out) { return unwrapClass(var, value_out); }

    // Compiler()
    static void new_Compiler(NativeFunctionContext& ctx) {
        ValueRef object;

        if (!wrapNewDelete(new Compiler(), &object))
            return;

        ctx.setReturnValue(move(object));
    }

    // ScriptContainer

    // ScriptContainer.run()
    static void ScriptContainer_run(NativeFunctionContext& ctx, ScriptContainer* container) {
        container->run();
    }

    template <>
    std::pair<const std::pair<const char*, NativeFunction>*, size_t> getMethods<ScriptContainer>() {
        static constexpr std::pair<const char*, NativeFunction> methods[]{
                { "run",                wrapFunctionVoid<ScriptContainer*, ScriptContainer_run> },
        };

        return std::make_pair(methods, std::size(methods));
    }

    template <>
    bool unwrap(Value var, ScriptContainer** value_out) { return unwrapClass(var, value_out); }

    static void new_ScriptContainer(NativeFunctionContext& ctx, StringPtr moduleName, StringPtr moduleSource) {
        ValueRef object;

        if (!wrapNewDelete(new ScriptContainer(moduleName, moduleSource), &object))
            return;

        ctx.setReturnValue(move(object));
    }

    // getVM()
    static void getVM(NativeFunctionContext& ctx) {
        ValueRef var;

        if (wrap(ctx.getActivationContext().getVM(), &var))
            ctx.setReturnValue(move(var));
    }

    // used by tests; should be probably moved
    static void functionThatAcceptsUnsignedInt(NativeFunctionContext& ctx, unsigned int) {
    }

    static void listDirectory(NativeFunctionContext& ctx, StringPtr path) {
        std::error_code ec;
        auto iter = std::filesystem::directory_iterator(path.ptr, ec);

        if (ec) {
            RuntimeFunctions::raiseException("Failed to open directory");
            return;
        }

        ValueRef list;

        constexpr auto reserveCount = 20;

        if (!NativeListFunctions::newList(reserveCount, &list))
            return;

        for (const auto& entry : iter) {
            auto fn = entry.path().filename().u8string();
            ValueRef entryName{Value::newStringWithLength(fn.c_str(), fn.size())};

            if (!NativeListFunctions::addItem(list, move(entryName)))
                return;
        }

        ctx.setReturnValue(move(list));
    }

    void registerBuiltinFunctions(VM* vm) {
        vm->registerCallback("getVM", &getVM);
        vm->registerCallback("print", &print);

        vm->registerCallback("ActivationContext", &wrapFunctionVoid<VM*, new_ActivationContext>);
        vm->registerCallback("Compiler", &new_Compiler);
        vm->registerCallback("VM", &new_VM);

        vm->registerCallback("_functionThatAcceptsUnsignedInt", &wrapFunctionVoid<unsigned int, functionThatAcceptsUnsignedInt>);
        vm->registerCallback("_listDirectory", &wrapFunctionVoid<StringPtr, listDirectory>);

        //vm->registerCallback("ScriptContainer", &new_ScriptContainer);
    }
}
