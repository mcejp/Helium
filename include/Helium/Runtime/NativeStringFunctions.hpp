#pragma once

#include <Helium/Runtime/ActivationContext.hpp>

namespace Helium
{
    class NativeStringFunctions {
    public:
        static void endsWith(NativeFunctionContext& ctx);
        static void startsWith(NativeFunctionContext& ctx);
    };
}
