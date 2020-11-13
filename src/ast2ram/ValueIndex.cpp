/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ValueIndex.h
 *
 * Defines the ValueIndex class, which indexes the location of variables
 * and record references within a loop nest during rule conversion.
 *
 ***********************************************************************/

#include "ast2ram/ValueIndex.h"
#include "ast/Aggregator.h"
#include "ast/Variable.h"
#include "ast2ram/Location.h"
#include "ram/Relation.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <set>

namespace souffle::ast2ram {

ValueIndex::ValueIndex() = default;
ValueIndex::~ValueIndex() = default;

void ValueIndex::addVarReference(const ast::Variable& var, const Location& l) {
    std::set<Location>& locs = varReferencePoints[var.getName()];
    locs.insert(l);
}

void ValueIndex::addVarReference(const ast::Variable& var, int ident, int pos, std::string rel) {
    addVarReference(var, Location({ident, pos, rel}));
}

bool ValueIndex::isDefined(const ast::Variable& var) const {
    return varReferencePoints.find(var.getName()) != varReferencePoints.end();
}

const Location& ValueIndex::getDefinitionPoint(const ast::Variable& var) const {
    auto pos = varReferencePoints.find(var.getName());
    assert(pos != varReferencePoints.end() && "Undefined variable referenced!");
    return *pos->second.begin();
}

void ValueIndex::setGeneratorLoc(const ast::Argument& arg, const Location& loc) {
    generatorDefinitionPoints.push_back(std::make_pair(&arg, loc));
}

const Location& ValueIndex::getGeneratorLoc(const ast::Argument& arg) const {
    if (dynamic_cast<const ast::Aggregator*>(&arg) != nullptr) {
        // aggregators can be used interchangeably if syntactically equal
        for (const auto& cur : generatorDefinitionPoints) {
            if (*cur.first == arg) {
                return cur.second;
            }
        }
    } else {
        // otherwise, unique for each appearance
        for (const auto& cur : generatorDefinitionPoints) {
            if (cur.first == &arg) {
                return cur.second;
            }
        }
    }
    fatal("arg `%s` has no generator location", arg);
}

void ValueIndex::setRecordDefinition(const ast::RecordInit& init, const Location& l) {
    recordDefinitionPoints.insert({&init, l});
}

void ValueIndex::setRecordDefinition(const ast::RecordInit& init, int ident, int pos, std::string rel) {
    setRecordDefinition(init, Location({ident, pos, rel}));
}

const Location& ValueIndex::getDefinitionPoint(const ast::RecordInit& init) const {
    assert(contains(recordDefinitionPoints, &init) && "undefined record");
    return recordDefinitionPoints.at(&init);
}

bool ValueIndex::isGenerator(const int level) const {
    // check for aggregator definitions
    return any_of(generatorDefinitionPoints,
            [&level](const auto& location) { return location.second.identifier == level; });
}

bool ValueIndex::isSomethingDefinedOn(int level) const {
    // check for variable definitions
    for (const auto& cur : varReferencePoints) {
        if (cur.second.begin()->identifier == level) {
            return true;
        }
    }
    // check for record definitions
    for (const auto& cur : recordDefinitionPoints) {
        if (cur.second.identifier == level) {
            return true;
        }
    }
    // nothing defined on this level
    return false;
}

void ValueIndex::print(std::ostream& out) const {
    out << "Variables:\n\t";
    out << join(varReferencePoints, "\n\t");
}

}  // namespace souffle::ast2ram
