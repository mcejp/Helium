#include <Helium/Assert.hpp>
#include <Helium/Runtime/BindingHelpers.hpp>
#include <Helium/Runtime/NativeListFunctions.hpp>
#include <Helium/Runtime/RuntimeFunctions.hpp>

namespace Helium
{
    void NativeListFunctions::add(NativeFunctionContext& ctx) {
        if (ctx.getNumArgs() < 1) {
            RuntimeFunctions::raiseException("Not enough arguments to <List>.add()");
            return;
        }

        Value list = ctx.getArg(0);

        if (!list.isList()) {
            RuntimeFunctions::raiseException("Expected a list");
            return;
        }

        for (size_t i = 1; i < ctx.getNumArgs(); i++) {
            if (!addItem(list, ValueRef::makeReference(ctx.getArg(i)))) {
                return;
            }
        }
    }

    bool NativeListFunctions::addItem(Value list, ValueRef&& value) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (!list.listAddItem(value.detach())) {
            auto& ctx = ActivationContext::getCurrent();
            ctx.raiseOutOfMemoryException("Value::listAddItem");
            return false;
        }

        return true;
    }

    /*void NativeListFunctions::empty(NativeFunctionContext& ctx) {
        if (ctx.getNumArgs() < 1) {
            NativeRuntimeFunctions::raiseException(ctx.getActivationContext(), "Not enough arguments to <List>.empty()");
            return;
        }

        Variable list = ctx.getArg(0);
        ctx.setReturnValue(Variable::newBool(list.list->length == 0));
    }*/

    bool NativeListFunctions::newList(size_t preallocSize, ValueRef* list_out) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        auto& ctx = ActivationContext::getCurrent();
        *list_out = ValueRef{Value::newList(ctx.getVM(), preallocSize)};

        if ((*list_out)->type != ValueType::list) {
            ctx.raiseOutOfMemoryException("Value::newList");
            return false;
        }

        return true;
    }

    void NativeListFunctions::remove(NativeFunctionContext& ctx) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        if (ctx.getNumArgs() < 2 || ctx.getNumArgs() > 3) {
            RuntimeFunctions::raiseException("Unexpected number of arguments to <List>.remove()");
            return;
        }

        Value list = ctx.getArg(0);
        unsigned int offset;
        unsigned int count = 1;

        if (!list.isList()) {
            RuntimeFunctions::raiseException("Expected a list");
            return;
        }

        if (!unwrap(ctx.getArg(1), &offset))
            return;

        if (ctx.getNumArgs() >= 3 && !unwrap(ctx.getArg(2), &offset))
            return;

        list.listRemoveItems(offset, count);
    }

    bool NativeListFunctions::setItem(Value list, size_t index, ValueRef&& value) {
        helium_assert_debug(ActivationContext::getCurrentOrNull() != nullptr);

        // TODO: could be OOM or index truncation
        if (!list.listSetItem(index, value.detach())) {
            auto& ctx = ActivationContext::getCurrent();
            ctx.raiseOutOfMemoryException("Value::listSetItem");
            return false;
        }

        return true;
    }
}
