#pragma once

#include <Helium/Assert.hpp>

#include <filesystem>
#include <memory>
#include <stack>
#include <string>

namespace Helium
{
    struct Module;

    struct SourcePoint {
        const char* unit;           // non-owning reference; pooled in Helium::Compiler instance
        // TODO: consider std::string_view once widely available
        unsigned int line;
        unsigned int column;
        size_t byteOffset;
    };

    bool operator<(SourcePoint a, SourcePoint b);

    struct SourceSpan {
        // Invariant: start <= end
        // Invariant: start.unit == end.unit

        SourcePoint start, end;

        explicit SourceSpan()
                : start{}, end{}
        {}

        // Precondition: start <= end
        // NOTE: end is INCLUSIVE
        // TODO: what if start.unit != end.unit
        explicit SourceSpan(SourcePoint start, SourcePoint end)
                : start{start}, end{end} {
            helium_assert_debug(!(end < start));
        }

        // Precondition: span1.start <= span2.end
        static SourceSpan union_(SourceSpan span1, SourceSpan span2);
    };

    // TODO: don't instantiate this in parse rules directly, maybe remove altogether
    struct CompileException
    {
        std::string name, desc;

        CompileException( const char* name, const char* desc )
                : name( name ), desc( desc )
        {
        }
    };

    class Compiler
    {
        std::stack<std::shared_ptr<std::string>> currentUnitName;

        public:
            enum { version = 1000 };

            std::unique_ptr<Module> compileFile(std::filesystem::path const& fileName);
            std::unique_ptr<Module> compileString(std::filesystem::path const& fileName, std::string_view script);

            // TODO: nuke
            std::string getCurrentUnit() const { return *currentUnitName.top().get(); }

            static void enableDisassemblingAllCompiledUnits(std::filesystem::path const& directory);
    };
}
