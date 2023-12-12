#pragma once

#include <cstdint>

#include "reader_config.h"

namespace kuzu {

namespace storage {
class PrimaryKeyIndex;
}

namespace common {

enum class RdfReaderMode : uint8_t {
    RESOURCE = 0,
    LITERAL = 1,
    RESOURCE_TRIPLE = 2,
    LITERAL_TRIPLE = 3,
};

struct RdfReaderConfig final : public ExtraReaderConfig {
    RdfReaderMode mode;

    explicit RdfReaderConfig(RdfReaderMode mode) : mode{mode} {}
    RdfReaderConfig(const RdfReaderConfig& other) : mode{other.mode} {}

    inline std::unique_ptr<ExtraReaderConfig> copy() override {
        return std::make_unique<RdfReaderConfig>(*this);
    }
};

} // namespace common
} // namespace kuzu
