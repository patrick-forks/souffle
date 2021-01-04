/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Visitor.h
 *
 * Defines a visitor pattern for AST
 *
 ***********************************************************************/

#pragma once

#include "ast/Aggregator.h"
#include "ast/AlgebraicDataType.h"
#include "ast/Argument.h"
#include "ast/Atom.h"
#include "ast/Attribute.h"
#include "ast/BinaryConstraint.h"
#include "ast/BooleanConstraint.h"
#include "ast/BranchInit.h"
#include "ast/Clause.h"
#include "ast/Component.h"
#include "ast/ComponentInit.h"
#include "ast/ComponentType.h"
#include "ast/Constant.h"
#include "ast/Constraint.h"
#include "ast/Counter.h"
#include "ast/FunctionalConstraint.h"
#include "ast/Functor.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/Literal.h"
#include "ast/Negation.h"
#include "ast/NilConstant.h"
#include "ast/Node.h"
#include "ast/NumericConstant.h"
#include "ast/Pragma.h"
#include "ast/Program.h"
#include "ast/RecordInit.h"
#include "ast/RecordType.h"
#include "ast/Relation.h"
#include "ast/StringConstant.h"
#include "ast/SubsetType.h"
#include "ast/Term.h"
#include "ast/Type.h"
#include "ast/TypeCast.h"
#include "ast/UnionType.h"
#include "ast/UnnamedVariable.h"
#include "ast/UserDefinedFunctor.h"
#include "ast/Variable.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace souffle::ast {

/** A tag type required for the is_ast_visitor type trait to identify AstVisitors */
struct ast_visitor_tag {};

/**
 * The generic base type of all AstVisitors realizing the dispatching of
 * visitor calls. Each visitor may define a return type R and a list of
 * extra parameters to be passed along with the visited Nodes to the
 * corresponding visitor function.
 *
 * @tparam R the result type produced by a visit call
 * @tparam Params extra parameters to be passed to the visit call
 */
template <typename R = void, typename NodeType = Node const, typename... Params>
struct Visitor : public ast_visitor_tag {
    /** A virtual destructor */
    virtual ~Visitor() = default;

    /** The main entry for the user allowing visitors to be utilized as functions */
    R operator()(NodeType& node, Params const&... args) {
        return visit(node, args...);
    }

    /**
     * The main entry for a visit process conducting the dispatching of
     * a visit to the various sub-types of Nodes. Sub-classes may override
     * this implementation to conduct pre-visit operations.
     *
     * @param node the node to be visited
     * @param args a list of extra parameters to be forwarded
     */
    virtual R visit(NodeType& node, Params const&... args) {
        // dispatch node processing based on dynamic type

#define FORWARD(Kind) \
    if (auto* n = as<Kind>(node)) return visit##Kind(*n, args...);

        // types
        FORWARD(SubsetType);
        FORWARD(UnionType);
        FORWARD(RecordType);
        FORWARD(AlgebraicDataType);

        // arguments
        FORWARD(Variable)
        FORWARD(UnnamedVariable)
        FORWARD(IntrinsicFunctor)
        FORWARD(UserDefinedFunctor)
        FORWARD(Counter)
        FORWARD(NumericConstant)
        FORWARD(StringConstant)
        FORWARD(NilConstant)
        FORWARD(TypeCast)
        FORWARD(RecordInit)
        FORWARD(BranchInit)
        FORWARD(Aggregator)

        // literals
        FORWARD(Atom)
        FORWARD(Negation)
        FORWARD(BooleanConstraint)
        FORWARD(BinaryConstraint)
        FORWARD(FunctionalConstraint)

        // components
        FORWARD(ComponentType);
        FORWARD(ComponentInit);
        FORWARD(Component);

        // rest
        FORWARD(Attribute);
        FORWARD(Clause);
        FORWARD(Relation);
        FORWARD(Program);
        FORWARD(Pragma);

#undef FORWARD

        // did not work ...

        fatal("unsupported type: %s", typeid(node).name());
    }

#define LINK(Kind, Parent)                                                          \
    virtual R visit##Kind(copy_const_t<NodeType, Kind>& n, Params const&... args) { \
        return visit##Parent(n, args...);                                           \
    }

    // -- types --
    LINK(SubsetType, Type);
    LINK(RecordType, Type);
    LINK(AlgebraicDataType, Type);
    LINK(UnionType, Type);
    LINK(Type, Node);

    // -- arguments --
    LINK(Variable, Argument)
    LINK(UnnamedVariable, Argument)
    LINK(Counter, Argument)
    LINK(TypeCast, Argument)
    LINK(BranchInit, Argument)

    LINK(NumericConstant, Constant)
    LINK(StringConstant, Constant)
    LINK(NilConstant, Constant)
    LINK(Constant, Argument)

    LINK(IntrinsicFunctor, Functor)
    LINK(UserDefinedFunctor, Functor)

    LINK(RecordInit, Term)
    LINK(Functor, Term)

    LINK(Term, Argument)

    LINK(Aggregator, Argument)

    LINK(Argument, Node);

    // literals
    LINK(Atom, Literal)
    LINK(Negation, Literal)
    LINK(Literal, Node);

    LINK(BooleanConstraint, Constraint)
    LINK(BinaryConstraint, Constraint)
    LINK(FunctionalConstraint, Constraint)
    LINK(Constraint, Literal)

    // components
    LINK(ComponentType, Node);
    LINK(ComponentInit, Node);
    LINK(Component, Node);

    // -- others --
    LINK(Program, Node);
    LINK(Attribute, Node);
    LINK(Clause, Node);
    LINK(Relation, Node);
    LINK(Pragma, Node);

#undef LINK

    /** The base case for all visitors -- if no more specific overload was defined */
    virtual R visitNode(const Node& /*node*/, Params const&... /*args*/) {
        return R();
    }
};

namespace detail {
template <typename T>
using EnableIfNode =
        std::enable_if_t<std::is_base_of_v<Node, std::remove_const_t<std::remove_reference_t<T>>>>;
}

/**
 * A utility function visiting all nodes within the ast rooted by the given node
 * recursively in a depth-first pre-order fashion applying the given visitor to each
 * encountered node.
 *
 * @param root the root of the AST to be visited
 * @param visitor the visitor to be applied on each node
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <class NodeType, typename... Ts, typename... Args>
detail::EnableIfNode<NodeType> visitDepthFirstPreOrder(
        NodeType&& root, Visitor<Ts...>& visitor, Args const&... args) {
    visitor(root, args...);
    for (auto* cur : root.getChildNodes()) {
        if (cur != nullptr) {
            visitDepthFirstPreOrder(*cur, visitor, args...);
        }
    }
}

/**
 * A utility function visiting all nodes within the ast rooted by the given node
 * recursively in a depth-first post-order fashion applying the given visitor to each
 * encountered node.
 *
 * @param root the root of the AST to be visited
 * @param visitor the visitor to be applied on each node
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <class NodeType, typename... Ts, typename... Args>
detail::EnableIfNode<NodeType> visitDepthFirstPostOrder(
        NodeType&& root, Visitor<Ts...>& visitor, Args const&... args) {
    for (auto* cur : root.getChildNodes()) {
        if (cur != nullptr) {
            visitDepthFirstPostOrder(*cur, visitor, args...);
        }
    }
    visitor(root, args...);
}

/**
 * A utility function visiting all nodes within the ast rooted by the given node
 * recursively in a depth-first pre-order fashion applying the given visitor to each
 * encountered node.
 *
 * @param root the root of the AST to be visited
 * @param visitor the visitor to be applied on each node
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <class NodeType, typename... Ts, typename... Args>
detail::EnableIfNode<NodeType> visitDepthFirst(
        NodeType&& root, Visitor<Ts...>& visitor, Args const&... args) {
    visitDepthFirstPreOrder(root, visitor, args...);
}

namespace detail {

/**
 * A specialized visitor wrapping a lambda function -- an auxiliary type required
 * for visitor convenience functions.
 */
template <class NodeToVisit, typename F = std::function<void(NodeToVisit&)>>
struct LambdaVisitor : public Visitor<void, copy_const_t<NodeToVisit, Node>> {
    F lambda;
    LambdaVisitor(F lam) : lambda(std::move(lam)) {}
    void visit(copy_const_t<NodeToVisit, Node>& node) override {
        if (auto* n = as<NodeToVisit>(node)) {
            lambda(*n);
        }
    }
};

// Only used for deduction
template <typename R, typename T>
T& getNodeTypeHelper(std::function<R(T&)>);

template <typename F>
typename lambda_traits<F>::arg0_type& getNodeTypeHelper(F const&);

/**
 * A factory function for creating LambdaVisitor instances.
 */
template <typename F>
auto makeLambdaVisitor(F&& f) {
    using NodeToVisit = std::remove_reference_t<decltype(getNodeTypeHelper(f))>;
    return LambdaVisitor<NodeToVisit, std::remove_reference_t<F>>(std::forward<F>(f));
}

/**
 * A type trait determining whether a given type is a visitor or not.
 */
template <typename T>
struct is_ast_visitor {
    static constexpr size_t value = std::is_base_of<ast_visitor_tag, T>::value;
};

template <typename T>
struct is_ast_visitor<const T> : public is_ast_visitor<T> {};

template <typename T>
struct is_ast_visitor<T&> : public is_ast_visitor<T> {};
}  // namespace detail

/**
 * A utility function visiting all nodes within the ast rooted by the given node
 * recursively in a depth-first pre-order fashion applying the given function to each
 * encountered node.
 *
 * @param root the root of the AST to be visited
 * @param fun the function to be applied
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <class NodeType, typename F, typename... Args>
std::enable_if_t<!detail::is_ast_visitor<F>::value> visitDepthFirst(
        NodeType&& root, F&& fun, Args const&... args) {
    auto visitor = detail::makeLambdaVisitor(std::forward<F>(fun));
    visitDepthFirst(root, visitor, args...);
}

/**
 * A utility function visiting all nodes within the ast rooted by the given node
 * recursively in a depth-first pre-order fashion applying the given function to each
 * encountered node.
 *
 * @param root the root of the AST to be visited
 * @param fun the function to be applied
 * @param args a list of extra parameters to be forwarded to the visitor
 */
/* FIXME: tomp
template <typename Lambda, typename R = typename lambda_traits<Lambda>::result_type,
        typename N = typename lambda_traits<Lambda>::arg0_type>
typename std::enable_if<!detail::is_ast_visitor<Lambda>::value, void>::type visitDepthFirst(
        const Node& root, const Lambda& fun) {
    visitDepthFirst(root, std::function<R(const N&)>(fun));
}
*/

/**
 * A utility function visiting all nodes within a given list of AST root nodes
 * recursively in a depth-first pre-order fashion applying the given function to each
 * encountered node.
 *
 * @param list the list of roots of the ASTs to be visited
 * @param fun the function to be applied
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <typename T, typename F, typename... Args>
void visitDepthFirst(const std::vector<T*>& list, F&& fun, Args const&... args) {
    for (auto&& cur : list) {
        visitDepthFirst(*cur, std::forward<F>(fun), args...);
    }
}

/**
 * A utility function visiting all nodes within a given list of AST root nodes
 * recursively in a depth-first pre-order fashion applying the given function to each
 * encountered node.
 *
 * @param list the list of roots of the ASTs to be visited
 * @param fun the function to be applied
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <typename T, typename F, typename... Args>
void visitDepthFirst(const VecOwn<T>& list, F&& fun, Args const&... args) {
    for (auto&& cur : list) {
        visitDepthFirst(*cur, std::forward<F>(fun), args...);
    }
}

/**
 * A utility function visiting all nodes within the ast rooted by the given node
 * recursively in a depth-first post-order fashion applying the given function to each
 * encountered node.
 *
 * @param root the root of the AST to be visited
 * @param fun the function to be applied
 * @param args a list of extra parameters to be forwarded to the visitor
 */
template <class NodeType, typename F, typename... Args>
detail::EnableIfNode<NodeType> visitDepthFirstPostOrder(NodeType&& root, F&& fun, Args const&... args) {
    auto visitor = detail::makeLambdaVisitor(std::forward<F>(fun), args...);
    visitDepthFirstPostOrder(root, visitor);
}

/**
 * A utility function visiting all nodes within the ast rooted by the given node
 * recursively in a depth-first post-order fashion applying the given function to each
 * encountered node.
 *
 * @param root the root of the AST to be visited
 * @param fun the function to be applied
 * @param args a list of extra parameters to be forwarded to the visitor
 */
/*
template <typename Lambda, typename R = typename lambda_traits<Lambda>::result_type,
        typename N = typename lambda_traits<Lambda>::arg0_type>
typename std::enable_if<!detail::is_ast_visitor<Lambda>::value, void>::type visitDepthFirstPostOrder(
        const Node& root, const Lambda& fun) {
    visitDepthFirstPostOrder(root, std::function<R(const N&)>(fun));
}
*/

}  // namespace souffle::ast
