#ifndef HELIUM_RUNTIME_DEBUG_GCTRACE_HPP
#define HELIUM_RUNTIME_DEBUG_GCTRACE_HPP

#include <Helium/Config.hpp>
#include <Helium/Runtime/VM.hpp>

#include <filesystem>

namespace Helium {

class GcTrace {
public:
    GcTrace(std::filesystem::path const& logFile);
    ~GcTrace();

#if HELIUM_TRACE_GC
    static void beginCollectGarbage(GarbageCollectReason reason, int numInstructionsSinceLastCollect);
    static void endCollectGarbage(int numValuesCollected);
#else
    static void beginCollectGarbage(GarbageCollectReason reason, int numInstructionsSinceLastCollect) {}
    static void endCollectGarbage(int numValuesCollected) {}
#endif
};


}

#endif
