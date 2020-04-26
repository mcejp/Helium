#pragma once

#include <Helium/Runtime/ActivationContext.hpp>
#include <Helium/Runtime/Code.hpp>
#include <Helium/Runtime/Value.hpp>

#include <optional>
#include <stack>
#include <vector>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define win32_gc_profiling
#endif

namespace Helium
{
    class ActivationContext;

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
            std::vector<Value> possibleGarbage;

            // Primitive variable methods
            //HashMap<StringWrapper, NativeFunction> stringFunctions;

#ifdef win32_gc_profiling
            LARGE_INTEGER performanceFrequency;
#endif

            unsigned desiredGcRuntime, gcThreshold;

        protected:
            void clearStacks();
            //void execute();
            bool ret();

            /*inline Variable& getLocal( unsigned index )
            {
                return locals[frameBottom.top() + index];
            }*/

            void invoke( Value me, unsigned target );
            void invoke( Value object, const char* methodName, InstructionOrigin* origin );

            /*inline void setLocal( unsigned index, Variable value )
            {
                frameTop = maximum( index + 1, frameTop );
                locals[frameBottom.top() + index].release();
                locals[frameBottom.top() + index] = value;
            }*/

        public:
            ValueRef global;

        public:
            VM();
            ~VM();

            //void addCode( size_t module, Instruction** code, size_t length );

            void addPossibleGarbage( Value var ) { possibleGarbage.push_back( var ); }

            void collectGarbage( bool final );

            /*inline unsigned getCodeLength() const
            {
                return numInstructions;
            }*/

            Module* getModuleByIndex(ModuleIndex_t moduleIndex) { return loadedModules[moduleIndex].get(); }
            ModuleIndex_t loadModule( Script* script );

            /*unsigned readNumParameters()
            {
                return callStack.pop();
            }*/

            int16_t registerCallback( const char* name, NativeFunction callback );
            //Variable invoke( Variable methocator, List<Variable>& arguments, bool blindCall, bool keepArguments );

            //ActivationContext::State run( size_t entry, Variable* result_out );
            void execute( ActivationContext& ctx );
    };
}
