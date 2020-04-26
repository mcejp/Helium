#pragma once

#include <Helium/Runtime/Value.hpp>

#include <string>

namespace Helium
{
    // Purpose over bare pointer?
    struct StringPtr {
        operator const char*() const { return ptr; }

        const char* ptr;
    };

    // Note: These are not *guaranteed* to be strictly native, e.g. actual implementation may be in script code.
    // ALL functions in this class require an Activation Scope.
    class RuntimeFunctions {
    public:
        // These follow Implicit Conversion Rules
        static bool asBoolean(Value value, bool* value_out, bool raiseIfInvalid);
        static bool asInteger(Value var, Int_t* value_out, bool raiseIfInvalid);
        static bool asReal(Value var, Real_t* value_out, bool raiseIfInvalid);

        // TODO: this is not safe and unfortunately will need to go
        // TODO: why? because it doesn't copy and the original string might disappear?
        static bool asString(Value var, StringPtr* value_out, bool raiseIfInvalid);

        // Returns false if an exception was raised
        // These functions are higher-level than e.g. NativeListFunctions::setItem, because they check the value type
        static bool getIndexed(Value range, Value index, ValueRef* value_out);
        static bool setIndexed(Value range, Value index, ValueRef&& value);

        // Returns false if an exception was raised
        // These functions are higher-level than e.g. NativeObjectFunctions::setMember, because they check the value type
        static bool getProperty(Value object, const VMString& name, ValueRef* value_out, bool raiseIfNotExists);
        static bool setMember(Value object, const VMString& name, ValueRef&& value);

        // Returns Variable_undefined if an exception has been raised
        static ValueRef operatorAdd(Value left, Value right);
        static ValueRef operatorSub(Value left, Value right);
        static ValueRef operatorMul(Value left, Value right);
        static ValueRef operatorDiv(Value left, Value right);
        static ValueRef operatorMod(Value left, Value right);
        static ValueRef operatorNeg(Value left);

        static ValueRef operatorLogAnd(Value left, Value right);
        static ValueRef operatorLogOr(Value left, Value right);
        static ValueRef operatorLogNot(Value left);

        // Returns false if an exception was raised
        static bool operatorEquals(Value left, Value right, bool* result_out);
        static bool operatorGreaterThan(Value left, Value right, bool* result_out);
        static bool operatorLessThan(Value left, Value right, bool* result_out);

        // Returns false if an error occurs -- but *some* exception will be raised either way
        static bool raiseException(const char* desc);
    };
}
