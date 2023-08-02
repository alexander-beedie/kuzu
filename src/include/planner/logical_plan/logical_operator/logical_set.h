#pragma once

#include "base_logical_operator.h"
#include "logical_update.h"
#include "planner/logical_plan/logical_operator/flatten_resolver.h"

namespace kuzu {
namespace planner {

struct LogicalSetPropertyInfo {
    std::shared_ptr<binder::Expression> nodeOrRel;
    binder::expression_pair setItem;

    LogicalSetPropertyInfo(
        std::shared_ptr<binder::Expression> nodeOrRel, binder::expression_pair setItem)
        : nodeOrRel{std::move(nodeOrRel)}, setItem{std::move(setItem)} {}
    LogicalSetPropertyInfo(const LogicalSetPropertyInfo& other)
        : nodeOrRel{other.nodeOrRel}, setItem{other.setItem} {}

    inline std::unique_ptr<LogicalSetPropertyInfo> copy() const {
        return std::make_unique<LogicalSetPropertyInfo>(*this);
    }
};

class LogicalSetNodeProperty : public LogicalOperator {
public:
    LogicalSetNodeProperty(std::vector<std::unique_ptr<LogicalSetPropertyInfo>> infos,
        std::shared_ptr<LogicalOperator> child)
        : LogicalOperator{LogicalOperatorType::SET_NODE_PROPERTY, std::move(child)},
          infos{std::move(infos)} {}

    inline void computeFactorizedSchema() final { copyChildSchema(0); }
    inline void computeFlatSchema() final { copyChildSchema(0); }

    inline const std::vector<std::unique_ptr<LogicalSetPropertyInfo>>& getInfosRef() const {
        return infos;
    }

    std::string getExpressionsForPrinting() const final;

    std::unique_ptr<LogicalOperator> copy() final;

private:
    std::vector<std::unique_ptr<LogicalSetPropertyInfo>> infos;
};

class LogicalSetRelProperty : public LogicalOperator {
public:
    LogicalSetRelProperty(std::vector<std::unique_ptr<LogicalSetPropertyInfo>> infos,
        std::shared_ptr<LogicalOperator> child)
        : LogicalOperator{LogicalOperatorType::SET_REL_PROPERTY, std::move(child)}, infos{std::move(
                                                                                        infos)} {}

    inline void computeFactorizedSchema() final { copyChildSchema(0); }
    inline void computeFlatSchema() final { copyChildSchema(0); }

    inline const std::vector<std::unique_ptr<LogicalSetPropertyInfo>>& getInfosRef() const {
        return infos;
    }

    f_group_pos_set getGroupsPosToFlatten(uint32_t idx);

    std::string getExpressionsForPrinting() const final;

    std::unique_ptr<LogicalOperator> copy() final;

private:
    std::vector<std::unique_ptr<LogicalSetPropertyInfo>> infos;
};

} // namespace planner
} // namespace kuzu
