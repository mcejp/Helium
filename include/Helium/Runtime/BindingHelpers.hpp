#pragma once

#include <Helium/Runtime/RuntimeFunctions.hpp>
#include <Helium/Runtime/NativeObjectFunctions.hpp>
#include <Helium/Runtime/VM.hpp>

#include <tuple>
#include <utility>

namespace Helium
{
    // TODO: Make use of Activation Scope

    static const VMString native_vms = VMString::fromCString(".native");

    template <class C>
    std::pair<const std::pair<const char*, NativeFunction>*, size_t> getMethods();

    template <typename T>
    bool unwrap(Value var, T* value_out);

    template <typename T>
    bool wrap(T&& value, ValueRef* value_out);

    template <class C>
    bool unwrapClass(Value object, C** ptr_out) {
        ValueRef native;

        if (!RuntimeFunctions::getProperty(object, native_vms, &native, false) || native->type != ValueType::internal) {
            RuntimeFunctions::raiseException("unwrapClass: missing native pointer in passed object");
            return false;
        }

        *ptr_out = static_cast<C*>(native->pointer);
        return true;
    }

    template <class C>
    bool unwrapPointer(Value var, C** ptr_out) {
        if (var.type != ValueType::internal) {
            RuntimeFunctions::raiseException("unwrapPointer: incorrect value type");
            return false;
        }

        *ptr_out = static_cast<C*>(var.pointer);
        return true;
    }

    template <class C>
    void deleteInternal(Value object) {
        if (object.type != ValueType::object)
            return;

        ValueRef native{object.objectCloneProperty(native_vms)};

        if (native->type == ValueType::internal) {
            auto ptr = static_cast<C*>(native->pointer);
            delete ptr;
        }
    }

    template <class C>
    bool wrapPointer(C* ptr, ValueRef* value_out) {
        if (ptr == nullptr) {
            *value_out = ValueRef::makeNul();
            return true;
        }

        *value_out = ValueRef::makeInternal(ptr);
        return true;
    }

    template <class C>
    bool wrapNewDelete(C* ptr, ValueRef* value_out) {
        if (ptr == nullptr) {
            *value_out = ValueRef::makeNul();
            return true;
        }

        ValueRef object;

        if (!NativeObjectFunctions::newObject(&object))
            return false;

        auto methods = getMethods<C>();

        for (size_t i = 0; i < methods.second; i++) {
            auto pair = methods.first[i];

            if (!NativeObjectFunctions::setProperty(object, pair.first, ValueRef::makeNativeFunction(pair.second),
                                                    true))
                return false;
        }

        if (!NativeObjectFunctions::setProperty(object, ".native", ValueRef::makeInternal(ptr), true))
            return false;

        //copy.object->clone = &cloneFile;
        object->object->finalize = deleteInternal<C>;

        *value_out = std::move(object);
        return true;
    }

    template <class C>
    bool wrapNonOwning(C* ptr, ValueRef* value_out) {
        if (ptr == nullptr) {
            *value_out = ValueRef::makeNul();
            return true;
        }

        ValueRef object;

        if (!NativeObjectFunctions::newObject(&object))
            return false;

        auto methods = getMethods<C>();

        for (size_t i = 0; i < methods.second; i++) {
            auto pair = methods.first[i];

            if (!NativeObjectFunctions::setProperty(object, pair.first, ValueRef{Value::newNativeFunction(pair.second)},
                                                    true))
                return false;
        }

        if (!NativeObjectFunctions::setProperty(object, ".native", ValueRef::makeInternal(ptr), true))
            return false;

        //copy.object->clone = &cloneFile;

        *value_out = std::move(object);
        return true;
    }

    template <size_t num>
    bool checkNumberOfArguments(NativeFunctionContext& ctx) {
        if (ctx.getNumArgs() != num) {
            RuntimeFunctions::raiseException("Invalid number of arguments");
            return false;
        }

        return true;
    }

    template <typename ReturnValue, typename Arg0, ReturnValue (*function)(Arg0)>
    void wrapFunction(NativeFunctionContext& ctx) {
        Arg0 arg0;

        if (!checkNumberOfArguments<1>(ctx) || !unwrap(ctx.getArg(0), &arg0))
            return;

        ValueRef returnValue;
        if (wrap(function(std::move(arg0)), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }

    template <typename Arg0, void (*function)(NativeFunctionContext&, Arg0)>
    void wrapFunctionVoid(NativeFunctionContext& ctx) {
        Arg0 arg0;

        if (!checkNumberOfArguments<1>(ctx) || !unwrap(ctx.getArg(0), &arg0))
            return;

        function(ctx, std::move(arg0));
    }

    template <typename Arg0, typename Arg1, void (*function)(NativeFunctionContext&, Arg0, Arg1)>
    void wrapFunctionVoid(NativeFunctionContext& ctx) {
        Arg0 arg0; Arg1 arg1;

        if (!checkNumberOfArguments<2>(ctx) || !unwrap(ctx.getArg(0), &arg0) || !unwrap(ctx.getArg(1), &arg1))
            return;

        function(ctx, std::move(arg0), std::move(arg1));
    }

    template <typename Arg0, typename Arg1, void(*function)(Arg0, Arg1)>
    void wrapFunctionVoid(NativeFunctionContext& ctx) {
        Arg0 arg0; Arg1 arg1;

        if (!checkNumberOfArguments<2>(ctx) || !unwrap(ctx.getArg(0), &arg0) || !unwrap(ctx.getArg(1), &arg1))
            return;

        function(std::move(arg0), std::move(arg1));
    }

    template <typename ReturnValue, typename Arg0, typename Arg1, ReturnValue(*function)(Arg0, Arg1)>
    void wrapFunction(NativeFunctionContext& ctx) {
        Arg0 arg0; Arg1 arg1;

        if (!checkNumberOfArguments<2>(ctx) || !unwrap(ctx.getArg(0), &arg0) || !unwrap(ctx.getArg(1), &arg1))
            return;

        ValueRef returnValue;
        if (wrap(function(std::move(arg0), std::move(arg1)), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }

    template <typename ReturnValue, typename Arg0, typename Arg1, typename Arg2, ReturnValue (*function)(Arg0, Arg1, Arg2)>
    void wrapFunction(NativeFunctionContext& ctx) {
        Arg0 arg0; Arg1 arg1; Arg2 arg2;

        if (!checkNumberOfArguments<3>(ctx) || !unwrap(ctx.getArg(0), &arg0) || !unwrap(ctx.getArg(1), &arg1)
                || !unwrap(ctx.getArg(2), &arg2))
            return;

        ValueRef returnValue;
        if (wrap(function(std::move(arg0), std::move(arg1), std::move(arg2)), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }

    //
    // method
    //

    template <class C, void (C::*method)()>
    void wrapMethodVoid(NativeFunctionContext& ctx) {
        C* ptr;

        if (!checkNumberOfArguments<1>(ctx) || !unwrap(ctx.getArg(0), &ptr))
            return;

        (ptr->*method)();
    }

    template <class C, typename Arg0, void (C::*method)(Arg0)>
    void wrapMethodVoid(NativeFunctionContext& ctx) {
        C* ptr; Arg0 arg0;

        if (!checkNumberOfArguments<2>(ctx) || !unwrap(ctx.getArg(0), &ptr) || !unwrap(ctx.getArg(1), &arg0))
            return;

        (ptr->*method)(std::move(arg0));
    }

    template <class C, typename Arg0, typename Arg1, void (C::*method)(Arg0, Arg1)>
    void wrapMethodVoid(NativeFunctionContext& ctx) {
        C* ptr; Arg0 arg0; Arg1 arg1;

        if (!checkNumberOfArguments<3>(ctx) || !unwrap(ctx.getArg(0), &ptr) || !unwrap(ctx.getArg(1), &arg0)
            || !unwrap(ctx.getArg(2), &arg1))
            return;

        (ptr->*method)(std::move(arg0), std::move(arg1));
    }

    template <class C, typename Arg0, typename Arg1, typename Arg2, void (C::*method)(Arg0, Arg1, Arg2)>
    void wrapMethodVoid(NativeFunctionContext& ctx) {
        C* ptr; Arg0 arg0; Arg1 arg1; Arg2 arg2;

        if (!checkNumberOfArguments<4>(ctx) || !unwrap(ctx.getArg(0), &ptr) || !unwrap(ctx.getArg(1), &arg0)
            || !unwrap(ctx.getArg(2), &arg1) || !unwrap(ctx.getArg(3), &arg2))
            return;

        (ptr->*method)(std::move(arg0), std::move(arg1), std::move(arg2));
    }

    //
    //  ReturnValue, method
    //

    template <typename ReturnValue, class C, ReturnValue(C::*method)()>
    void wrapMethod(NativeFunctionContext& ctx) {
        C* ptr;

        if (!checkNumberOfArguments<1>(ctx) || !unwrap(ctx.getArg(0), &ptr))
            return;

        ValueRef returnValue;
        if (wrap((ptr->*method)(), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }

    template <typename ReturnValue, class C, typename Arg0, ReturnValue (C::*method)(Arg0)>
    void wrapMethod(NativeFunctionContext& ctx) {
        C* ptr;
        Arg0 arg0;

        if (!checkNumberOfArguments<2>(ctx) || !unwrap(ctx.getArg(0), &ptr) || !unwrap(ctx.getArg(1), &arg0))
            return;

        ValueRef returnValue;
        if (wrap((ptr->*method)(std::move(arg0)), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }

    template <typename ReturnValue, class C, typename Arg0, typename Arg1, ReturnValue(C::*method)(Arg0, Arg1)>
    void wrapMethod(NativeFunctionContext& ctx) {
        C* ptr;
        Arg0 arg0; Arg1 arg1;

        if (!checkNumberOfArguments<3>(ctx) || !unwrap(ctx.getArg(0), &ptr) || !unwrap(ctx.getArg(1), &arg0)
            || !unwrap(ctx.getArg(2), &arg1))
            return;

        ValueRef returnValue;
        if (wrap((ptr->*method)(std::move(arg0), std::move(arg1)), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }

    template <typename ReturnValue, class C, typename Arg0, typename Arg1, typename Arg2, ReturnValue(C::*method)(Arg0, Arg1, Arg2)>
    void wrapMethod(NativeFunctionContext& ctx) {
        C* ptr;
        Arg0 arg0; Arg1 arg1; Arg2 arg2;

        if (!checkNumberOfArguments<4>(ctx) || !unwrap(ctx.getArg(0), &ptr) || !unwrap(ctx.getArg(1), &arg0)
            || !unwrap(ctx.getArg(2), &arg1) || !unwrap(ctx.getArg(3), &arg2))
            return;

        ValueRef returnValue;
        if (wrap((ptr->*method)(std::move(arg0), std::move(arg1), std::move(arg2)), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }

    template <typename ReturnValue, class C, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, ReturnValue(C::*method)(Arg0, Arg1, Arg2, Arg3, Arg4)>
    void wrapMethod(NativeFunctionContext& ctx) {
        C* ptr;
        Arg0 arg0; Arg1 arg1; Arg2 arg2; Arg3 arg3; Arg4 arg4;

        if (!checkNumberOfArguments<6>(ctx) || !unwrap(ctx.getArg(0), &ptr) || !unwrap(ctx.getArg(1), &arg0)
            || !unwrap(ctx.getArg(2), &arg1) || !unwrap(ctx.getArg(3), &arg2) || !unwrap(ctx.getArg(4), &arg3)
            || !unwrap(ctx.getArg(5), &arg4))
            return;

        ValueRef returnValue;
        if (wrap((ptr->*method)(std::move(arg0), std::move(arg1), std::move(arg2), std::move(arg3), std::move(arg4)), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }

    template <typename ReturnValue, class C, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, ReturnValue(C::*method)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5)>
    void wrapMethod(NativeFunctionContext& ctx) {
        C* ptr;
        Arg0 arg0; Arg1 arg1; Arg2 arg2; Arg3 arg3; Arg4 arg4; Arg5 arg5;

        if (!checkNumberOfArguments<7>(ctx) || !unwrap(ctx.getArg(0), &ptr) || !unwrap(ctx.getArg(1), &arg0)
            || !unwrap(ctx.getArg(2), &arg1) || !unwrap(ctx.getArg(3), &arg2) || !unwrap(ctx.getArg(4), &arg3)
            || !unwrap(ctx.getArg(5), &arg4) || !unwrap(ctx.getArg(6), &arg5))
            return;

        ValueRef returnValue;
        if (wrap((ptr->*method)(std::move(arg0), std::move(arg1), std::move(arg2), std::move(arg3), std::move(arg4), std::move(arg5)), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }

    template <typename ReturnValue, class C, typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7, ReturnValue(C::*method)(Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7)>
    void wrapMethod(NativeFunctionContext& ctx) {
        C* ptr;
        Arg0 arg0; Arg1 arg1; Arg2 arg2; Arg3 arg3; Arg4 arg4; Arg5 arg5; Arg6 arg6; Arg7 arg7;

        if (!checkNumberOfArguments<9>(ctx) || !unwrap(ctx.getArg(0), &ptr) || !unwrap(ctx.getArg(1), &arg0)
            || !unwrap(ctx.getArg(2), &arg1) || !unwrap(ctx.getArg(3), &arg2) || !unwrap(ctx.getArg(4), &arg3)
            || !unwrap(ctx.getArg(5), &arg4) || !unwrap(ctx.getArg(6), &arg5) || !unwrap(ctx.getArg(7), &arg6)
            || !unwrap(ctx.getArg(8), &arg7))
            return;

        ValueRef returnValue;
        if (wrap((ptr->*method)(std::move(arg0), std::move(arg1), std::move(arg2), std::move(arg3), std::move(arg4), std::move(arg5), std::move(arg6), std::move(arg7)), &returnValue))
            ctx.setReturnValue(std::move(returnValue));
    }
}
