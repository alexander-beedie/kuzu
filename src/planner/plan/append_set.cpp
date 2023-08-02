#include "binder/query/updating_clause/bound_set_clause.h"
#include "planner/logical_plan/logical_operator/logical_set.h"
#include "planner/query_planner.h"

namespace kuzu {
namespace planner {

void QueryPlanner::appendSetNodeProperty(
    const std::vector<binder::BoundSetPropertyInfo*>& boundInfos, LogicalPlan& plan) {
    for (auto& boundInfo : boundInfos) {
        auto node = (NodeExpression*)boundInfo->nodeOrRel.get();
        auto lhsNodeID = node->getInternalIDProperty();
        auto rhs = boundInfo->setItem.second;
        // flatten rhs
        auto rhsDependentGroupsPos = plan.getSchema()->getDependentGroupsPos(rhs);
        auto rhsGroupsPosToFlatten = factorization::FlattenAllButOne::getGroupsPosToFlatten(
            rhsDependentGroupsPos, plan.getSchema());
        appendFlattens(rhsGroupsPosToFlatten, plan);
        // flatten lhs if needed
        auto lhsGroupPos = plan.getSchema()->getGroupPos(*lhsNodeID);
        auto rhsLeadingGroupPos =
            SchemaUtils::getLeadingGroupPos(rhsDependentGroupsPos, *plan.getSchema());
        if (lhsGroupPos != rhsLeadingGroupPos) {
            appendFlattenIfNecessary(lhsGroupPos, plan);
        }
    }
    std::vector<std::unique_ptr<LogicalSetPropertyInfo>> logicalInfos;
    logicalInfos.reserve(boundInfos.size());
    for (auto& boundInfo : boundInfos) {
        logicalInfos.push_back(
            std::make_unique<LogicalSetPropertyInfo>(boundInfo->nodeOrRel, boundInfo->setItem));
    }
    auto setNodeProperty =
        std::make_shared<LogicalSetNodeProperty>(std::move(logicalInfos), plan.getLastOperator());
    setNodeProperty->computeFactorizedSchema();
    plan.setLastOperator(setNodeProperty);
}

void QueryPlanner::appendSetRelProperty(
    const std::vector<binder::BoundSetPropertyInfo*>& boundInfos, LogicalPlan& plan) {
    std::vector<std::unique_ptr<LogicalSetPropertyInfo>> logicalInfos;
    logicalInfos.reserve(boundInfos.size());
    for (auto& boundInfo : boundInfos) {
        logicalInfos.push_back(
            std::make_unique<LogicalSetPropertyInfo>(boundInfo->nodeOrRel, boundInfo->setItem));
    }
    auto setRelProperty =
        std::make_shared<LogicalSetRelProperty>(std::move(logicalInfos), plan.getLastOperator());
    for (auto i = 0u; i < boundInfos.size(); ++i) {
        appendFlattens(setRelProperty->getGroupsPosToFlatten(i), plan);
        setRelProperty->setChild(0, plan.getLastOperator());
    }
    setRelProperty->computeFactorizedSchema();
    plan.setLastOperator(setRelProperty);
}

} // namespace planner
} // namespace kuzu
