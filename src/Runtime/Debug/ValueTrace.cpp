#include <Helium/Assert.hpp>
#include <Helium/Runtime/ActivationContext.hpp>
#include <Helium/Runtime/Code.hpp>
#include <Helium/Runtime/Debug/ValueTrace.hpp>

#include <fmt/format.h>

#include <fstream>
#include <iostream>
#include <unordered_map>

// TODO: it might be useful to give an option to exclude primitive types from the tracking

namespace Helium {

using fmt::format;

enum class ValueState {
    VALID,
    DESTROYED,
};

struct ValueInfo {
    ValueState state;
    ValueType type;
};

namespace {
    // FIXME: make thread-local
    ValueTraceCtx *s_current = nullptr;
    std::unordered_map<VarId_t, ValueInfo> s_values;
    int s_numFatalErrors = 0;

    std::fstream logfile;

    void reportContext(ValueTraceCtx *context) {
        for (; context; context = context->prev) {
            logfile << format("\tin {}\n", context->name);

            if (context->ctx != nullptr) {
                logfile << format("\t\t[ActivationContext {}] ", static_cast<void*>(context->ctx));

                auto instr = context->ctx->getLastExecutedInstruction();

                if (instr) {
                    auto origin = instr->origin;
                    auto id = InstructionDesc::getByOpcode(instr->opcode);
                    logfile << format("instruction {} in `{}`\t({}:{})", id->name, origin->function->c_str(), origin->unit->c_str(), origin->line);
                }
                else {
                    logfile << "unknown instruction";
                }

                logfile << "\n";
            }

            if (context->frame) {
                auto& frame = *context->frame;

                logfile << format("\t\t[Frame {}] ", static_cast<void*>(context->frame));

                helium_assert(frame.scriptFunction != nullptr);
                logfile << format("function `{}`", frame.scriptFunction->name) << "\n";
            }
        }
    }

    void reportDoubleDestroy(VarId_t varId) {
        auto const& vi = s_values[varId];

        logfile << format("{:14} #{:<4} ({})", "DOUBLE-DESTROY", varId, to_string(vi.type)) << "\n";
        // TODO: print where allocated
        // TODO: print where destroyed previously
        // TODO: print where destroyed
    }

    void reportLeak(VarId_t varId) {
        auto const& vi = s_values[varId];

        logfile << format("{:14} #{:<4} ({})", "LEAK", varId, to_string(vi.type)) << "\n";
        // TODO: print where allocated
    }

    void track(const char *operation, Value var, ValueTraceCtx *context) {
        helium_assert(context != nullptr);

        logfile << format("{:14} #{:<4} ({})", operation, var.varId, to_string(var.type));

        if (var.type == ValueType::list || var.type == ValueType::object) {
            logfile << format(" (nref={}, ref#={})", var.gc->numReferences, var.refId);
        }
        else if (var.type == ValueType::string) {
            logfile << format(" '{}'", var.string->text);
        }

        logfile << "\n";

        reportContext(context);
    }
}

ValueTrace::ValueTrace(std::filesystem::path const& logFilePath) {
    logfile.open(logFilePath, std::ios::out);
}

ValueTrace::~ValueTrace() {
    logfile.close();
}

void ValueTrace::raiseFatalErrors() {
    if (s_numFatalErrors > 0) {
        std::cerr << format("VALUE_TRACE: {} fatal errors occured", s_numFatalErrors) << "\n";
        abort();
    }
}

void ValueTrace::report() {
    for (auto const& [id, vi] : s_values) {
        if (vi.state == ValueState::VALID) {
            reportLeak(id);
        }
    }
}

void ValueTrace::trackCreated(Value var) {
    track("NEW", var, s_current);

    helium_assert_debug(s_values.count(var.varId) == 0);

    s_values.emplace(var.varId, ValueInfo { ValueState::VALID, var.type });
}

void ValueTrace::trackDestroyed(Value var) {
    track("DESTROY", var, s_current);

    helium_assert_debug(s_values.count(var.varId) == 1);

    auto& vi = s_values[var.varId];

    if (vi.state != ValueState::VALID) {
        reportDoubleDestroy(var.varId);
        s_numFatalErrors++;
    }

    vi.state = ValueState::DESTROYED;
}

void ValueTrace::trackReferenced(Value var, VarId_t newRefId) {
    var.refId = newRefId;
    track("ADD_REF", var, s_current);
}

void ValueTrace::trackReleased(Value var) {
    track("RELEASE", var, s_current);
}

ValueTraceCtx::ValueTraceCtx(const char* name)
        : prev(s_current), name(name) {
    s_current = this;
}

ValueTraceCtx::ValueTraceCtx(const char* name, Frame* frame)
        : prev(s_current), name(name), frame(frame) {
    s_current = this;
}

ValueTraceCtx::ValueTraceCtx(const char* name, ActivationContext& ctx)
        : prev(s_current), name(name), ctx(&ctx) {
    s_current = this;
}

ValueTraceCtx::~ValueTraceCtx() {
    s_current = this->prev;
}

}
