#pragma once

#include <Helium/Runtime/Code.hpp>

namespace Helium
{
    class Optimizer
    {
        public:
            static void optimize( Script* script );
            static void optimizeWithStatistics( Script* script );
    };
}
