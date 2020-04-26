#include <Helium/Runtime/BindingHelpers.hpp>
#include <Helium/Runtime/RuntimeFunctions.hpp>
#include <Helium/Runtime/NativeStringFunctions.hpp>

#include <cstring>

namespace Helium
{
    void NativeStringFunctions::endsWith(NativeFunctionContext &ctx) {
        if (ctx.getNumArgs() != 2) {
            RuntimeFunctions::raiseException("Unexpected number of arguments to <String>.endsWith()");
            return;
        }

        Value string = ctx.getArg(0);
        StringPtr tail;

        if (!unwrap(ctx.getArg(1), &tail))
            return;

        size_t tailLength = strlen(tail);
        if (string.length >= tailLength) {
            // TODO: this might not be strictly UTF-8-correct
            if (memcmp(string.string->text + string.length - tailLength, tail, tailLength) == 0) {
                ctx.setReturnValue(ValueRef::makeBoolean(true));
                return;
            }
        }

        ctx.setReturnValue(ValueRef::makeBoolean(false));
    }

    void NativeStringFunctions::startsWith(NativeFunctionContext &ctx) {
        if (ctx.getNumArgs() != 2) {
            RuntimeFunctions::raiseException("Unexpected number of arguments to <String>.startsWith()");
            return;
        }

        Value string = ctx.getArg(0);
        StringPtr head;

        if (!unwrap(ctx.getArg(1), &head))
            return;

        size_t headLength = strlen(head);
        if (string.length >= headLength) {
            if (memcmp(string.string->text, head, headLength) == 0) {
                ctx.setReturnValue(ValueRef::makeBoolean(true));
                return;
            }
        }

        ctx.setReturnValue(ValueRef::makeBoolean(false));
    }
}
