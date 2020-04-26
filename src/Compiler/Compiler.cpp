#include <Helium/Compiler/BytecodeCompiler.hpp>
#include <Helium/Compiler/Compiler.hpp>
#include <Helium/Compiler/Lexer.hpp>
#include <Helium/Compiler/Parser.hpp>
#include <Helium/Runtime/Debug/Disassembler.hpp>

#include <fmt/format.h>

#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#ifdef HELIUM_ENABLE_FORMAL
#include <Helium/Formal/FormalAnalyzer.hpp>
#include <iostream>
#endif

namespace Helium
{
    using fmt::format;
    using std::string;

    static std::optional<std::filesystem::path> disassemblyDir;

    bool operator<(SourcePoint a, SourcePoint b) {
        helium_assert(a.unit == b.unit);

        return (a.line < b.line) || (a.line == b.line && a.column < b.column);
    }

    SourceSpan SourceSpan::union_(SourceSpan span1, SourceSpan span2) {
        // TODO: this won't be needed eventually, when P3 generates all spans correctly (e.g. for argList)
        if (span1.start.unit == nullptr)
            return span2;

        if (span2.start.unit == nullptr)
            return span1;

        helium_assert(span1.start.unit == span2.end.unit);
        helium_assert_debug(!(span2.end < span1.start));

        return SourceSpan{span1.start, span2.end};
    }

    std::unique_ptr<Module> Compiler::compileFile(std::filesystem::path const& fileName)
    {
        std::ifstream inFile(fileName);

        if ( !inFile )
        {
            // TODO: propagate OS error
            throw CompileException( "FileOpenError", ( fileName.string() + " is not a readable file" ).c_str() );
        }

        std::stringstream buffer;
        buffer << inFile.rdbuf();
        auto source = buffer.str();

        return compileString( fileName, source );
    }

    std::unique_ptr<Module> Compiler::compileString(std::filesystem::path const& fileName, std::string_view source)
    {
        auto fileNameAsString = fileName.string();

        currentUnitName.push( std::make_shared<string>( fileNameAsString ) );

        LinearAllocator astAllocator(LinearAllocator::kDefaultBlockSize);
        Lexer lexer( this, fileNameAsString.c_str(), source );
        Parser parser( this, &lexer, astAllocator );

        pool_ptr<AstNodeScript> tree = parser.parseScript();

        // Print out the AST
        /*tree->mainFunction->body->print(0);

        for (const auto& function : tree->functions) {
            function->body->print(0);
        }*/

#ifdef HELIUM_ENABLE_FORMAL
        class MyFormalAnalysisReceiver : public FormalDiagnosticReceiver {
        public:
            void error(const SourceSpan& span, stx::string_view message) override {
                std::cout << "Formal Error: " << message << "\n              at line " << span.start.line << ":" << span.start.column << std::endl;
            }
        };

        MyFormalAnalysisReceiver diag;

        FormalAnalyzer fa(&diag);

        fa.verifyScript(tree.get());
#endif

        std::unique_ptr<Module> script = BytecodeCompiler::compile(*tree, true, currentUnitName.top() );

        //printf("[%s] AST memory: %zu bytes (%zu including overhead)\n", unitName, astAllocator.getMemoryUsage(), astAllocator.getMemoryUsageInclOverhead());

        currentUnitName.pop();

        // enableDisassemblingAllCompiledUnits?
        if (script && disassemblyDir) {
            // generate new file name
            std::stringstream fn;

            for (auto const& component : fileName) {
                if (fn.tellp() != std::streampos(0)) {
                    fn << "_";
                }

                // sanitize
                for (char c : component.string()) {
                    if (!strchr("/\\. ", c)) {
                        fn << c;
                    }
                    else {
                        fn << '_';
                    }
                }
            }

            if (fn.tellp() != std::streampos(0)) {
                // FIXME
            }

            // disassemble into specificed directory
            auto disassemblyPath = *disassemblyDir / fn.str();
            std::ofstream f(disassemblyPath);

            f << format("; disassembling {}", fileNameAsString) << "\n" << "\n";

            Disassembler::disassemble(*script, [&f](std::string_view line) {
                f << line << std::endl;
            });
        }

        return script;
    }

    void Compiler::enableDisassemblingAllCompiledUnits(std::filesystem::path const& directory) {
        std::filesystem::create_directories(directory);
        disassemblyDir = directory;
    }

    /*void AstNodeExpression::print( int indent )
    {
        // TODO: this is wrong - should be virtualized

        for ( int i = 0; i < indent; i++ )
            printf( ". " );

        printf( "[%i] ", type );

        if (type == AstNodeType::identifier)
            printf( "'%s' ", static_cast<AstNodeIdent*>(this)->name.c_str());
        else if (type == AstNodeType::literalBoolean)
            printf( "%s ", static_cast<AstNodeLiteralBoolean*>(this)->value ? "true" : "false");
        else if (type == AstNodeType::literalInteger)
            printf( "%" PRIi64 " ", static_cast<int64_t>(static_cast<AstNodeLiteralInteger*>(this)->value));
        else if (type == AstNodeType::literalReal)
            printf( "%g ", static_cast<AstNodeLiteralReal*>(this)->value);
        else if (type == AstNodeType::literalString)
            printf( "'%s' ", static_cast<AstNodeLiteralString*>(this)->text.c_str());

        printf("\n");
    }*/

    SourceSpan AstNodeBlock::getFullSpan() const {
        if (statements.empty()) {
            return span;
        }
        else {
            auto fullSpan = statements[0]->getFullSpan();

            for (auto& statement : statements) {
                fullSpan = SourceSpan::union_(fullSpan, statement->getFullSpan());
            }

            return fullSpan;
        }
    }

    SourceSpan AstNodeList::getFullSpan() const {
        // FIXME: this doesn't cover the ')'
        SourceSpan fullSpan = span;

        for (auto& item : items) {
            fullSpan = SourceSpan::union_(fullSpan, item->getFullSpan());
        }

        return fullSpan;
    }
}
