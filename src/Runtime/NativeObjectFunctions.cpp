#include <Helium/Assert.hpp>
#include <Helium/Runtime/NativeObjectFunctions.hpp>
#include <Helium/Runtime/RuntimeFunctions.hpp>

namespace Helium
{
    using std::move;

    bool NativeObjectFunctions::newObject(ValueRef* object_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        auto& ctx = ActivationContext::getCurrent();
        *object_out = ValueRef{Value::newObject(ctx.getVM())};

        if ((*object_out)->type != ValueType::object) {
            ctx.raiseOutOfMemoryException("Value::newObject");
            return false;
        }

        return true;
    }

    bool NativeObjectFunctions::setProperty(Value object, const VMString& name, ValueRef&& value, bool readOnly) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        auto result = object.objectSetProperty(name, value.detach(), readOnly);

        switch (result) {
            case Value::ObjectSetPropertyResult::success:
                return true;

            case Value::ObjectSetPropertyResult::memoryError:
                RuntimeFunctions::raiseException("Memory allocation failed");
                return false;

            case Value::ObjectSetPropertyResult::propertyReadOnlyError:
                RuntimeFunctions::raiseException("Attempting to overwrite a read-only property");
                return false;
        }

        helium_unreachable();
    }

    bool NativeObjectFunctions::setProperty(Value object, const char* name, ValueRef&& value, bool readOnly) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        return setProperty(object, VMString::fromCString(name), move(value), readOnly);
    }

    bool NativeObjectFunctions::setProperty(Value object, const char* name, ValueRef&& value) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        return setProperty(object, VMString::fromCString(name), move(value), false);
    }
}
