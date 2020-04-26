#ifndef HELIUM_FORMAL_FORMALANALYZER_HPP
#define HELIUM_FORMAL_FORMALANALYZER_HPP

#include <Helium/Memory/LinearAllocator.hpp>

#include <stx/string_view.hpp>

namespace Helium {

class AnalysisContext;
class AstNodeScript;

class FormalDiagnosticReceiver {
public:
    virtual void error(const SourceSpan& span, stx::string_view message) = 0;
};

class FormalAnalyzer {
public:
    explicit FormalAnalyzer(FormalDiagnosticReceiver* diagnosticReceiver);

    void verifyScript(const AstNodeScript* script);

private:
    void verifyBlock(AnalysisContext& parentContext, const AstNodeBlock* block);
    void verifyFunction(const AnalysisContext& parentContext, const AstNodeFunction* function);

    FormalDiagnosticReceiver* diagnosticReceiver;
    LinearAllocator allocator{LinearAllocator::kDefaultBlockSize};
};

}

#endif //HELIUM_FORMAL_FORMALANALYZER_HPP
