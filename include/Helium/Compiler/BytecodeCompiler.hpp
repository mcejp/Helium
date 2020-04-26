#pragma once

#include <Helium/Runtime/Code.hpp>

#include <memory>
#include <string>

namespace Helium
{
    class AstNodeScript;
    struct Branch;

    class BytecodeCompiler
    {
        public:
            static std::unique_ptr<Module> compile(AstNodeScript& tree, bool withDebugInformation = true, std::shared_ptr<std::string> unitNameString = 0 );
    };
}
