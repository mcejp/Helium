#pragma once

#include <Helium/Runtime/Value.hpp>

namespace Helium
{
    // For the time being:
    //  - metaclasses can only be instantiated in native code, with manual memory management
    //  - callbacks must be native functions
    //
    // To relax either of these limitations, metaclass must become a GC-able object.
    struct Metaclass {
        NativeFunction getProperty;
    };
}
