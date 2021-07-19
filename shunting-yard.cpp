#include "./shunting-yard.h"
#include "./shunting-yard-exceptions.h"

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

using cparse::Calculator;
using cparse::PackToken;
using cparse::TokenBase;
using cparse::TokenMap;
using cparse::RefToken;
using cparse::Operation;
using cparse::opID_t;
using cparse::Config_t;
using cparse::typeMap_t;
using cparse::TokenQueue_t;
using cparse::evaluationData;
using cparse::rpnBuilder;
using cparse::REF;

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

/* * * * * Utility functions: * * * * */

bool match_op_id(opID_t id, opID_t mask)
{
    uint64_t result = id & mask;
    auto * val = reinterpret_cast<uint32_t *>(&result);
    return (val[0] && val[1]);
}

TokenBase * exec_operation(const PackToken & left, const PackToken & right,
                           evaluationData * data, const QString & OP_MASK)
{
    auto it = data->opMap.find(OP_MASK);

    if (it == data->opMap.end())
    {
        return nullptr;
    }

    for (const Operation & operation : it->second)
    {
        if (match_op_id(data->opID, operation.getMask()))
        {
            try
            {
                return operation.exec(left, right, data).release();
            }
            catch (const Operation::Reject &)
            {
                continue;
            }
        }
    }

    return nullptr;
}

inline QString normalize_op(QString op)
{
    if (op[0] == 'L' || op[0] == 'R')
    {
        op.remove(0, 1);
        return op;
    }

    return op;
}

// Use this function to discard a reference to an object
// And obtain the original TokenBase*.
// Please note that it only deletes memory if the token
// is of type REF.
TokenBase * resolve_reference(TokenBase * b, TokenMap * scope = nullptr)
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

/* * * * * Static containers: * * * * */

// Build configurations once only:
Config_t & Calculator::Default()
{
    static Config_t conf;
    return conf;
}

typeMap_t & Calculator::type_attribute_map()
{
    static typeMap_t type_map;
    return type_map;
}

/* * * * * rpnBuilder Class: * * * * */

void rpnBuilder::cleanRPN(TokenQueue_t * rpn)
{
    while (!rpn->empty())
    {
        delete resolve_reference(rpn->front());
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

/* * * * * RAII_TokenQueue_t struct  * * * * */

// Used to make sure an rpn is dealloc'd correctly
// even when an exception is thrown.
//
// Note: This is needed because C++ does not
// allow a try-finally block.
struct Calculator::RAII_TokenQueue_t : TokenQueue_t
{
    RAII_TokenQueue_t() {}
    RAII_TokenQueue_t(const TokenQueue_t & rpn) : TokenQueue_t(rpn) {}
    ~RAII_TokenQueue_t()
    {
        rpnBuilder::cleanRPN(this);
    }

    RAII_TokenQueue_t(const RAII_TokenQueue_t & rpn)
        : TokenQueue_t(rpn)
    {
        throw std::runtime_error("You should not copy this class!");
    }
    RAII_TokenQueue_t & operator=(const RAII_TokenQueue_t &)
    {
        throw std::runtime_error("You should not copy this class!");
    }
};

/* * * * * Calculator class * * * * */

TokenQueue_t Calculator::toRPN(const char * expr,
                               TokenMap vars, const char * delim,
                               const char ** rest, Config_t config)
{
    rpnBuilder data(vars, config.opPrecedence);
    char * nextChar = nullptr;

    static char c = '\0';

    if (!delim)
    {
        delim = &c;
    }

    while (*expr && isspace(*expr) && !strchr(delim, *expr))
    {
        ++expr;
    }

    if (*expr == '\0' || strchr(delim, *expr))
    {
        throw std::invalid_argument("Cannot build a Calculator from an empty expression!");
    }

    // In one pass, ignore whitespace and parse the expression into RPN
    // using Dijkstra's Shunting-yard algorithm.
    while (*expr && (data.bracketLevel || !strchr(delim, *expr)))
    {
        if (isdigit(*expr))
        {
            int base = 10;

            // Parse the prefix notation for octal and hex numbers:
            if (expr[0] == '0')
            {
                if (expr[1] == 'x')
                {
                    // 0x1 == 1 in hex notation
                    base = 16;
                    expr += 2;
                }
                else if (isdigit(expr[1]))
                {
                    // 01 == 1 in octal notation
                    base = 8;
                    expr++;
                }
            }

            // If the token is a number, add it to the output queue.
            int64_t _int = strtoll(expr, &nextChar, base);

            // If the number was not a float:
            if (base != 10 || !strchr(".eE", *nextChar))
            {
                data.handle_token(new Token<int64_t>(_int, INT));
            }
            else
            {
                double digit = strtod(expr, &nextChar);
                data.handle_token(new Token<double>(digit, REAL));
            }

            expr = nextChar;
        }
        else if (rpnBuilder::isvarchar(*expr))
        {
            rWordParser_t * parser;

            // If the token is a variable, resolve it and
            // add the parsed number to the output queue.
            QString key = rpnBuilder::parseVar(expr, &expr);

            if ((parser = config.parserMap.find(key)))
            {
                // Parse reserved words:
                try
                {
                    parser(expr, &expr, &data);
                }
                catch (...)
                {
                    rpnBuilder::cleanRPN(&data.rpn);
                    throw;
                }
            }
            else
            {
                PackToken * value = vars.find(key);

                if (value)
                {
                    // Save a reference token:
                    TokenBase * copy = (*value)->clone();
                    data.handle_token(new RefToken(PackToken(key), copy));
                }
                else
                {
                    // Save the variable name:
                    data.handle_token(new Token<QString>(key, VAR));
                }
            }
        }
        else if (*expr == '\'' || *expr == '"')
        {
            // If it is a string literal, parse it and
            // add to the output queue.
            char quote = *expr;

            ++expr;
            QString ss;

            while (*expr && *expr != quote && *expr != '\n')
            {
                if (*expr == '\\')
                {
                    switch (expr[1])
                    {
                        case 'n':
                            expr += 2;
                            ss += '\n';
                            break;

                        case 't':
                            expr += 2;
                            ss += '\t';
                            break;

                        default:
                            if (strchr("\"'\n", expr[1]))
                            {
                                ++expr;
                            }

                            ss += *expr;
                            ++expr;
                    }
                }
                else
                {
                    ss += *expr;
                    ++expr;
                }
            }

            if (*expr != quote)
            {
                QString squote = (quote == '"' ? "\"" : "'");
                rpnBuilder::cleanRPN(&data.rpn);
                throw syntax_error("Expected quote (" + squote + ") at end of string declaration: " + squote + ss + ".");
            }

            ++expr;
            data.handle_token(new Token<QString>(ss, STR));
        }
        else
        {
            // Otherwise, the variable is an operator or paranthesis.
            switch (*expr)
            {
                case '(':

                    // If it is a function call:
                    if (!data.lastTokenWasOp)
                    {
                        // This counts as a bracket and as an operator:
                        data.handle_op("()");
                        // Add it as a bracket to the op stack:
                    }

                    data.open_bracket("(");
                    ++expr;
                    break;

                case '[':
                    if (!data.lastTokenWasOp)
                    {
                        // If it is an operator:
                        data.handle_op("[]");
                    }
                    else
                    {
                        // If it is the list constructor:
                        // Add the list constructor to the rpn:
                        data.handle_token(new CppFunction(&TokenList::default_constructor, "list"));

                        // We make the program see it as a normal function call:
                        data.handle_op("()");
                    }

                    // Add it as a bracket to the op stack:
                    data.open_bracket("[");
                    ++expr;
                    break;

                case '{':
                    // Add a map constructor call to the rpn:
                    data.handle_token(new CppFunction(&TokenMap::default_constructor, "map"));

                    // We make the program see it as a normal function call:
                    data.handle_op("()");
                    data.open_bracket("{");
                    ++expr;
                    break;

                case ')':
                    data.close_bracket("(");
                    ++expr;
                    break;

                case ']':
                    data.close_bracket("[");
                    ++expr;
                    break;

                case '}':
                    data.close_bracket("{");
                    ++expr;
                    break;

                default:
                {
                    // Then the token is an operator

                    const char * start = expr;
                    QString ss;
                    ss += *expr;
                    ++expr;

                    while (*expr && ispunct(*expr) && !strchr("+-'\"()[]{}_", *expr))
                    {
                        ss += *expr;
                        ++expr;
                    }

                    QString op = ss;

                    // Check if the word parser applies:
                    rWordParser_t * parser = config.parserMap.find(op);

                    // Evaluate the meaning of this operator in the following order:
                    // 1. Is there a word parser for it?
                    // 2. Is it a valid operator?
                    // 3. Is there a character parser for its first character?
                    if (parser)
                    {
                        // Parse reserved operators:
                        try
                        {
                            parser(expr, &expr, &data);
                        }
                        catch (...)
                        {
                            rpnBuilder::cleanRPN(&data.rpn);
                            throw;
                        }
                    }
                    else if (data.opp.exists(op))
                    {
                        data.handle_op(op);
                    }
                    else if ((parser = config.parserMap.find(QString(op[0]))))
                    {
                        expr = start + 1;

                        try
                        {
                            parser(expr, &expr, &data);
                        }
                        catch (...)
                        {
                            rpnBuilder::cleanRPN(&data.rpn);
                            throw;
                        }
                    }
                    else
                    {
                        rpnBuilder::cleanRPN(&data.rpn);
                        throw syntax_error("Invalid operator: " + op);
                    }
                }
            }
        }

        // Ignore spaces but stop on delimiter if not inside brackets.
        while (*expr && isspace(*expr)
               && (data.bracketLevel || !strchr(delim, *expr)))
        {
            ++expr;
        }
    }

    // Check for syntax errors (excess of operators i.e. 10 + + -1):
    if (data.lastTokenWasUnary)
    {
        rpnBuilder::cleanRPN(&data.rpn);
        throw syntax_error("Expected operand after unary operator `" + data.opStack.top() + "`");
    }

    QString cur_op;

    while (!data.opStack.empty())
    {
        cur_op = normalize_op(data.opStack.top());
        data.rpn.push(new Token<QString>(cur_op, OP));
        data.opStack.pop();
    }

    // In case one of the custom parsers left an empty expression:
    if (data.rpn.empty())
    {
        data.rpn.push(new TokenNone());
    }

    if (rest)
    {
        *rest = expr;
    }

    return data.rpn;
}

PackToken Calculator::calculate(const char * expr, const TokenMap & vars,
                                const char * delim, const char ** rest)
{
    // Convert to RPN with Dijkstra's Shunting-yard algorithm.
    RAII_TokenQueue_t rpn = Calculator::toRPN(expr, vars, delim, rest);

    TokenBase * ret = Calculator::calculate(rpn, vars);

    return PackToken(resolve_reference(ret));
}

void cleanStack(std::stack<TokenBase *> st)
{
    while (!st.empty())
    {
        delete resolve_reference(st.top());
        st.pop();
    }
}

TokenBase * Calculator::calculate(const TokenQueue_t & rpn, const TokenMap & scope,
                                  const Config_t & config)
{
    evaluationData data(rpn, scope, config.opMap);

    // Evaluate the expression in RPN form.
    std::stack<TokenBase *> evaluation;

    while (!data.rpn.empty())
    {
        TokenBase * base = data.rpn.front()->clone();
        data.rpn.pop();

        // Operator:
        if (base->type == OP)
        {
            data.op = static_cast<Token<QString>*>(base)->val;
            delete base;

            /* * * * * Resolve operands Values and References: * * * * */

            if (evaluation.size() < 2)
            {
                cleanStack(evaluation);
                throw std::domain_error("Invalid equation.");
            }

            TokenBase * r_token = evaluation.top();
            evaluation.pop();
            TokenBase * l_token = evaluation.top();
            evaluation.pop();

            if (r_token->type & REF)
            {
                data.right.reset(static_cast<RefToken *>(r_token));
                r_token = data.right->resolve(&data.scope);
            }
            else if (r_token->type == VAR)
            {
                auto key = PackToken(static_cast<Token<QString>*>(r_token)->val);
                data.right = std::make_unique<RefToken>(key);
            }
            else
            {
                data.right = std::make_unique<RefToken>();
            }

            if (l_token->type & REF)
            {
                data.left.reset(static_cast<RefToken *>(l_token));
                l_token = data.left->resolve(&data.scope);
            }
            else if (l_token->type == VAR)
            {
                auto key = PackToken(static_cast<Token<QString>*>(l_token)->val);
                data.left = std::make_unique<RefToken>(key);
            }
            else
            {
                data.left = std::make_unique<RefToken>();
            }

            if (l_token->type == FUNC && data.op == "()")
            {
                // * * * * * Resolve Function Calls: * * * * * //

                auto * l_func = static_cast<Function *>(l_token);

                // Collect the parameter tuple:
                Tuple right;

                if (r_token->type == TUPLE)
                {
                    right = *static_cast<Tuple *>(r_token);
                }
                else
                {
                    right = Tuple(r_token);
                }

                delete r_token;

                PackToken _this;

                if (data.left->origin->type != NONE)
                {
                    _this = data.left->origin;
                }
                else
                {
                    _this = data.scope;
                }

                // Execute the function:
                PackToken ret;

                try
                {
                    ret = Function::call(_this, l_func, &right, data.scope);
                }
                catch (...)
                {
                    cleanStack(evaluation);
                    delete l_func;
                    throw;
                }

                delete l_func;
                evaluation.push(ret->clone());
            }
            else
            {
                // * * * * * Resolve All Other Operations: * * * * * //

                data.opID = Operation::build_mask(l_token->type, r_token->type);
                PackToken l_pack(l_token);
                PackToken r_pack(r_token);
                TokenBase * result = nullptr;

                try
                {
                    // Resolve the operation:
                    result = exec_operation(l_pack, r_pack, &data, data.op);

                    if (!result)
                    {
                        result = exec_operation(l_pack, r_pack, &data, "");
                    }
                }
                catch (...)
                {
                    cleanStack(evaluation);
                    throw;
                }

                if (result)
                {
                    evaluation.push(result);
                }
                else
                {
                    cleanStack(evaluation);
                    throw undefined_operation(data.op, l_pack, r_pack);
                }
            }
        }
        else if (base->type == VAR)      // Variable
        {
            PackToken * value = nullptr;
            QString key = static_cast<Token<QString>*>(base)->val;

            value = data.scope.find(key);

            if (value)
            {
                TokenBase * copy = (*value)->clone();
                evaluation.push(new RefToken(PackToken(key), copy));
                delete base;
            }
            else
            {
                evaluation.push(base);
            }
        }
        else
        {
            evaluation.push(base);
        }
    }

    return evaluation.top();
}

/* * * * * Non Static Functions * * * * */

Calculator::~Calculator()
{
    rpnBuilder::cleanRPN(&this->RPN);
}

Calculator::Calculator()
{
    this->RPN.push(new TokenNone());
}

Calculator::Calculator(const Calculator & calc)
{
    TokenQueue_t _rpn = calc.RPN;

    // Deep copy the token list, so everything can be
    // safely deallocated:
    while (!_rpn.empty())
    {
        TokenBase * base = _rpn.front();
        _rpn.pop();
        this->RPN.push(base->clone());
    }
}

// Work as a sub-parser:
// - Stops at delim or '\0'
// - Returns the rest of the string as char* rest
Calculator::Calculator(const char * expr, const TokenMap & vars, const char * delim,
                       const char ** rest, const Config_t & config)
{
    this->RPN = Calculator::toRPN(expr, vars, delim, rest, config);
}

void Calculator::compile(const char * expr, const TokenMap & vars, const char * delim,
                         const char ** rest)
{
    // Make sure it is empty:
    rpnBuilder::cleanRPN(&this->RPN);

    this->RPN = Calculator::toRPN(expr, vars, delim, rest, Config());
}

PackToken Calculator::eval(const TokenMap & vars, bool keep_refs) const
{
    TokenBase * value = calculate(this->RPN, vars, Config());
    PackToken p = PackToken(value->clone());

    if (keep_refs)
    {
        return PackToken(value);
    }

    return PackToken(resolve_reference(value));
}

Calculator & Calculator::operator=(const Calculator & calc)
{
    // Make sure the RPN is empty:
    rpnBuilder::cleanRPN(&this->RPN);

    // Deep copy the token list, so everything can be
    // safely deallocated:
    TokenQueue_t _rpn = calc.RPN;

    while (!_rpn.empty())
    {
        TokenBase * base = _rpn.front();
        _rpn.pop();
        this->RPN.push(base->clone());
    }

    return *this;
}

/* * * * * For Debug Only * * * * */

QString Calculator::str() const
{
    return str(this->RPN);
}

QString Calculator::str(TokenQueue_t rpn)
{
    QString ss;

    ss += "Calculator { RPN: [ ";

    while (!rpn.empty())
    {
        ss += PackToken(resolve_reference(rpn.front()->clone())).str();
        rpn.pop();

        ss += (!rpn.empty() ? ", " : "");
    }

    ss += " ] }";
    return ss;
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
