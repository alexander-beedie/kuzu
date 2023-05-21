#include "processor/operator/recursive_extend/shortest_path_recursive_join.h"

namespace kuzu {
namespace processor {

void ShortestPathRecursiveJoin::initLocalStateInternal(
    ResultSet* resultSet_, ExecutionContext* context) {
    BaseRecursiveJoin::initLocalStateInternal(resultSet_, context);
    auto maxNodeOffset = nodeTable->getMaxNodeOffset(transaction);
    std::vector<std::unique_ptr<BaseFrontierScanner>> scanners;
    if (dataInfo->trackPath) {
        bfsMorsel = std::make_unique<ShortestPathMorsel<true /* trackPath */>>(
            maxNodeOffset, lowerBound, upperBound, sharedState->semiMask.get());
        for (auto i = lowerBound; i <= upperBound; ++i) {
            scanners.push_back(std::make_unique<PathScanner>(bfsMorsel->targetDstNodeOffsets, i));
        }
    } else {
        bfsMorsel = std::make_unique<ShortestPathMorsel<false /* trackPath */>>(
            maxNodeOffset, lowerBound, upperBound, sharedState->semiMask.get());
        for (auto i = lowerBound; i <= upperBound; ++i) {
            scanners.push_back(
                std::make_unique<DstNodeScanner>(bfsMorsel->targetDstNodeOffsets, i));
        }
    }
    frontiersScanner = std::make_unique<FrontiersScanner>(std::move(scanners));
}

} // namespace processor
} // namespace kuzu