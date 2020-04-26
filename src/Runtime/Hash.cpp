#include <Helium/Runtime/Hash.hpp>

#include <xxhash.h>

namespace Helium
{
    Hash_t Hash::fromString(const char *str, size_t length)
    {
        return XXH32(str, length, 0);
    }
}