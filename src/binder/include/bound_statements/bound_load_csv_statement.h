#pragma once

#include "src/binder/include/bound_statements/bound_reading_statement.h"
#include "src/common/include/types.h"

using namespace graphflow::common;

namespace graphflow {
namespace binder {

/**
 * Assume csv file contains structured information (data type is specified in the header).
 */
class BoundLoadCSVStatement : public BoundReadingStatement {

public:
    BoundLoadCSVStatement(string filePath, char tokenSeparator, string lineVariableName,
        vector<pair<string, DataType>> headerInfo)
        : BoundReadingStatement{LOAD_CSV_STATEMENT}, filePath{move(filePath)},
          tokenSeparator(tokenSeparator), lineVariableName{move(lineVariableName)},
          headerInfo{move(headerInfo)} {}

public:
    string filePath;
    char tokenSeparator;
    string lineVariableName;
    vector<pair<string, DataType>> headerInfo;
};

} // namespace binder
} // namespace graphflow
