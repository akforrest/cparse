#include "shunting-yard.h"
#include "shunting-yard-exceptions.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <exception>
#include <string>
#include <stack>
#include <utility>  // For std::pair
#include <cstring>  // For strchr()

#include "cparse.h"
#include "calculator.h"
#include "tokenhelpers.h"

using cparse::Calculator;
using cparse::PackToken;
using cparse::TokenBase;
using cparse::TokenMap;
using cparse::RefToken;
using cparse::Operation;
using cparse::opID_t;
using cparse::Config_t;
using cparse::TokenQueue_t;
using cparse::evaluationData;
using cparse::rpnBuilder;
using cparse::REF;

#include "./builtin-features/functions.h"
#include "./builtin-features/operations.h"
#include "./builtin-features/reservedWords.h"
#include "./builtin-features/typeSpecificFunctions.h"

void cparse::initialize()
{
    builtin_functions::Startup();
    builtin_operations::Startup();
    builtin_reservedWords::Startup();
    builtin_typeSpecificFunctions::Startup();
}

TokenBase * cparse::resolve_reference(TokenBase * b, TokenMap * scope)
{
    if (b->type & REF)
    {
        // Resolve the reference:
        auto * ref = static_cast<RefToken *>(b);
        TokenBase * value = ref->resolve(scope);

        delete ref;
        return value;
    }

    return b;
}

void cparse::cleanStack(std::stack<TokenBase *> st)
{
    while (!st.empty())
    {
        delete cparse::resolve_reference(st.top());
        st.pop();
    }
}

/* * * * * Operation class: * * * * */

// Convert a type into an unique mask for bit wise operations:
uint32_t Operation::mask(tokType_t type)
{
    if (type == ANY_TYPE)
    {
        return 0xFFFF;
    }

    return ((type & 0xE0) << 24) | (1 << (type & 0x1F));
}

// Build a mask for each pair of operands
opID_t Operation::build_mask(tokType_t left, tokType_t right)
{
    opID_t result = mask(left);
    return (result << 32) | mask(right);
}

Operation::Operation(const opSignature_t & sig, opFunc_t func)
    : _mask(build_mask(sig.left, sig.right)), _exec(func) {}

cparse::opID_t Operation::getMask() const
{
    return _mask;
}

PackToken Operation::exec(const PackToken & left, const PackToken & right, evaluationData * data) const
{
    return _exec(left, right, data);
}

/* * * * * rpnBuilder Class: * * * * */

void rpnBuilder::cleanRPN(TokenQueue_t * rpn)
{
    while (!rpn->empty())
    {
        delete cparse::resolve_reference(rpn->front());
        rpn->pop();
    }
}

/**
 * Consume operators with precedence >= than op
 * and add them to the RPN
 *
 * The algorithm works as follows:
 *
 * Let p(o) denote the precedence of an operator o.
 *
 * If the token is an operator, o1, then
 *   While there is an operator token, o2, at the top
 *       and p(o1) >= p(o2) (`>` for Right to Left associativity)
 *     then:
 *
 *     pop o2 off the stack onto the output queue.
 *   Push o1 on the stack.
 */
void rpnBuilder::handle_opStack(const QString & op)
{
    QString cur_op;

    // If it associates from left to right:
    if (opp.assoc(op) == 0)
    {
        while (!opStack.empty() &&
               opp.prec(op) >= opp.prec(opStack.top()))
        {
            cur_op = normalize_op(opStack.top());
            rpn.push(new Token<QString>(cur_op, OP));
            opStack.pop();
        }
    }
    else
    {
        while (!opStack.empty() &&
               opp.prec(op) > opp.prec(opStack.top()))
        {
            cur_op = normalize_op(opStack.top());
            rpn.push(new Token<QString>(cur_op, OP));
            opStack.pop();
        }
    }
}

void rpnBuilder::handle_binary(const QString & op)
{
    // Handle OP precedence
    handle_opStack(op);
    // Then push the current op into the stack:
    opStack.push(op);
}

// Convert left unary operators to binary and handle them:
void rpnBuilder::handle_left_unary(const QString & unary_op)
{
    this->rpn.push(new TokenUnary());
    // Only put it on the stack and wait to check op precedence:
    opStack.push(unary_op);
}

// Convert right unary operators to binary and handle them:
void rpnBuilder::handle_right_unary(const QString & unary_op)
{
    // Handle OP precedence:
    handle_opStack(unary_op);
    // Add the unary token:
    this->rpn.push(new TokenUnary());
    // Then add the current op directly into the rpn:
    rpn.push(new Token<QString>(normalize_op(unary_op), OP));
}

// Find out if op is a binary or unary operator and handle it:
void rpnBuilder::handle_op(const QString & op)
{
    // If it's a left unary operator:
    if (this->lastTokenWasOp)
    {
        if (opp.exists("L" + op))
        {
            handle_left_unary("L" + op);
            this->lastTokenWasUnary = true;
            this->lastTokenWasOp = op[0].unicode();
        }
        else
        {
            cleanRPN(&(this->rpn));
            throw std::domain_error("Unrecognized unary operator: '" + op.toStdString() + "'.");
        }

        // If its a right unary operator:
    }
    else if (opp.exists("R" + op))
    {
        handle_right_unary("R" + op);

        // Set it to false, since we have already added
        // an unary token and operand to the stack:
        this->lastTokenWasUnary = false;
        this->lastTokenWasOp = false;

        // If it is a binary operator:
    }
    else
    {
        if (opp.exists(op))
        {
            handle_binary(op);
        }
        else
        {
            cleanRPN(&(rpn));
            throw std::domain_error("Undefined operator: `" + op.toStdString() + "`!");
        }

        this->lastTokenWasUnary = false;
        this->lastTokenWasOp = op[0].unicode();
    }
}

void rpnBuilder::handle_token(TokenBase * token)
{
    if (!lastTokenWasOp)
    {
        throw syntax_error("Expected an operator or bracket but got " + PackToken::str(token));
    }

    rpn.push(token);
    lastTokenWasOp = false;
    lastTokenWasUnary = false;
}

void rpnBuilder::open_bracket(const QString & bracket)
{
    opStack.push(bracket);
    lastTokenWasOp = bracket[0].unicode();
    lastTokenWasUnary = false;
    ++bracketLevel;
}

void rpnBuilder::close_bracket(const QString & bracket)
{
    if (char(lastTokenWasOp) == bracket[0])
    {
        rpn.push(new Tuple());
    }

    QString cur_op;

    while (!opStack.empty() && opStack.top() != bracket)
    {
        cur_op = normalize_op(opStack.top());
        rpn.push(new Token<QString>(cur_op, OP));
        opStack.pop();
    }

    if (opStack.empty())
    {
        rpnBuilder::cleanRPN(&rpn);
        throw syntax_error("Extra '" + bracket + "' on the expression!");
    }

    opStack.pop();
    lastTokenWasOp = false;
    lastTokenWasUnary = false;
    --bracketLevel;
}

bool rpnBuilder::isvarchar(const char c)
{
    return isalpha(c) || c == '_';
}

QString rpnBuilder::parseVar(const char * expr, const char ** rest)
{
    QString ss;
    ss += *expr;
    ++expr;

    while (rpnBuilder::isvarchar(*expr) || isdigit(*expr))
    {
        ss += *expr;
        ++expr;
    }

    if (rest)
    {
        *rest = expr;
    }

    return ss;
}

void cleanStack(std::stack<TokenBase *> st)
{
    while (!st.empty())
    {
        delete resolve_reference(st.top());
        st.pop();
    }
}

cparse::OppMap_t::OppMap_t()
{
    // These operations are hard-coded inside the Calculator,
    // thus their precedence should always be defined:
    pr_map["[]"] = -1;
    pr_map["()"] = -1;
    pr_map["["] = 0x7FFFFFFF;
    pr_map["("] = 0x7FFFFFFF;
    pr_map["{"] = 0x7FFFFFFF;
    RtoL.insert("=");
}

void cparse::OppMap_t::add(const QString & op, int precedence)
{
    if (precedence < 0)
    {
        RtoL.insert(op);
        precedence = -precedence;
    }

    pr_map[op] = precedence;
}

void cparse::OppMap_t::addUnary(const QString & op, int precedence)
{
    add("L" + op, precedence);

    // Also add a binary operator with same precedence so
    // it is possible to verify if an op exists just by checking
    // the binary set of operators:
    if (!exists(op))
    {
        add(op, precedence);
    }
}

void cparse::OppMap_t::addRightUnary(const QString & op, int precedence)
{
    add("R" + op, precedence);

    // Also add a binary operator with same precedence so
    // it is possible to verify if an op exists just by checking
    // the binary set of operators:
    if (!exists(op))
    {
        add(op, precedence);
    }
    else
    {
        // Note that using a unary and binary operators with
        // the same left operand is ambiguous and that the unary
        // operator will take precedence.
        //
        // So only do it if you know the expected left operands
        // have distinct types.
    }
}

int cparse::OppMap_t::prec(const QString & op) const
{
    return pr_map.at(op);
}

bool cparse::OppMap_t::assoc(const QString & op) const
{
    return RtoL.count(op);
}

bool cparse::OppMap_t::exists(const QString & op) const
{
    return pr_map.count(op);
}

evaluationData::evaluationData(TokenQueue_t rpn, const TokenMap & scope, const opMap_t & opMap)
    : rpn(std::move(rpn)), scope(scope), opMap(opMap) {}

void cparse::parserMap_t::add(const QString & word, rWordParser_t * parser)
{
    wmap[word] = parser;
}

void cparse::parserMap_t::add(char c, rWordParser_t * parser)
{
    cmap[c] = parser;
}

cparse::rWordParser_t * cparse::parserMap_t::find(const QString & text)
{
    rWordMap_t::iterator w_it;

    if ((w_it = wmap.find(text)) != wmap.end())
    {
        return w_it->second;
    }

    return nullptr;
}

cparse::rWordParser_t * cparse::parserMap_t::find(char c)
{
    rCharMap_t::iterator c_it;

    if ((c_it = cmap.find(c)) != cmap.end())
    {
        return c_it->second;
    }

    return nullptr;
}

RefToken::RefToken(PackToken k, TokenBase * v, PackToken m) :
    TokenBase(v->type | REF), original_value(v), key(std::forward<PackToken>(k)), origin(std::forward<PackToken>(m)) {}

RefToken::RefToken(PackToken k, PackToken v, PackToken m) :
    TokenBase(v->type | REF), original_value(std::forward<PackToken>(v)), key(std::forward<PackToken>(k)), origin(std::forward<PackToken>(m)) {}

TokenBase * RefToken::resolve(TokenMap * localScope) const
{
    TokenBase * result = nullptr;

    // Local variables have no origin == NONE,
    // thus, require a localScope to be resolved:
    if (origin->type == NONE && localScope)
    {
        // Get the most recent value from the local scope:
        PackToken * r_value = localScope->find(key.asString());

        if (r_value)
        {
            result = (*r_value)->clone();
        }
    }

    // In last case return the compilation-time value:
    return result ? result : original_value->clone();
}

TokenBase * RefToken::clone() const
{
    return new RefToken(*this);
}

cparse::opSignature_t::opSignature_t(const tokType_t L, const QString & op, const tokType_t R)
    : left(L), op(op), right(R) {}

void cparse::opMap_t::add(const opSignature_t & sig, Operation::opFunc_t func)
{
    (*this)[sig.op].push_back(Operation(sig, func));
}

QString cparse::opMap_t::str() const
{
    if (this->empty())
    {
        return "{}";
    }

    QString result = "{ ";

    for (const auto & pair : (*this))
    {
        result += "\"" + pair.first + "\", ";
    }

    result.chop(2);
    return result + " }";
}

Config_t::Config_t(parserMap_t p, OppMap_t opp, opMap_t opMap)
    : parserMap(std::move(p)), opPrecedence(std::move(opp)), opMap(std::move(opMap)) {}
