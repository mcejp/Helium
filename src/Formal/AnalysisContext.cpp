#include "AnalysisContext.hpp"

#include <Helium/Formal/TheoremOstream.hpp>

#include <iostream>

namespace Helium {

using std::cout;

static size_t globalCounter = 0;

AnalysisContext::AnalysisContext(const AnalysisContext& parentContext) : parentContext(&parentContext),
                                                                         id(globalCounter++),
                                                                         assumptions(parentContext.assumptions) {
}

void AnalysisContext::assumeAt(SemanticPoint point, pool_ptr<Theorem>&& assumption, const char* ruleName) {
    cout << "Context " << id << ": Assume [" << *assumption << "] at <Spoint " << point.index << "> via rule " << ruleName << std::endl;

    assumptions.emplace(Assumption {std::move(assumption), point, {}});
}

std::vector<Theorem*> AnalysisContext::assumptionsValidAt(SemanticPoint point) {
    std::vector<Theorem*> list;

    for (auto& assum : assumptions) {
        if (assum.isValidAt(point))
            list.push_back(assum.theorem.get());
    }

    return list;
}

}
