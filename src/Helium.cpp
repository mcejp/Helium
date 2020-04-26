#include <Helium/Config.hpp>
#include <Helium/Compiler/Compiler.hpp>
#include <Helium/Compiler/Optimizer.hpp>
#include <Helium/Runtime/Debug/Disassembler.hpp>
#include <Helium/Runtime/RuntimeFunctions.hpp>
#include <Helium/Runtime/VM.hpp>

#if HELIUM_TRACE_VALUES
#include <Helium/Runtime/Debug/ValueTrace.hpp>
#endif

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

namespace Helium
{
    namespace fs = std::filesystem;
    using std::string;

    // eugh
    void registerBuiltinFunctions(VM* vm);
    void registerSDL2(VM* vm);
    void registerSDL2_ttf(VM* vm);

    static int runProgram( int argc, char** argv )
    {
        string output, dasmOutput, program;
        bool printVersion = false;
        bool optimize, disassemble, run, silent;

        std::vector<fs::path> modulePaths;
        modulePaths.emplace_back( "." );

#ifdef WIN32
        WCHAR buffer[FILENAME_MAX];
        GetModuleFileNameW( nullptr, buffer, std::size( buffer ) );

        modulePaths.push_back( fs::path(std::wstring(buffer)).parent_path() );
#endif

        optimize = true;
        disassemble = false;
        run = true;
        silent = false;

        // Read the commandline arguments
        std::vector<std::string> argList;

        //
        bool parseArguments = true;

        for ( int i = 1; i < argc; i++ )
        {
            if ( !parseArguments )
            {
                argList.emplace_back(argv[i]);
                continue;
            }

            if ( strcmp( argv[i], "--" ) == 0 )
                parseArguments = false;
            else if ( strcmp( argv[i], "-c" ) == 0 )
                run = false;
            else if ( strncmp( argv[i], "-d", 2 ) == 0 )
            {
                disassemble = true;
                run = false;

                dasmOutput = argv[i] + 2;
            }
            else if ( strncmp( argv[i], "-I", 2 ) == 0 )
                modulePaths.emplace_back( argv[i] + 2 );
            else if ( strncmp( argv[i], "-o", 2 ) == 0 )
                output = argv[i] + 2;
            else if ( strncmp( argv[i], "-O", 2 ) == 0 )
                optimize = std::stoi( argv[i] + 2 ) != 0;
            else if ( strcmp( argv[i], "-s" ) == 0 )
                silent = true;
            else if ( strcmp( argv[i], "-V" ) == 0 )
                printVersion = true;
            else if ( program.empty() )
                program = argv[i];
            else
                argList.emplace_back(argv[i]);
        }

        if ( program.empty() )
            printVersion = true;

        if ( printVersion )
            printf( "Helium HEAD\n  Copyright (c) 2008-2020\n\n" );

        std::unique_ptr<Helium::VM> vm;
        std::unique_ptr<Helium::Script> script;

        try
        {
            vm = std::make_unique<Helium::VM>();

            if ( program.empty() )
            {
                printf( "Helium: no input given.\n" );

                /*AutoVariable info = Variable::newObject( vm.get() );
                info.setMember( "platform", Variable::newString( helium_platform_name, -1 ) );
                info.setMember( "sizeof( Variable )", Variable::newInteger( sizeof( Variable ) ) );
                info.setMember( "version", Variable::newInteger( 1 ) );
                info.print();*/

                return 1;
            }
        }
        catch ( Helium::Value ex )
        {
            if ( !silent )
            {
                puts( " -- An exception occured:" );
                ex.print();
            }

            ex.release();
            return 1;
        }

        try
        {
            Helium::Compiler compiler;

            if ( !script )
            //* If the program is not binary (previous load failed)
                script = compiler.compileFile( program.c_str() );
        }
        catch ( CompileException const& ex )
        {
            if ( !silent )
                printf( "ERROR\t: Compilation failed:\n    %s - %s\n", ex.name.c_str(), ex.desc.c_str() );

            return 1;
        }

        Helium::registerBuiltinFunctions(vm.get());

#ifdef HELIUM_WITH_SDL2
        Helium::registerSDL2(vm.get());
#endif

#ifdef HELIUM_WITH_SDL2_TTF
        Helium::registerSDL2_ttf(vm.get());
#endif

        if ( optimize )
        {
#ifndef helium_no_optimizer
            if ( silent )
                Helium::Optimizer::optimize( script.get() );
            else
                Helium::Optimizer::optimizeWithStatistics( script.get() );
#endif
        }

        if ( disassemble )
        {
            std::ofstream dasmFile;

            if ( !dasmOutput.empty() )
                dasmFile.open(dasmOutput.c_str());

            std::ostream& os = !dasmOutput.empty() ? dasmFile : std::cout;

            auto sink = [&os](std::string_view line) {
                os << line << std::endl;
            };

            if ( !silent )
            {
                sink( "; disassembling '" + program + "'" );
                sink("");
            }

            Disassembler::disassemble(*script, sink);
        }

        if ( run )
        {
            auto moduleIndex = vm->loadModule( script.get() );

            Value vmArgList = Helium::Value::newList( vm.get(), argList.size() );
            helium_assert(vmArgList.type == ValueType::list);

            for (const auto& arg : argList)
                vmArgList.listAddItem( Helium::Value::newStringWithLength( arg.c_str(), arg.size() ) );

            vm->global->objectSetProperty(VMString::fromCString("args"), vmArgList, true);

            ActivationContext ctx(vm.get());
            ActivationScope scope(ctx);

            if (ctx.callMainFunction(moduleIndex)) {
                // This is where the fun begins
                vm->execute(ctx);
            }

            if (ctx.getState() == ActivationContext::returnedValue) {
                //result_out = ctx.stack.pop();
            }
            else if (ctx.getState() == ActivationContext::raisedException) {
                Value ex = ctx.getException();

                ValueRef exitCode;
                if (RuntimeFunctions::getProperty(ex, VMString::fromCString("exitCode"), &exitCode, false)) {
                    Int_t i;

                    // FIXME: if this raises another exception, what will happen to the original one?
                    if (RuntimeFunctions::asInteger(exitCode, &i)) {
                        return (int) i;
                    }
                }

                ex.print();
            }
        }

        return 0;
    }
}

int main( int argc, char *argv[] )
{
#ifdef _DEBUG
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF );
    //_CrtSetBreakAlloc();
#endif

#if HELIUM_TRACE_VALUES
    Helium::ValueTrace vt(".helium_value_trace");
    Helium::ValueTraceCtx vtContext("main");
#endif

#ifdef HELIUM_DEBUG
    Helium::Compiler::enableDisassemblingAllCompiledUnits(".helium_disassembly");
#endif

    int ec = Helium::runProgram( argc, argv );

    Helium::Value::printStatistics();

#ifdef HELIUM_TRACE_VALUES
    vt.report();
#endif

    return ec;
}

void helium_assert_fail(const char* expression, const char* file, int line) {
    fprintf(stderr, "helium_assert_fail: %s\n\tfile %s, line %d\n\n", expression, file, line);
    abort();
}
