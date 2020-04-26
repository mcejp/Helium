#pragma once

#include <cstddef>
#include <cstdint>

namespace Helium
{
    typedef uint32_t Hash_t;

    class Hash {
        public:
            static Hash_t fromString(const char *str, size_t length);
    };
}
