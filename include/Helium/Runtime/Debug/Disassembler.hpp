#pragma once

#include <Helium/Runtime/Code.hpp>

#include <functional>

namespace Helium {

using LineSink = std::function<void(std::string_view)>;

class Disassembler {
public:
    static void disassemble(Script const& script, LineSink const& output);
};

}
