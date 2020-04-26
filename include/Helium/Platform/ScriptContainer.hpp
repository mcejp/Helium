#pragma once

#include <Helium/Compiler/Compiler.hpp>

#include <Helium/Runtime/VM.hpp>

namespace Helium
{
    class ScriptContainer {
        public:
            ScriptContainer(const char* scriptName, const char* scriptSource);
            void run();

        private:
            Compiler compiler;

            VM vm;
            ModuleIndex_t moduleIndex;
            ActivationContext ctx;
    };
}

