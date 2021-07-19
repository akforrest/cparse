#include "calculator.h"

#include "shunting-yard.h"
#include "shunting-yard-exceptions.h"
#include "tokenhelpers.h"
#include "packtoken.h"

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

PackToken Calculator::calculate(const char * expr, const TokenMap & vars,
                                const char * delim, const char ** rest)
{
    // Convert to RPN with Dijkstra's Shunting-yard algorithm.
    RAII_TokenQueue_t rpn = Calculator::toRPN(expr, vars, delim, rest);

    TokenBase * ret = Calculator::calculate(rpn, vars);

    return PackToken(resolve_reference(ret));
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
