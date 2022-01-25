#include <numeric>
#include <variant>
#include <vector>

#include "gtest/gtest.h"

#include "src/common/include/assert.h"
#include "src/common/include/configs.h"
#include "src/common/include/data_chunk/data_chunk.h"
#include "src/common/include/date.h"
#include "src/common/include/value.h"
#include "src/processor/include/physical_plan/operator/order_by/key_block_merger.h"
#include "src/processor/include/physical_plan/operator/order_by/order_by_key_encoder.h"

using ::testing::Test;
using namespace graphflow::processor;
using namespace std;

class KeyBlockMergerTest : public Test {

public:
    void SetUp() override { memoryManager = make_unique<MemoryManager>(); }

public:
    unique_ptr<MemoryManager> memoryManager;

    void checkTupleIdxesAndFactorizedTableIdxes(uint8_t* keyBlockPtr,
        const uint64_t keyBlockEntrySizeInBytes, const vector<uint64_t>& expectedTupleIdxOrder,
        const vector<uint64_t>& expectedFactorizedTableIdxOrder) {
        assert(expectedTupleIdxOrder.size() == expectedFactorizedTableIdxOrder.size());
        for (auto i = 0u; i < expectedTupleIdxOrder.size(); i++) {
            auto encodedTupleIdx = OrderByKeyEncoder::getEncodedTupleIdx(
                keyBlockPtr + keyBlockEntrySizeInBytes - sizeof(uint64_t));
            ASSERT_EQ(encodedTupleIdx, expectedTupleIdxOrder[i]);
            ASSERT_EQ(OrderByKeyEncoder::getEncodedFactorizedTableIdx(
                          keyBlockPtr + keyBlockEntrySizeInBytes - sizeof(uint64_t)),
                expectedFactorizedTableIdxOrder[i]);
            keyBlockPtr += keyBlockEntrySizeInBytes;
        }
    }

    template<typename T>
    OrderByKeyEncoder prepareSingleOrderByColEncoder(const vector<T>& sortingData,
        const vector<bool>& nullMasks, DataType dataType, bool isAsc, uint16_t factorizedTableIdx,
        bool hasPayLoadCol, vector<shared_ptr<FactorizedTable>>& factorizedTables,
        shared_ptr<DataChunk>& dataChunk) {
        GF_ASSERT(sortingData.size() == nullMasks.size());
        dataChunk->state->selectedSize = sortingData.size();
        auto valueVector = make_shared<ValueVector>(memoryManager.get(), dataType);
        auto values = (T*)valueVector->values;
        for (auto i = 0u; i < dataChunk->state->selectedSize; i++) {
            if (nullMasks[i]) {
                valueVector->setNull(i, true);
            } else if constexpr (is_same<T, string>::value) {
                valueVector->addString(i, sortingData[i]);
            } else {
                values[i] = sortingData[i];
            }
        }
        dataChunk->insert(0, valueVector);

        vector<shared_ptr<ValueVector>> orderByVectors{
            valueVector}; // only contains orderBy columns
        vector<shared_ptr<ValueVector>> allVectors{
            valueVector}; // all columns including orderBy and payload columns

        TupleSchema tupleSchema;
        tupleSchema.appendField({dataType, false /* isUnflat */, 0 /* dataChunkPos */});

        if (hasPayLoadCol) {
            auto payloadValueVector = make_shared<ValueVector>(memoryManager.get(), STRING);
            for (auto i = 0u; i < dataChunk->state->selectedSize; i++) {
                payloadValueVector->addString(i, to_string(i));
            }
            dataChunk->insert(1, payloadValueVector);
            // To test whether the orderByCol -> factorizedTableColIdx works properly, we put the
            // payload column at index 0, and the orderByCol at index 1.
            allVectors.insert(allVectors.begin(), payloadValueVector);
            tupleSchema.appendField({dataType, false, 0 /* dataChunkPos */});
        }
        tupleSchema.initialize();

        auto factorizedTable = make_unique<FactorizedTable>(*memoryManager, tupleSchema);
        factorizedTable->append(allVectors, sortingData.size());

        vector<bool> isAscOrder = {isAsc};
        auto orderByKeyEncoder =
            OrderByKeyEncoder(orderByVectors, isAscOrder, *memoryManager, factorizedTableIdx);
        orderByKeyEncoder.encodeKeys();

        factorizedTables.emplace_back(move(factorizedTable));
        return orderByKeyEncoder;
    }

    template<typename T>
    void singleOrderByColMergeTest(const vector<T>& leftSortingData,
        const vector<bool>& leftNullMasks, const vector<T>& rightSortingData,
        const vector<bool>& rightNullMasks, const vector<uint64_t>& expectedTupleIdxOrder,
        const vector<uint64_t>& expectedFactorizedTableIdxOrder, const DataType dataType,
        const bool isAsc, bool hasPayLoadCol) {
        vector<shared_ptr<FactorizedTable>> factorizedTables;
        auto dataChunk0 = make_shared<DataChunk>(hasPayLoadCol ? 2 : 1);
        auto dataChunk1 = make_shared<DataChunk>(hasPayLoadCol ? 2 : 1);
        auto orderByKeyEncoder1 =
            prepareSingleOrderByColEncoder(leftSortingData, leftNullMasks, dataType, isAsc,
                0 /* factorizedTableIdx */, hasPayLoadCol, factorizedTables, dataChunk0);
        auto orderByKeyEncoder2 =
            prepareSingleOrderByColEncoder(rightSortingData, rightNullMasks, dataType, isAsc,
                1 /* factorizedTableIdx */, hasPayLoadCol, factorizedTables, dataChunk1);

        vector<StringAndUnstructuredKeyColInfo> stringAndUnstructuredKeyColInfo;
        if (hasPayLoadCol) {
            stringAndUnstructuredKeyColInfo.emplace_back(
                StringAndUnstructuredKeyColInfo(1 /* colIdxInFactorizedTable */,
                    0 /* colOffsetInEncodedKeyBlock */, isAsc, true /* isStrCol */));
        } else if constexpr (is_same<T, string>::value || is_same<T, Value>::value) {
            stringAndUnstructuredKeyColInfo.emplace_back(StringAndUnstructuredKeyColInfo(
                0 /* colIdxInFactorizedTable */, 0 /* colOffsetInEncodedKeyBlock */, isAsc,
                is_same<T, string>::value /* isStrCol */));
        }

        KeyBlockMerger keyBlockMerger = KeyBlockMerger(factorizedTables,
            stringAndUnstructuredKeyColInfo, orderByKeyEncoder1.getKeyBlockEntrySizeInBytes());

        auto resultKeyBlock =
            make_shared<KeyBlock>(memoryManager->allocateBlock(DEFAULT_MEMORY_BLOCK_SIZE * 2));
        resultKeyBlock->numEntriesInMemBlock = leftSortingData.size() + rightSortingData.size();
        auto keyBlockMergeTask =
            make_shared<KeyBlockMergeTask>(orderByKeyEncoder1.getKeyBlocks()[0],
                orderByKeyEncoder2.getKeyBlocks()[0], resultKeyBlock, keyBlockMerger);
        KeyBlockMergeMorsel keyBlockMergeMorsel(
            0, leftSortingData.size(), 0, rightSortingData.size());
        keyBlockMergeMorsel.keyBlockMergeTask = keyBlockMergeTask;

        keyBlockMerger.mergeKeyBlocks(keyBlockMergeMorsel);

        checkTupleIdxesAndFactorizedTableIdxes(resultKeyBlock->getMemBlockData(),
            orderByKeyEncoder1.getKeyBlockEntrySizeInBytes(), expectedTupleIdxOrder,
            expectedFactorizedTableIdxOrder);
    }

    OrderByKeyEncoder prepareMultipleOrderByColsEncoder(uint16_t factorizedTableIdx,
        vector<shared_ptr<FactorizedTable>>& factorizedTables, shared_ptr<DataChunk>& dataChunk,
        TupleSchema& tupleSchema) {
        vector<shared_ptr<ValueVector>> orderByVectors;
        for (auto i = 0u; i < dataChunk->getNumValueVectors(); i++) {
            orderByVectors.emplace_back(dataChunk->getValueVector(i));
        }

        vector<bool> isAscOrder(orderByVectors.size(), true);
        auto orderByKeyEncoder =
            OrderByKeyEncoder(orderByVectors, isAscOrder, *memoryManager, factorizedTableIdx);

        auto factorizedTable = make_unique<FactorizedTable>(*memoryManager, tupleSchema);
        for (auto i = 0u; i < dataChunk->state->selectedSize; i++) {
            factorizedTable->append(orderByVectors, 1);
            orderByKeyEncoder.encodeKeys();
            dataChunk->state->currIdx++;
        }

        factorizedTables.emplace_back(move(factorizedTable));
        return orderByKeyEncoder;
    }

    void prepareMultipleOrderByColsValueVector(vector<int64_t>& int64Values,
        vector<double>& doubleValues, vector<timestamp_t>& timestampValues,
        shared_ptr<DataChunk>& dataChunk) {
        assert(int64Values.size() == doubleValues.size());
        assert(doubleValues.size() == timestampValues.size());
        dataChunk->state->selectedSize = int64Values.size();
        dataChunk->state->currIdx = 0;

        auto int64ValueVector = make_shared<ValueVector>(memoryManager.get(), INT64);
        auto doubleValueVector = make_shared<ValueVector>(memoryManager.get(), DOUBLE);
        auto timestampValueVector = make_shared<ValueVector>(memoryManager.get(), TIMESTAMP);

        dataChunk->insert(0, int64ValueVector);
        dataChunk->insert(1, doubleValueVector);
        dataChunk->insert(2, timestampValueVector);

        for (auto i = 0u; i < int64Values.size(); i++) {
            ((int64_t*)int64ValueVector->values)[i] = int64Values[i];
            ((double*)doubleValueVector->values)[i] = doubleValues[i];
            ((timestamp_t*)timestampValueVector->values)[i] = timestampValues[i];
        }
    }

    void multipleOrderByColTest(bool hasStrCol) {
        vector<int64_t> int64Values1 = {INT64_MIN, -78, 23};
        vector<double> doubleValues1 = {3.28, -0.0001, 4.621};
        vector<timestamp_t> timestampValues1 = {
            Timestamp::FromCString("2035-07-01 11:14:33", strlen("2035-07-01 11:14:33")),
            Timestamp::FromCString("1962-04-07 11:12:35.123", strlen("1962-04-07 11:12:35.123")),
            Timestamp::FromCString("1962-04-07 11:12:35.123", strlen("1962-04-07 11:12:35.123"))};
        auto dataChunk1 = make_shared<DataChunk>(3 + (hasStrCol ? 1 : 0));
        prepareMultipleOrderByColsValueVector(
            int64Values1, doubleValues1, timestampValues1, dataChunk1);

        vector<int64_t> int64Values2 = {INT64_MIN, -78, 23, INT64_MAX};
        vector<double> doubleValues2 = {0.58, -0.0001, 4.621, 4.621};
        vector<timestamp_t> timestampValues2 = {
            Timestamp::FromCString("2036-07-01 11:14:33", strlen("2036-07-01 11:14:33")),
            Timestamp::FromCString("1962-04-07 11:12:35.123", strlen("1962-04-07 11:12:35.123")),
            Timestamp::FromCString("1962-04-07 11:12:35.123", strlen("1962-04-07 11:12:35.123")),
            Timestamp::FromCString("2035-07-01 11:14:33", strlen("2035-07-01 11:14:33"))};
        auto dataChunk2 = make_shared<DataChunk>(3 + (hasStrCol ? 1 : 0));
        prepareMultipleOrderByColsValueVector(
            int64Values2, doubleValues2, timestampValues2, dataChunk2);

        TupleSchema tupleSchema;
        tupleSchema.appendField(
            FieldInTupleSchema(INT64, false /* isUnflat */, 0 /* dataChunkPos */));
        tupleSchema.appendField(
            FieldInTupleSchema(DOUBLE, false /* isUnflat */, 0 /* dataChunkPos */));
        tupleSchema.appendField(
            FieldInTupleSchema(TIMESTAMP, false /* isUnflat */, 0 /* dataChunkPos */));

        if (hasStrCol) {
            tupleSchema.appendField(
                FieldInTupleSchema(STRING, false /* isUnflat */, 0 /* dataChunkPos */));
            auto stringValueVector1 = make_shared<ValueVector>(memoryManager.get(), STRING);
            auto stringValueVector2 = make_shared<ValueVector>(memoryManager.get(), STRING);
            dataChunk1->insert(3, stringValueVector1);
            dataChunk2->insert(3, stringValueVector2);

            stringValueVector1->addString(0, "same prefix 123");
            stringValueVector1->addString(1, "same prefix 128");
            stringValueVector1->addString(2, "same prefix 123");

            stringValueVector2->addString(0, "same prefix 127");
            stringValueVector2->addString(1, "same prefix 123");
            stringValueVector2->addString(2, "same prefix 121");
            stringValueVector2->addString(3, "same prefix 126");
        }
        tupleSchema.initialize();

        vector<shared_ptr<FactorizedTable>> factorizedTables;
        for (auto i = 0; i < 4; i++) {
            factorizedTables.emplace_back(
                make_unique<FactorizedTable>(*memoryManager, tupleSchema));
        }
        auto orderByKeyEncoder2 = prepareMultipleOrderByColsEncoder(
            4 /* factorizedTableIdx */, factorizedTables, dataChunk2, tupleSchema);
        auto orderByKeyEncoder1 = prepareMultipleOrderByColsEncoder(
            5 /* factorizedTableIdx */, factorizedTables, dataChunk1, tupleSchema);

        vector<uint64_t> expectedTupleIdxOrder = {0, 0, 1, 1, 2, 2, 3};
        vector<uint64_t> expectedFactorizedTableIdxOrder = {4, 5, 5, 4, 5, 4, 4};

        vector<StringAndUnstructuredKeyColInfo> stringAndUnstructuredKeyColInfo;
        if (hasStrCol) {
            stringAndUnstructuredKeyColInfo.emplace_back(
                StringAndUnstructuredKeyColInfo(3 /* colIdxInFactorizedTable */,
                    TypeUtils::getDataTypeSize(INT64) + TypeUtils::getDataTypeSize(DOUBLE) +
                        TypeUtils::getDataTypeSize(TIMESTAMP),
                    true /* isAscOrder */, true /* isStrCol */));
            expectedTupleIdxOrder = {0, 0, 1, 1, 2, 2, 3};
            expectedFactorizedTableIdxOrder = {4, 5, 4, 5, 4, 5, 4};
        }

        KeyBlockMerger keyBlockMerger = KeyBlockMerger(factorizedTables,
            stringAndUnstructuredKeyColInfo, orderByKeyEncoder1.getKeyBlockEntrySizeInBytes());
        auto resultKeyBlock =
            make_shared<KeyBlock>(memoryManager->allocateBlock(DEFAULT_MEMORY_BLOCK_SIZE * 2));
        resultKeyBlock->numEntriesInMemBlock = 7;
        auto keyBlockMergeTask =
            make_shared<KeyBlockMergeTask>(orderByKeyEncoder1.getKeyBlocks()[0],
                orderByKeyEncoder2.getKeyBlocks()[0], resultKeyBlock, keyBlockMerger);
        KeyBlockMergeMorsel keyBlockMergeMorsel(0, 3, 0, 4);
        keyBlockMergeMorsel.keyBlockMergeTask = keyBlockMergeTask;

        keyBlockMerger.mergeKeyBlocks(keyBlockMergeMorsel);

        checkTupleIdxesAndFactorizedTableIdxes(resultKeyBlock->getMemBlockData(),
            orderByKeyEncoder1.getKeyBlockEntrySizeInBytes(), expectedTupleIdxOrder,
            expectedFactorizedTableIdxOrder);
    }

    OrderByKeyEncoder prepareMultipleStrKeyColsEncoder(shared_ptr<DataChunk>& dataChunk,
        vector<vector<string>>& strValues, uint16_t factorizedTableIdx,
        vector<shared_ptr<FactorizedTable>>& factorizedTables) {
        dataChunk->state->currIdx = 0;
        dataChunk->state->selectedSize = strValues[0].size();
        for (auto i = 0u; i < strValues.size(); i++) {
            auto strValueVector = make_shared<ValueVector>(memoryManager.get(), STRING);
            dataChunk->insert(i, strValueVector);
            for (auto j = 0u; j < strValues[i].size(); j++) {
                strValueVector->addString(j, strValues[i][j]);
            }
        }

        // The first, second, fourth columns are keyColumns.
        vector<shared_ptr<ValueVector>> orderByVectors{dataChunk->getValueVector(0),
            dataChunk->getValueVector(1), dataChunk->getValueVector(3)};

        vector<shared_ptr<ValueVector>> allVectors{dataChunk->getValueVector(0),
            dataChunk->getValueVector(1), dataChunk->getValueVector(2),
            dataChunk->getValueVector(3)};

        TupleSchema tupleSchema(vector<FieldInTupleSchema>(
            4, FieldInTupleSchema(STRING, false /* isUnflat */, 0 /* dataChunkPos */)));
        auto factorizedTable = make_unique<FactorizedTable>(*memoryManager, tupleSchema);

        vector<bool> isAscOrder(strValues.size(), true);
        auto orderByKeyEncoder =
            OrderByKeyEncoder(orderByVectors, isAscOrder, *memoryManager, factorizedTableIdx);

        for (auto i = 0u; i < strValues[0].size(); i++) {
            factorizedTable->append(allVectors, 1);
            orderByKeyEncoder.encodeKeys();
            dataChunk->state->currIdx++;
        }

        factorizedTables.emplace_back(move(factorizedTable));
        return orderByKeyEncoder;
    }
};

TEST_F(KeyBlockMergerTest, singleOrderByColInt64Test) {
    vector<int64_t> leftSortingData = {INT64_MIN, -8848, 1, 7, 13, INT64_MAX, 0 /* NULL */};
    vector<int64_t> rightSortingData = {INT64_MIN, -6, 4, 22, 32, 38, 0 /* NULL */};
    vector<bool> leftNullMasks = {false, false, false, false, false, false, true};
    vector<bool> rightNullMasks = {false, false, false, false, false, false, true};
    vector<uint64_t> expectedTupleIdxOrder = {0, 0, 1, 1, 2, 2, 3, 4, 3, 4, 5, 5, 6, 6};
    vector<uint64_t> expectedFactorizedTableIdxOrder = {0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1};
    singleOrderByColMergeTest(leftSortingData, leftNullMasks, rightSortingData, rightNullMasks,
        expectedTupleIdxOrder, expectedFactorizedTableIdxOrder, INT64, true /* isAsc */,
        false /* hasPayLoadCol */);
}

TEST_F(KeyBlockMergerTest, singleOrderByColInt64NoNullTest) {
    vector<int64_t> leftSortingData = {INT64_MIN, -512, -5, 22, INT64_MAX};
    vector<int64_t> rightSortingData = {INT64_MIN, -999, 31, INT64_MAX};
    vector<bool> leftNullMasks(leftSortingData.size(), false);
    vector<bool> rightNullMasks(rightSortingData.size(), false);
    vector<uint64_t> expectedTupleIdxOrder = {0, 0, 1, 1, 2, 3, 2, 4, 3};
    vector<uint64_t> expectedFactorizedTableIdxOrder = {0, 1, 1, 0, 0, 0, 1, 0, 1};
    singleOrderByColMergeTest(leftSortingData, leftNullMasks, rightSortingData, rightNullMasks,
        expectedTupleIdxOrder, expectedFactorizedTableIdxOrder, INT64, true /* isAsc */,
        false /* hasPayLoadCol */);
}

TEST_F(KeyBlockMergerTest, singleOrderByColInt64SameValueTest) {
    vector<int64_t> leftSortingData = {4, 4, 4, 4, 4, 4};
    vector<int64_t> rightSortingData = {4, 4, 4, 4, 4, 4, 4, 4, 4};
    vector<bool> leftNullMasks(leftSortingData.size(), false);
    vector<bool> rightNullMasks(rightSortingData.size(), false);
    vector<uint64_t> expectedTupleIdxOrder = {0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7, 8};
    vector<uint64_t> expectedFactorizedTableIdxOrder = {
        0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    singleOrderByColMergeTest(leftSortingData, leftNullMasks, rightSortingData, rightNullMasks,
        expectedTupleIdxOrder, expectedFactorizedTableIdxOrder, INT64, false /* isAsc */,
        false /* hasPayLoadCol */);
}

TEST_F(KeyBlockMergerTest, singleOrderByColInt64LargeNumTuplesTest) {
    vector<int64_t> leftSortingData, rightSortingData;
    vector<uint64_t> expectedTupleIdxOrder(leftSortingData.size() + rightSortingData.size());
    vector<uint64_t> expectedFactorizedTableIdxOrder(
        leftSortingData.size() + rightSortingData.size());
    // Each memory block can hold a maximum of 240 tuples (4096 / (8 + 9)).
    // We fill the leftSortingData with the even numbers of 0-480 and the rightSortingData with the
    // odd numbers of 0-480 so that each of them takes up exactly one memoryBlock.
    for (auto i = 0u; i < 480; i++) {
        if (i % 2) {
            expectedTupleIdxOrder.emplace_back(rightSortingData.size());
            expectedFactorizedTableIdxOrder.emplace_back(1);
            rightSortingData.emplace_back(i);
        } else {
            expectedTupleIdxOrder.emplace_back(leftSortingData.size());
            expectedFactorizedTableIdxOrder.emplace_back(0);
            leftSortingData.emplace_back(i);
        }
    }
    vector<bool> leftNullMasks(leftSortingData.size(), false);
    vector<bool> rightNullMasks(rightSortingData.size(), false);
    singleOrderByColMergeTest(leftSortingData, leftNullMasks, rightSortingData, rightNullMasks,
        expectedTupleIdxOrder, expectedFactorizedTableIdxOrder, INT64, true /* isAsc */,
        false /* hasPayLoadCol */);
}

TEST_F(KeyBlockMergerTest, singleOrderByColUnstrTest) {
    vector<Value> leftSortingData = {
        Value(int64_t(52)), Value(59.4251), Value(int64_t(69)), Value(int64_t(0)) /* NULL */};
    vector<Value> rightSortingData = {Value(52.0004), Value(int64_t(59)), Value(68.98),
        Value(int64_t(70)), Value(int64_t(0)) /* NULL */};
    vector<bool> leftNullMasks = {false, false, false, true};
    vector<bool> rightNullMasks = {false, false, false, false, true};
    vector<uint64_t> expectedTupleIdxOrder = {0, 0, 1, 1, 2, 2, 3, 3, 4};
    vector<uint64_t> expectedFactorizedTableIdxOrder = {0, 1, 1, 0, 1, 0, 1, 0, 1};
    singleOrderByColMergeTest(leftSortingData, leftNullMasks, rightSortingData, rightNullMasks,
        expectedTupleIdxOrder, expectedFactorizedTableIdxOrder, UNSTRUCTURED, true /* isAsc */,
        false /* hasPayLoadCol */);
}

TEST_F(KeyBlockMergerTest, singleOrderByColStringTest) {
    vector<string> leftSortingData = {
        "" /* NULL */, "tiny str", "long string", "common prefix string3", "common prefix string1"};
    vector<string> rightSortingData = {"" /* NULL */, "" /* NULL */, "tiny str1",
        "common prefix string4", "common prefix string2", "common prefix string1",
        "" /* empty str */};
    vector<bool> leftNullMasks = {true, false, false, false, false};
    vector<bool> rightNullMasks = {true, true, false, false, false, false, false};
    vector<uint64_t> expectedTupleIdxOrder = {0, 0, 1, 2, 1, 2, 3, 3, 4, 4, 5, 6};
    vector<uint64_t> expectedFactorizedTableIdxOrder = {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1};
    singleOrderByColMergeTest(leftSortingData, leftNullMasks, rightSortingData, rightNullMasks,
        expectedTupleIdxOrder, expectedFactorizedTableIdxOrder, STRING, false /* isAsc */,
        false /* hasPayLoadCol */);
}

TEST_F(KeyBlockMergerTest, singleOrderByColStringNoNullTest) {
    vector<string> leftSortingData = {"common prefix string1", "common prefix string2",
        "common prefix string3", "long string", "tiny str"};
    vector<string> rightSortingData = {"common prefix string1", "common prefix string2",
        "common prefix string4", "tiny str", "tiny str1"};
    vector<bool> leftNullMasks(leftSortingData.size(), false);
    vector<bool> rightNullMasks(rightSortingData.size(), false);
    vector<uint64_t> expectedTupleIdxOrder = {0, 0, 1, 1, 2, 2, 3, 4, 3, 4};
    vector<uint64_t> expectedFactorizedTableIdxOrder = {0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
    singleOrderByColMergeTest(leftSortingData, leftNullMasks, rightSortingData, rightNullMasks,
        expectedTupleIdxOrder, expectedFactorizedTableIdxOrder, STRING, true /* isAsc */,
        false /* hasPayLoadCol */);
}

TEST_F(KeyBlockMergerTest, singleOrderByColStringWithPayLoadTest) {
    vector<string> leftSortingData = {"", "", "abcabc str", "long long string1", "short str2"};
    vector<string> rightSortingData = {
        "", "test str1", "this is a long string", "very short", "" /* NULL */};
    vector<bool> leftNullMasks(leftSortingData.size(), false);
    vector<bool> rightNullMasks = {false, false, false, false, true};
    vector<uint64_t> expectedTupleIdxOrder = {0, 1, 0, 2, 3, 4, 1, 2, 3, 4};
    vector<uint64_t> expectedFactorizedTableIdxOrder = {0, 0, 1, 0, 0, 0, 1, 1, 1, 1};
    singleOrderByColMergeTest(leftSortingData, leftNullMasks, rightSortingData, rightNullMasks,
        expectedTupleIdxOrder, expectedFactorizedTableIdxOrder, STRING, true /* isAsc */,
        true /* hasPayLoadCol */);
}

TEST_F(KeyBlockMergerTest, multiple0rderByColNoStrTest) {
    multipleOrderByColTest(false /* hasStrCol */);
}

TEST_F(KeyBlockMergerTest, multiple0rderByColOneStrColTest) {
    multipleOrderByColTest(true /* hasStrCol */);
}

TEST_F(KeyBlockMergerTest, multipleStrKeyColsTest) {
    auto dataChunk1 = make_shared<DataChunk>(4);
    auto dataChunk2 = make_shared<DataChunk>(4);
    auto dataChunk3 = make_shared<DataChunk>(4);
    vector<vector<string>> strValues1 = {{"common str1", "common str1", "shorts1", "shorts2"},
        {"same str1", "same str1", "same str1", "same str1"},
        {"payload3", "payload1", "payload2", "payload4"},
        {"long long str4", "long long str6", "long long str3", "long long str2"}};
    vector<vector<string>> strValues2 = {{"common str1", "common str1", "shorts1"},
        {"same str1", "same str1", "same str1"}, {"payload3", "payload1", "payload2"},
        {
            "",
            "long long str5",
            "long long str4",
        }};

    vector<vector<string>> strValues3 = {{"common str1", "common str1"}, {"same str1", "same str1"},
        {"payload3", "payload1"}, {"largerStr", "long long str4"}};
    vector<shared_ptr<FactorizedTable>> factorizedTables;
    auto orderByKeyEncoder1 = prepareMultipleStrKeyColsEncoder(
        dataChunk1, strValues1, 0 /* factorizedTableIdx */, factorizedTables);
    auto orderByKeyEncoder2 = prepareMultipleStrKeyColsEncoder(
        dataChunk2, strValues2, 1 /* factorizedTableIdx */, factorizedTables);
    auto orderByKeyEncoder3 = prepareMultipleStrKeyColsEncoder(
        dataChunk3, strValues3, 2 /* factorizedTableIdx */, factorizedTables);

    vector<StringAndUnstructuredKeyColInfo> stringAndUnstructuredKeyColInfo = {
        StringAndUnstructuredKeyColInfo(0 /* colIdxInFactorizedTable */,
            0 /* colOffsetInEncodedKeyBlock */, true /* isAscOrder */, true /* isStrCol */),
        StringAndUnstructuredKeyColInfo(1 /* colIdxInFactorizedTable */,
            orderByKeyEncoder1.getEncodingSize(STRING), true /* isAscOrder */, true /* isStrCol */),
        StringAndUnstructuredKeyColInfo(3 /* colIdxInFactorizedTable */,
            orderByKeyEncoder1.getEncodingSize(STRING) * 2, true /* isAscOrder */,
            true /* isStrCol */)};

    KeyBlockMerger keyBlockMerger = KeyBlockMerger(factorizedTables,
        stringAndUnstructuredKeyColInfo, orderByKeyEncoder1.getKeyBlockEntrySizeInBytes());

    auto resultKeyBlock =
        make_shared<KeyBlock>(memoryManager->allocateBlock(DEFAULT_MEMORY_BLOCK_SIZE * 2));
    resultKeyBlock->numEntriesInMemBlock = 7;
    auto keyBlockMergeTask = make_shared<KeyBlockMergeTask>(orderByKeyEncoder1.getKeyBlocks()[0],
        orderByKeyEncoder2.getKeyBlocks()[0], resultKeyBlock, keyBlockMerger);
    KeyBlockMergeMorsel keyBlockMergeMorsel(0, 4, 0, 3);
    keyBlockMergeMorsel.keyBlockMergeTask = keyBlockMergeTask;
    keyBlockMerger.mergeKeyBlocks(keyBlockMergeMorsel);

    auto resultMemBlock1 =
        make_shared<KeyBlock>(memoryManager->allocateBlock(DEFAULT_MEMORY_BLOCK_SIZE * 3));
    resultMemBlock1->numEntriesInMemBlock = 9;
    auto keyBlockMergeTask1 = make_shared<KeyBlockMergeTask>(
        resultKeyBlock, orderByKeyEncoder3.getKeyBlocks()[0], resultMemBlock1, keyBlockMerger);
    KeyBlockMergeMorsel keyBlockMergeMorsel1(0, 7, 0, 2);
    keyBlockMergeMorsel1.keyBlockMergeTask = keyBlockMergeTask1;
    keyBlockMerger.mergeKeyBlocks(keyBlockMergeMorsel1);

    vector<uint64_t> expectedTupleIdxOrder = {0, 0, 0, 1, 1, 1, 2, 2, 3};
    vector<uint64_t> expectedFactorizedTableIdxOrder = {1, 2, 0, 2, 1, 0, 0, 1, 0};
    checkTupleIdxesAndFactorizedTableIdxes(resultMemBlock1->getMemBlockData(),
        orderByKeyEncoder1.getKeyBlockEntrySizeInBytes(), expectedTupleIdxOrder,
        expectedFactorizedTableIdxOrder);
}