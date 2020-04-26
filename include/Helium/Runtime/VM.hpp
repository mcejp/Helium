#pragma once

#include <Helium/Runtime/ActivationContext.hpp>
#include <Helium/Runtime/Code.hpp>
#include <Helium/Runtime/Value.hpp>

#include <optional>
#include <stack>
#include <vector>

namespace Helium
{
    class ActivationContext;

    enum class GarbageCollectReason {
        numInstructionsSinceLastCollect,
        numPossibleRoots,
        vmShutdown,
    };

    /**
     * A module exists only within a VM.
     */
    struct Module
    {
        std::vector<ScriptFunction> functions;
        std::vector<Instruction> instructions;

        std::vector<uint8_t> stringMemory;
        VMString* strings;

        std::vector<std::shared_ptr<SwitchTable>> switchTables;

        std::optional<FunctionIndex_t> findMainFunction();
    };

    class VM
    {
        protected:
            struct ExternalFunc
            {
                std::string name;
                NativeFunction callback;
            };

            // Code and execution
            std::vector<std::unique_ptr<Module>> loadedModules;
            std::vector<ExternalFunc> externals;

            // Garbage collection
            // Possible roots of cycles (_purple_ in the paper's terminology)
            std::vector<Value> possibleRoots;
            int numInstructionsSinceLastCollect = 0;

            // Primitive variable methods
            //HashMap<StringWrapper, NativeFunction> stringFunctions;

        public:
            ValueRef global;

        public:
            VM();
            ~VM();

            void addPossibleRootOfCycle(Value var ) { possibleRoots.push_back(var ); }
            void collectGarbage( GarbageCollectReason reason );

            Module* getModuleByIndex(ModuleIndex_t moduleIndex) { return loadedModules[moduleIndex].get(); }
            ModuleIndex_t loadModule( Script* script );

            int16_t registerCallback( const char* name, NativeFunction callback );

            void execute( ActivationContext& ctx );
    };

    std::string_view to_string(GarbageCollectReason);
}
