#pragma once

#include <Helium/Runtime/ActivationContext.hpp>

namespace Helium
{
    class NativeObjectFunctions {
    public:
        // Calling any of the functions below requires an Activation Scope
        static bool newObject(ValueRef* object_out);
        static bool setProperty(Value object, const VMString& name, ValueRef&& value, bool readOnly);
        static bool setProperty(Value object, const char* name, ValueRef&& value, bool readOnly);
        static bool setProperty(Value object, const char* name, ValueRef&& value);
    };
}
