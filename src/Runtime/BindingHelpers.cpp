#include <Helium/Runtime/BindingHelpers.hpp>

#include <limits>

namespace Helium
{
    template <>
    bool wrap(bool&& value, ValueRef* value_out) {
        *value_out = ValueRef::makeBoolean(value);
        return true;
    }

    template <>
    bool wrap(int&& value, ValueRef* value_out) {
        *value_out = ValueRef::makeInteger(value);
        return true;
    }

    template <>
    bool wrap(unsigned int&& value, ValueRef* value_out) {
        *value_out = ValueRef::makeInteger(value);
        return true;
    }

    template <>
    bool wrap(unsigned long int&& value, ValueRef* value_out) {
        *value_out = ValueRef::makeInteger(value);
        return true;
    }

    template <>
    bool unwrap(Value var, int* value_out) {
        Int_t value;

        if (!RuntimeFunctions::asInteger(var, &value, true))
            return false;

        if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
            RuntimeFunctions::raiseException("Value is out of range of native type 'int'");
            return false;
        }

        *value_out = static_cast<int>(value);
        return true;
    }

    template <>
    bool unwrap(Value var, unsigned int* value_out) {
        Int_t value;

        if (!RuntimeFunctions::asInteger(var, &value, true))
            return false;

        if (value < std::numeric_limits<unsigned int>::min() || value > std::numeric_limits<unsigned int>::max()) {
            RuntimeFunctions::raiseException("Value is out of range of native type 'unsigned int'");
            return false;
        }

        *value_out = static_cast<unsigned int>(value);
        return true;
    }

    // Commented out because uint64_t *should* replace it. This works on mingw-w64, needs to be checked on Linux
    /*template <>
    bool unwrap(ActivationContext& ctx, Variable var, unsigned long int* value_out) {
        Int_t value;

        if (!NativeRuntimeFunctions::asInteger(ctx, var, &value))
            return false;

        if (value < 0 || value > ULONG_MAX) {
            NativeRuntimeFunctions::raiseException(ctx, "Value is out of range of native type 'unsigned long int'");
            return false;
        }

        *value_out = (unsigned long int)value;
        return true;
    }*/

    // Needed at least on mingw-w64
    template <>
    bool unwrap(Value var, uint64_t* value_out) {
        Int_t value;

        if (!RuntimeFunctions::asInteger(var, &value, true))
            return false;

        if (value < std::numeric_limits<uint64_t>::min() || value > std::numeric_limits<uint64_t>::max()) {
            RuntimeFunctions::raiseException("Value is out of range of native type 'uint64_t'");
            return false;
        }

        *value_out = static_cast<uint64_t>(value);
        return true;
    }

    template <>
    bool unwrap(Value var, StringPtr* value_out) {
        if (!RuntimeFunctions::asString(var, value_out, true))
            return false;

        return true;
    }
}
