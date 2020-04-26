#include <Helium/Runtime/Debug/GcTrace.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <fstream>

namespace Helium {

using fmt::format;

namespace {

std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

std::ofstream logfile;

void printTimestamp(std::ostream& o, std::time_t t) {
    o << format("{:%F %T%z}", *std::gmtime(&t));
}

}

GcTrace::GcTrace(std::filesystem::path const& logFile) {
    logfile.open(logFile);

    logfile << "Started on ";
    printTimestamp(logfile, std::time(nullptr));
    logfile << ".\n\n";
}

GcTrace::~GcTrace() {
    logfile.close();
}

void GcTrace::beginCollectGarbage(GarbageCollectReason reason, int numInstructionsSinceLastCollect) {
    logfile << "[";
    printTimestamp(logfile, std::time(nullptr));
    logfile << format("] GC_TRACE: start; reason={} numInstructionsSinceLastCollect={} numExistingValues={}\n",
            to_string(reason), numInstructionsSinceLastCollect, Value::getNumExistingValues());

    // Do not include our own overhead
    startTime = std::chrono::high_resolution_clock::now();
}

void GcTrace::endCollectGarbage(int numValuesCollected) {
    auto end = std::chrono::high_resolution_clock::now();

    logfile << "[";
    printTimestamp(logfile, std::time(nullptr));
    logfile << format("] GC_TRACE: {} objects released; time={} numExistingValues={}\n\n",
            numValuesCollected, end - startTime, Value::getNumExistingValues());
}

}
