#pragma once

#include <Helium/Runtime/Value.hpp>

#include <filesystem>

namespace Helium
{
    class ActivationContext;
    struct Frame;

    class ValueTrace {
    public:
        ValueTrace(std::filesystem::path const& logFilePath);
        ~ValueTrace();

        void raiseFatalErrors();
        void report();

        static void trackCreated(Value var);
        static void trackDestroyed(Value var);
        static void trackReferenced(Value var, VarId_t newRefId);
        static void trackReleased(Value var);
    };

    struct ValueTraceCtx {
        ValueTraceCtx(const char* name);
        ValueTraceCtx(const char* name, Frame* frame);
        ValueTraceCtx(const char* name, ActivationContext& ctx);
        ~ValueTraceCtx();

        ValueTraceCtx* prev;
        const char* name;
        Frame* frame = nullptr;
        ActivationContext* ctx = nullptr;
    };
}
