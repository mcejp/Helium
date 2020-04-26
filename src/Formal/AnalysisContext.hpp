#ifndef HELIUM_FORMAL_ANALYSISCONTEXT_HPP
#define HELIUM_FORMAL_ANALYSISCONTEXT_HPP

#include <Helium/Formal/Theorem.hpp>
#include <Helium/Memory/LinearAllocator.hpp>

#include <set>
#include <vector>

namespace Helium {

struct Assumption {
    pool_ptr<Theorem> theorem;
    SemanticPoint validFrom;
    stx::optional<SemanticPoint> validBefore;

    bool isValidAt(SemanticPoint& point) const {
        return point.index >= validFrom.index && (!validBefore || point.index < validBefore->index);
    }
};

// needed to enable std::set<Assumption>
bool operator<(const Assumption& x, const Assumption& y) {
    return x.theorem < y.theorem;
}

class AnalysisContext {
public:
    AnalysisContext();
    explicit AnalysisContext(const AnalysisContext& parentContext);

    void assumeAt(SemanticPoint point, pool_ptr<Theorem>&& assumption, const char* ruleName);
    std::vector<Theorem*> assumptionsValidAt(SemanticPoint point);

    //auto& getAssumptions() { return assumptions; }

    auto getId() const { return id; }
    auto& getTheoremAllocator() { return theoremAllocator; }

    void invalidateAssumption(Theorem* assumption);

    SemanticPoint currentPoint() const { return SemanticPoint{nextPointIndex}; }
    SemanticPoint nextPoint() { return SemanticPoint{++nextPointIndex}; }

private:
    const AnalysisContext* parentContext = nullptr;
    const size_t id;

    std::set<Assumption> assumptions;

    LinearAllocator theoremAllocator {LinearAllocator::kDefaultBlockSize};
    size_t nextPointIndex = 0;
};

}

#endif
