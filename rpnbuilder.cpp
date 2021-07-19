#include "rpnbuilder.h"
#include "exceptions.h"

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

#include "builtin-features/functions.h"
#include "builtin-features/operations.h"
#include "builtin-features/reservedwords.h"
#include "builtin-features/typespecificfunctions.h"

using namespace cparse;

Q_LOGGING_CATEGORY(cparseLog, "cparse")

void cparse::initialize()
{
    builtin_functions::Startup();
    builtin_operations::Startup();
    builtin_reservedWords::Startup();
    builtin_typeSpecificFunctions::Startup();
}

Token * cparse::resolve_reference(Token * b, TokenMap * scope)
{
    if (b->m_type & REF)
    {
        // Resolve the reference:
        auto * ref = static_cast<RefToken *>(b);
        Token * value = ref->resolve(scope);

        delete ref;
        return value;
    }

    return b;
}

void cparse::cleanStack(std::stack<Token *> st)
{
    while (!st.empty())
    {
        delete cparse::resolve_reference(st.top());
        st.pop();
    }
}

/* * * * * Operation class: * * * * */

// Convert a type into an unique mask for bit wise operations:
uint32_t Operation::mask(TokenType type)
{
    if (type == ANY_TYPE)
    {
        return 0xFFFF;
    }

    return ((type & 0xE0) << 24) | (1 << (type & 0x1F));
}

// Build a mask for each pair of operands
OpId Operation::buildMask(TokenType left, TokenType right)
{
    OpId result = mask(left);
    return (result << 32) | mask(right);
}

Operation::Operation(const OpSignature & sig, OpFunc func)
    : m_mask(buildMask(sig.left, sig.right)),
      m_exec(func)
{
}

cparse::OpId Operation::getMask() const
{
    return m_mask;
}

PackToken Operation::exec(const PackToken & left, const PackToken & right, EvaluationData * data) const
{
    return m_exec(left, right, data);
}

/* * * * * rpnBuilder Class: * * * * */

void RpnBuilder::clearRPN(TokenQueue * rpn)
{
    while (!rpn->empty())
    {
        delete cparse::resolve_reference(rpn->front());
        rpn->pop();
    }
}

void RpnBuilder::clear()
{
    clearRPN(&m_rpn);
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
void RpnBuilder::handleOpStack(const QString & op)
{
    QString cur_op;

    // If it associates from left to right:
    if (m_opp.assoc(op) == 0)
    {
        while (!m_opStack.empty() &&
               m_opp.prec(op) >= m_opp.prec(m_opStack.top()))
        {
            cur_op = normalize_op(m_opStack.top());
            m_rpn.push(new TokenTyped<QString>(cur_op, OP));
            m_opStack.pop();
        }
    }
    else
    {
        while (!m_opStack.empty() &&
               m_opp.prec(op) > m_opp.prec(m_opStack.top()))
        {
            cur_op = normalize_op(m_opStack.top());
            m_rpn.push(new TokenTyped<QString>(cur_op, OP));
            m_opStack.pop();
        }
    }
}

void RpnBuilder::handleBinary(const QString & op)
{
    // Handle OP precedence
    handleOpStack(op);
    // Then push the current op into the stack:
    m_opStack.push(op);
}

// Convert left unary operators to binary and handle them:
void RpnBuilder::handleLeftUnary(const QString & unary_op)
{
    this->m_rpn.push(new TokenUnary());
    // Only put it on the stack and wait to check op precedence:
    m_opStack.push(unary_op);
}

// Convert right unary operators to binary and handle them:
void RpnBuilder::handleRightUnary(const QString & unary_op)
{
    // Handle OP precedence:
    handleOpStack(unary_op);
    // Add the unary token:
    this->m_rpn.push(new TokenUnary());
    // Then add the current op directly into the rpn:
    m_rpn.push(new TokenTyped<QString>(normalize_op(unary_op), OP));
}

// Find out if op is a binary or unary operator and handle it:
void RpnBuilder::processOpStack()
{
    while (!m_opStack.empty())
    {
        QString cur_op = normalize_op(m_opStack.top());
        m_rpn.push(new TokenTyped<QString>(cur_op, OP));
        m_opStack.pop();
    }

    // In case one of the custom parsers left an empty expression:
    if (m_rpn.empty())
    {
        m_rpn.push(new TokenNone());
    }
}

cparse::TokenType RpnBuilder::backType() const
{
    return m_rpn.back()->m_type;
}

void RpnBuilder::setBackType(TokenType type)
{
    m_rpn.back()->m_type = type;
}

QString RpnBuilder::topOp() const
{
    return m_opStack.top();
}

uint32_t RpnBuilder::bracketLevel() const
{
    return m_bracketLevel;
}

bool RpnBuilder::lastTokenWasOp() const
{
    return m_lastTokenWasOp;
}

bool RpnBuilder::lastTokenWasUnary() const
{
    return m_lastTokenWasUnary;
}

const cparse::TokenQueue & RpnBuilder::rpn() const
{
    return m_rpn;
}

bool RpnBuilder::opExists(const QString & op) const
{
    return m_opp.exists(op);
}

bool RpnBuilder::handleOp(const QString & op)
{
    // If it's a left unary operator:
    if (this->m_lastTokenWasOp)
    {
        if (m_opp.exists("L" + op))
        {
            handleLeftUnary("L" + op);
            this->m_lastTokenWasUnary = true;
            this->m_lastTokenWasOp = op[0].unicode();
        }
        else
        {
            clearRPN(&(this->m_rpn));
            qWarning(cparseLog) << "Unrecognized unary operator: '" << op << "'.";
            return false;
        }

        // If its a right unary operator:
    }
    else if (m_opp.exists("R" + op))
    {
        handleRightUnary("R" + op);

        // Set it to false, since we have already added
        // an unary token and operand to the stack:
        this->m_lastTokenWasUnary = false;
        this->m_lastTokenWasOp = false;

        // If it is a binary operator:
    }
    else
    {
        if (m_opp.exists(op))
        {
            handleBinary(op);
        }
        else
        {
            clearRPN(&(m_rpn));
            qWarning(cparseLog) << "Undefined operator: `" << op << "`!";
            return false;
        }

        this->m_lastTokenWasUnary = false;
        this->m_lastTokenWasOp = op[0].unicode();
    }

    return true;
}

bool RpnBuilder::handleToken(Token * token)
{
    if (!m_lastTokenWasOp)
    {
        qWarning(cparseLog) << "Expected an operator or bracket but got " << PackToken::str(token);
        delete token;
        return false;
    }

    m_rpn.push(token);
    m_lastTokenWasOp = false;
    m_lastTokenWasUnary = false;
    return true;
}

bool RpnBuilder::openBracket(const QString & bracket)
{
    m_opStack.push(bracket);
    m_lastTokenWasOp = bracket[0].unicode();
    m_lastTokenWasUnary = false;
    ++m_bracketLevel;
    return true;
}

bool RpnBuilder::closeBracket(const QString & bracket)
{
    if (char(m_lastTokenWasOp) == bracket[0])
    {
        m_rpn.push(new Tuple());
    }

    QString cur_op;

    while (!m_opStack.empty() && m_opStack.top() != bracket)
    {
        cur_op = normalize_op(m_opStack.top());
        m_rpn.push(new TokenTyped<QString>(cur_op, OP));
        m_opStack.pop();
    }

    if (m_opStack.empty())
    {
        RpnBuilder::clearRPN(&m_rpn);
        qWarning(cparseLog) << "Extra '" + bracket + "' on the expression!";
        return false;
    }

    m_opStack.pop();
    m_lastTokenWasOp = false;
    m_lastTokenWasUnary = false;
    --m_bracketLevel;
    return true;
}

bool RpnBuilder::isvarchar(const char c)
{
    return isalpha(c) || c == '_';
}

QString RpnBuilder::parseVar(const char * expr, const char ** rest)
{
    QString ss;
    ss += *expr;
    ++expr;

    while (RpnBuilder::isvarchar(*expr) || isdigit(*expr))
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

void cleanStack(std::stack<Token *> st)
{
    while (!st.empty())
    {
        delete resolve_reference(st.top());
        st.pop();
    }
}

cparse::OpPrecedenceMap::OpPrecedenceMap()
{
    // These operations are hard-coded inside the Calculator,
    // thus their precedence should always be defined:
    m_prMap["[]"] = -1;
    m_prMap["()"] = -1;
    m_prMap["["] = 0x7FFFFFFF;
    m_prMap["("] = 0x7FFFFFFF;
    m_prMap["{"] = 0x7FFFFFFF;
    m_rtol.insert("=");
}

void cparse::OpPrecedenceMap::add(const QString & op, int precedence)
{
    if (precedence < 0)
    {
        m_rtol.insert(op);
        precedence = -precedence;
    }

    m_prMap[op] = precedence;
}

void cparse::OpPrecedenceMap::addUnary(const QString & op, int precedence)
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

void cparse::OpPrecedenceMap::addRightUnary(const QString & op, int precedence)
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

int cparse::OpPrecedenceMap::prec(const QString & op) const
{
    return m_prMap.at(op);
}

bool cparse::OpPrecedenceMap::assoc(const QString & op) const
{
    return m_rtol.count(op);
}

bool cparse::OpPrecedenceMap::exists(const QString & op) const
{
    return m_prMap.count(op);
}

EvaluationData::EvaluationData(TokenQueue rpn, const TokenMap & scope, const OpMap & opMap)
    : rpn(std::move(rpn)), scope(scope), opMap(opMap) {}

void cparse::ParserMap::add(const QString & word, WordParserFunc * parser)
{
    wmap[word] = parser;
}

void cparse::ParserMap::add(char c, WordParserFunc * parser)
{
    cmap[c] = parser;
}

cparse::WordParserFunc * cparse::ParserMap::find(const QString & text)
{
    WordParserFuncMap::iterator w_it;

    if ((w_it = wmap.find(text)) != wmap.end())
    {
        return w_it->second;
    }

    return nullptr;
}

cparse::WordParserFunc * cparse::ParserMap::find(char c)
{
    CharParserFuncMap::iterator c_it;

    if ((c_it = cmap.find(c)) != cmap.end())
    {
        return c_it->second;
    }

    return nullptr;
}

cparse::OpSignature::OpSignature(const TokenType L, const QString & op, const TokenType R)
    : left(L), op(op), right(R) {}

void cparse::OpMap::add(const OpSignature & sig, Operation::OpFunc func)
{
    (*this)[sig.op].push_back(Operation(sig, func));
}

QString cparse::OpMap::str() const
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

Config::Config(ParserMap p, OpPrecedenceMap opp, OpMap opMap)
    : parserMap(std::move(p)), opPrecedence(std::move(opp)), opMap(std::move(opMap)) {}
