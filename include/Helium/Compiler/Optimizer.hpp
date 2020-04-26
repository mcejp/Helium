#pragma once

#include <Helium/Runtime/Code.hpp>

namespace Helium
{
    class Optimizer
    {
        public:
            static void optimize(Module& script);
            static void optimizeWithStatistics(Module& script);
    };
}
