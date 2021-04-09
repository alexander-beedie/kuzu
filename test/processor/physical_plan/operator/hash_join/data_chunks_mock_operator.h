#pragma once

#include "src/processor/include/physical_plan/operator/physical_operator.h"
#include "src/processor/include/physical_plan/operator/tuple/data_chunks.h"

using namespace graphflow::processor;

namespace graphflow {
namespace processor {

class DataChunksMockOperator : public PhysicalOperator {
public:
    DataChunksMockOperator(MorselDesc& morsel) : PhysicalOperator(SCAN), morsel(morsel) {
        dataChunks = make_shared<DataChunks>();
    }
    explicit DataChunksMockOperator(shared_ptr<DataChunks> mockDataChunks, MorselDesc& morsel)
        : PhysicalOperator(SCAN), morsel(morsel) {
        dataChunks = move(mockDataChunks);
    }

    bool hasNextMorsel() override { return true; };
    void getNextTuples() override {}
    unique_ptr<PhysicalOperator> clone() override {
        return make_unique<DataChunksMockOperator>(dataChunks, morsel);
    }

    MorselDesc& morsel;
    mutex opLock;
};

class ScanMockOp : public DataChunksMockOperator {
public:
    ScanMockOp(MorselDesc& morsel) : DataChunksMockOperator(morsel) { generateDataChunks(); }

    void getNextTuples() override;
    unique_ptr<PhysicalOperator> clone() override { return make_unique<ScanMockOp>(morsel); }

private:
    void generateDataChunks();
};

class ProbeScanMockOp : public DataChunksMockOperator {
public:
    ProbeScanMockOp(MorselDesc& morsel) : DataChunksMockOperator(morsel) { generateDataChunks(); }

    void getNextTuples() override;
    unique_ptr<PhysicalOperator> clone() override { return make_unique<ProbeScanMockOp>(morsel); }

private:
    void generateDataChunks();
};

class ScanMockOpWithSelector : public DataChunksMockOperator {
public:
    ScanMockOpWithSelector(MorselDesc& morsel) : DataChunksMockOperator(morsel) {
        generateDataChunks();
    }

    void getNextTuples() override;
    unique_ptr<PhysicalOperator> clone() override {
        return make_unique<ScanMockOpWithSelector>(morsel);
    }

private:
    void generateDataChunks();
};

class ProbeScanMockOpWithSelector : public DataChunksMockOperator {
public:
    ProbeScanMockOpWithSelector(MorselDesc& morsel) : DataChunksMockOperator(morsel) {
        generateDataChunks();
    }

    void getNextTuples() override;
    unique_ptr<PhysicalOperator> clone() override {
        return make_unique<ProbeScanMockOpWithSelector>(morsel);
    }

private:
    void generateDataChunks();
};

} // namespace processor
} // namespace graphflow