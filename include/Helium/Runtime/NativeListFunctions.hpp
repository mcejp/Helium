#pragma once

#include <Helium/Runtime/ActivationContext.hpp>

namespace Helium
{
    class NativeListFunctions {
    public:
        static void add(NativeFunctionContext& ctx);
        static void remove(NativeFunctionContext& ctx);

        // Calling any of the functions below requires an Activation Scope
        static bool newList(size_t preallocSize, ValueRef* list_out);
        static bool addItem(Value list, ValueRef&& value);
        static bool setItem(Value list, size_t index, ValueRef&& value);
    };
}
