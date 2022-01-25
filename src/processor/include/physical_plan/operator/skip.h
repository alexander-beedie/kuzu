#pragma once

#include "src/processor/include/physical_plan/operator/filtering_operator.h"
#include "src/processor/include/physical_plan/operator/physical_operator.h"

namespace graphflow {
namespace processor {

class Skip : public PhysicalOperator, public FilteringOperator {

public:
    Skip(uint64_t skipNumber, shared_ptr<atomic_uint64_t> counter, uint32_t dataChunkToSelectPos,
        unordered_set<uint32_t> dataChunksPosInScope, unique_ptr<PhysicalOperator> child,
        ExecutionContext& context, uint32_t id)
        : PhysicalOperator{move(child), context, id},
          FilteringOperator(), skipNumber{skipNumber}, counter{move(counter)},
          dataChunkToSelectPos{dataChunkToSelectPos}, dataChunksPosInScope{
                                                          move(dataChunksPosInScope)} {}

    PhysicalOperatorType getOperatorType() override { return SKIP; }

    shared_ptr<ResultSet> initResultSet() override;

    bool getNextTuples() override;

    unique_ptr<PhysicalOperator> clone() override {
        return make_unique<Skip>(skipNumber, counter, dataChunkToSelectPos, dataChunksPosInScope,
            children[0]->clone(), context, id);
    }

private:
    uint64_t skipNumber;
    shared_ptr<atomic_uint64_t> counter;
    uint32_t dataChunkToSelectPos;
    unordered_set<uint32_t> dataChunksPosInScope;
};

} // namespace processor
} // namespace graphflow