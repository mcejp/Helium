#include <Helium/Assert.hpp>
#include <Helium/Runtime/NativeListFunctions.hpp>
#include <Helium/Runtime/NativeObjectFunctions.hpp>
#include <Helium/Runtime/RuntimeFunctions.hpp>

#include <limits>

#include <cinttypes>
#include <cstring>

namespace Helium
{
    using std::move;

    bool RuntimeFunctions::asBoolean(Value value, bool* value_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        switch ( value.type ) {
        case ValueType::undefined:
            helium_assert(value.type != ValueType::undefined);
            return false;

        case ValueType::nul:
            *value_out = false;
            return true;

        case ValueType::boolean:
            *value_out = value.booleanValue;
            return true;

        case ValueType::integer:
            *value_out = value.integerValue != 0;
            return true;

        case ValueType::real:
            *value_out = value.realValue != 0.0;
            return true;

        case ValueType::internal:
        case ValueType::list:
        case ValueType::nativeFunction:
        case ValueType::object:
        case ValueType::scriptFunction:
            *value_out = true;
            return true;

        case ValueType::string:
            *value_out = value.length > 0;
            return true;
        }
    }

    bool RuntimeFunctions::asInteger(Value var, Int_t* value_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (var.type == ValueType::integer) {
            *value_out = var.integerValue;
            return true;
        }
        else {
            RuntimeFunctions::raiseException("Expected an integer");
            return false;
        }
    }

    bool RuntimeFunctions::asReal(Value var, Real_t* value_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (var.type == ValueType::integer) {
            *value_out = static_cast<Real_t>(var.integerValue);
            return true;
        }
        else if (var.type == ValueType::real) {
            *value_out = var.realValue;
            return true;
        }
        else {
            RuntimeFunctions::raiseException("Expected a real");
            return false;
        }
    }

    bool RuntimeFunctions::asString(Value var, StringPtr* value_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (var.type == ValueType::string) {
            *value_out = StringPtr{ var.string->text };
            return true;
        }
        else {
            RuntimeFunctions::raiseException("Expected a string");
            return false;
        }
    }

    bool RuntimeFunctions::getIndexed(Value range, Value index, ValueRef* value_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (range.type == ValueType::list) {
            if (index.type == ValueType::integer) {
                if (index.integerValue >= 0 && index.integerValue < range.list->length) {
                    *value_out = ValueRef{range.list->items[index.integerValue].reference()};
                    return true;
                }
                else {
                    raiseException("List index out of range");
                    return false;
                }
            }
            else {
                raiseException("Invalid index for operator []");
                return false;
            }
        }
        else if (range.type == ValueType::string) {
            if (index.type == ValueType::integer) {
                if (index.integerValue >= 0 && index.integerValue < range.length) {
                    *value_out = ValueRef::makeInteger(range.string->text[index.integerValue]);
                    return true;
                }
                else {
                    raiseException("String index out of range");
                    return false;
                }
            }
            else {
                raiseException("Invalid index for operator []");
                return false;
            }
        }
        else {
            raiseException("Invalid value for operator []");
            return false;
        }
    }

    bool RuntimeFunctions::getProperty(Value object, const VMString& name, ValueRef* value_out, bool raiseIfNotExists) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);
        helium_assert_debug(object.type != ValueType::undefined);

        switch (object.type) {
        case ValueType::object: {
            *value_out = ValueRef{object.objectCloneProperty(name)};

            if (!(*value_out)->isUndefined())
                return true;
            else
                break;
        }

        case ValueType::integer:
            // TODO: use something like Int.prototype
            /*builtInMember( "ascii" )
            {
                char c = integerValue;
                return newString( &c, 1 );
            }*/
            if ( strcmp(name.text, "string") == 0 )
            {
                auto str = std::to_string(object.integerValue);
                *value_out = ValueRef::makeStringWithLength(str.c_str(), str.size());
                return true;
            }
            break;

        case ValueType::list:
            // TODO: use something like List.prototype
            if ( strcmp(name.text, "length") == 0 ) {
                *value_out = ValueRef::makeInteger(object.list->length);
                return true;
            }
            break;

        case ValueType::string:
            // TODO: use something like String.prototype
            if ( strcmp(name.text, "length") == 0 ) {
                *value_out = ValueRef::makeInteger(object.length);
                return true;
            }
            break;

        default: {
            /*if (raiseIfNotExists)
                raiseException(ctx, "Attempting to get property of a non-object");
            return false;*/
        }
        }

        if (raiseIfNotExists) {
            auto str = std::string("Property '") + name.text + "' does not exist";
            raiseException(str.c_str());
        }

        return false;
    }

    bool RuntimeFunctions::setIndexed(Value range, Value index, ValueRef&& value) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (range.type == ValueType::list) {
            if (index.type == ValueType::integer) {
                // TODO: don't hardcode uint32 here
                if (index.integerValue >= 0 && index.integerValue < range.list->length && index.integerValue < std::numeric_limits<uint32_t>::max()) {
                    size_t listIndex = static_cast<size_t>(index.integerValue);

                    return NativeListFunctions::setItem(range, listIndex, move(value));
                }
                else {
                    raiseException("List index out of range");
                    return false;
                }
            }
            else {
                raiseException("Invalid index for operator []");
                return false;
            }
        }
        else {
            raiseException("Invalid value for operator []");
            return false;
        }
    }

    bool RuntimeFunctions::setMember(Value object, const VMString& name, ValueRef&& value) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (object.type == ValueType::object) {
            return NativeObjectFunctions::setProperty(object, name, move(value), false);
        }
        else {
            raiseException("Attempting to set a member variable in a non-object");
            return false;
        }
    }

    ValueRef RuntimeFunctions::operatorAdd(Value left, Value right) {
        // FIXME: overflow/truncation checks
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::integer && right.type == ValueType::integer) {
            return ValueRef::makeInteger( left.integerValue + right.integerValue);
        }
        else if (left.type == ValueType::integer && right.type == ValueType::real) {
            return ValueRef::makeReal(static_cast<Real_t>(left.integerValue) + right.realValue );
        }
        /*else if (left.type == ValueType::integer && right.type == ValueType::string) {
            return Variable::newReal( ( double )( left.integerValue) + strtod( right.string->text, 0 ) );
        }*/
        else if (left.type == ValueType::real && right.type == ValueType::integer) {
            return ValueRef::makeReal( left.realValue + static_cast<Real_t>(right.integerValue) );
        }
        else if (left.type == ValueType::real && right.type == ValueType::real) {
            return ValueRef::makeReal( left.realValue + right.realValue);
        }
        /*else if (left.type == ValueType::real && right.type == ValueType::string) {
            return Variable::newReal( left.realValue + strtod( right.string->text, 0 ) );
        }*/
        else if (left.type == ValueType::string && right.type == ValueType::integer) {
            auto str = std::to_string(right.integerValue);
            return ValueRef{left.appendString( str.c_str(), str.size() )};
        }
        else if (left.type == ValueType::string && right.type == ValueType::real) {
            auto str = std::to_string(right.realValue);
            return ValueRef{left.appendString( str.c_str(), str.size() )};
        }
        else if (left.type == ValueType::string && right.type == ValueType::string) {
            return ValueRef{left.appendString( ( right.string )->text, right.length )};
        }
        else if (left.type == ValueType::list && right.type == ValueType::list) {
            ValueRef sum;

            if (!NativeListFunctions::newList( left.list->length + right.list->length, &sum ))
                return {};

            //* TODO: optimize for the case when no other references exist

            for ( unsigned i = 0; i < ( left.list )->length; i++ ) {
                if (!NativeListFunctions::addItem(sum, ValueRef::makeReference(left.list->items[i]))) {
                    return {};
                }
            }

            for ( unsigned i = 0; i < ( right.list )->length; i++ ) {
                if (!NativeListFunctions::addItem(sum, ValueRef::makeReference(right.list->items[i]))) {
                    return {};
                }
            }

            return sum;
        }
        else if (left.type == ValueType::object && right.type == ValueType::object) {
            ValueRef copy = ValueRef{left.replicate()};

            //* TODO: optimize for the case when no other references exist

            for ( unsigned i = 0; i < ( right.object )->numMembers; i++ ) {
                bool readOnly = (( right.object )->members[i].flags & Member_readOnly) != 0;

                if (!NativeObjectFunctions::setProperty(copy,
                                                        (right.object)->members[i].key,
                                                        ValueRef::makeReference(right.object->members[i].value),
                                                        readOnly))
                    return {};
            }

            return copy;
        }
        else {
            raiseException("Invalid operands to operator '+'");
            return {};
        }
    }

    ValueRef RuntimeFunctions::operatorSub(Value left, Value right) {
        // FIXME: overflow/truncation checks
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::integer && right.type == ValueType::integer) {
            return ValueRef::makeInteger( left.integerValue - right.integerValue);
        }
        else if (left.type == ValueType::real && right.type == ValueType::integer) {
            return ValueRef::makeReal( left.realValue - static_cast<Real_t>(right.integerValue) );
        }
        else if (left.type == ValueType::real && right.type == ValueType::real) {
            return ValueRef::makeReal( left.realValue - right.realValue);
        }
        else {
            raiseException("Invalid operands to operator '-'");
            return {};
        }
    }

    ValueRef RuntimeFunctions::operatorMul(Value left, Value right) {
        // FIXME: overflow/truncation checks
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::integer && right.type == ValueType::integer) {
            return ValueRef::makeInteger( left.integerValue * right.integerValue);
        }
        else if (left.type == ValueType::integer && right.type == ValueType::real) {
            return ValueRef::makeReal( static_cast<Real_t>(left.integerValue) * right.realValue );
        }
        else if (left.type == ValueType::real && right.type == ValueType::integer) {
            return ValueRef::makeReal( left.realValue * static_cast<Real_t>(right.integerValue) );
        }
        else if (left.type == ValueType::real && right.type == ValueType::real) {
            return ValueRef::makeReal( left.realValue * right.realValue);
        }
        else {
            raiseException("Invalid operands to operator '*'");
            return {};
        }
    }

    ValueRef RuntimeFunctions::operatorDiv(Value left, Value right) {
        // FIXME: overflow/truncation checks
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::integer && right.type == ValueType::integer) {
            if (right.integerValue == 0) {
                raiseException("Division by 0");
                return {};
            }

            return ValueRef::makeInteger( left.integerValue / right.integerValue);
        }
        else if (left.type == ValueType::integer && right.type == ValueType::real) {
            if (right.realValue == 0) {
                raiseException("Division by 0");
                return {};
            }

            return ValueRef::makeReal( static_cast<Real_t>(left.integerValue) / right.realValue );
        }
        else if (left.type == ValueType::real && right.type == ValueType::integer) {
            if (right.integerValue == 0) {
                raiseException("Division by 0");
                return {};
            }

            return ValueRef::makeReal( left.realValue / right.integerValue );
        }
        else if (left.type == ValueType::real && right.type == ValueType::real) {
            if (right.realValue == 0) {
                raiseException("Division by 0");
                return {};
            }

            return ValueRef::makeReal( left.realValue / right.realValue);
        }
        else {
            raiseException("Invalid operands to operator '/'");
            return {};
        }
    }

    ValueRef RuntimeFunctions::operatorMod(Value left, Value right) {
        // FIXME: overflow/truncation checks
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::integer && right.type == ValueType::integer) {
            if (right.integerValue == 0) {
                raiseException("Division by 0");
                return {};
            }

            return ValueRef::makeInteger( left.integerValue % right.integerValue);
        }
        else {
            raiseException("Invalid operands to operator '%'");
            return {};
        }
    }

    ValueRef RuntimeFunctions::operatorNeg(Value left) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if ( left.type == ValueType::integer )
            return ValueRef::makeInteger( -left.integerValue );
        else if ( left.type == ValueType::real )
            return ValueRef::makeReal( -left.realValue );
        else {
            raiseException("Invalid operand to unary operator '-'");
            return {};
        }
    }

    ValueRef RuntimeFunctions::operatorLogAnd(Value left, Value right) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::boolean && right.type == ValueType::boolean) {
            return ValueRef::makeBoolean(left.booleanValue && right.booleanValue);
        }
        else {
            raiseException("Invalid operands to operator '&&'");
            return {};
        }
    }

    ValueRef RuntimeFunctions::operatorLogOr(Value left, Value right) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::boolean && right.type == ValueType::boolean) {
            return ValueRef::makeBoolean(left.booleanValue || right.booleanValue);
        }
        else {
            raiseException("Invalid operands to operator '||'");
            return {};
        }
    }

    ValueRef RuntimeFunctions::operatorLogNot(Value left) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::boolean) {
            return ValueRef::makeBoolean(!left.booleanValue);
        }
        else {
            raiseException("Invalid operand to operator '!'");
            return {};
        }
    }

    bool RuntimeFunctions::operatorEquals(Value left, Value right, bool* result_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);
        helium_assert_debug(left.type != ValueType::undefined);
        helium_assert_debug(right.type != ValueType::undefined);

        if ( left.type != right.type ) {
            *result_out = false;
            return true;
        }

        switch ( left.type ) {
        case ValueType::undefined:
            helium_assert(left.type != ValueType::undefined);
            break;

        case ValueType::nul:
            *result_out = true;
            break;

        case ValueType::boolean:
            *result_out = left.booleanValue == right.booleanValue;
            break;

        case ValueType::integer:
            *result_out = left.integerValue == right.integerValue;
            break;

        case ValueType::real:
            *result_out = left.realValue == right.realValue;
            break;

        case ValueType::string:
            *result_out = strcmp( left.string->text, right.string->text ) == 0;
            break;

        case ValueType::object:
            *result_out = left.object == right.object;
            break;

        case ValueType::internal:
            *result_out = left.pointer == right.pointer;
            break;

        case ValueType::list:
            *result_out = left.list == right.list;
            break;

        case ValueType::nativeFunction:
            *result_out = left.nativeFunction == right.nativeFunction;
            break;

        case ValueType::scriptFunction:
            *result_out = left.length == right.length && left.functionIndex == right.functionIndex;
            break;
        }

        return true;
    }

    bool RuntimeFunctions::operatorGreaterThan(Value left, Value right, bool* result_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::integer && right.type == ValueType::integer) {
            *result_out = left.integerValue > right.integerValue;
            return true;
        }
        else if (left.type == ValueType::integer && right.type == ValueType::real) {
            *result_out = static_cast<Real_t>(left.integerValue) > right.realValue;
            return true;
        }
        else if (left.type == ValueType::real && right.type == ValueType::integer) {
            *result_out = left.realValue > static_cast<Real_t>(right.integerValue);
            return true;
        }
        else if (left.type == ValueType::real && right.type == ValueType::real) {
            *result_out = left.realValue > right.realValue;
            return true;
        }
        else {
            raiseException("Invalid operands to operator '>'");
            return false;
        }
    }

    bool RuntimeFunctions::operatorLessThan(Value left, Value right, bool* result_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (left.type == ValueType::integer && right.type == ValueType::integer) {
            *result_out = left.integerValue < right.integerValue;
            return true;
        }
        else if (left.type == ValueType::integer && right.type == ValueType::real) {
            *result_out = static_cast<Real_t>(left.integerValue) < right.realValue;
            return true;
        }
        else if (left.type == ValueType::real && right.type == ValueType::integer) {
            *result_out = left.realValue < static_cast<Real_t>(right.integerValue);
            return true;
        }
        else if (left.type == ValueType::real && right.type == ValueType::real) {
            *result_out = left.realValue < right.realValue;
            return true;
        }
        else {
            raiseException("Invalid operands to operator '<'");
            return false;
        }
    }

    bool RuntimeFunctions::raiseException(const char* desc) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        auto& ac = ActivationContext::getCurrent();
        ValueRef ex(Value::newObject(ac.getVM()));

        static const auto desc_vms = VMString::fromCString("desc");
        if (ex->objectSetProperty(desc_vms, Value::newString(desc), false) != Value::ObjectSetPropertyResult::success) {
            // This would be pretty bad
            helium_assert(false);
            return false;
        }

        ac.raiseException(move(ex));
        return true;
    }
}
