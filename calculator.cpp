#include "calculator.h"

#include "rpnbuilder.h"
#include "exceptions.h"
#include "tokenhelpers.h"
#include "packtoken.h"
#include "reftoken.h"

namespace
{
    using namespace cparse;

    bool match_op_id(OpId id, OpId mask)
    {
        uint64_t result = id & mask;
        auto * val = reinterpret_cast<uint32_t *>(&result);
        return (val[0] && val[1]);
    }

    Token * exec_operation(const PackToken & left, const PackToken & right,
                           EvaluationData * data, const QString & OP_MASK)
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

    /* * * * * RAII_TokenQueue_t struct  * * * * */

    // Used to make sure an rpn is dealloc'd correctly
    // even when an exception is thrown.
    //
    // Note: This is needed because C++ does not
    // allow a try-finally block.
    struct RaiiTokenQueue : TokenQueue
    {
        RaiiTokenQueue() {}
        RaiiTokenQueue(const TokenQueue & rpn) : TokenQueue(rpn) {}
        ~RaiiTokenQueue()
        {
            RpnBuilder::clearRPN(this);
        }

        RaiiTokenQueue(const RaiiTokenQueue & rpn)
            : TokenQueue(rpn)
        {
            throw std::runtime_error("You should not copy this class!");
        }
        RaiiTokenQueue & operator=(const RaiiTokenQueue &)
        {
            throw std::runtime_error("You should not copy this class!");
        }
    };
}

Config & Calculator::defaultConfig()
{
    static Config conf;
    return conf;
}

const Config & Calculator::config() const
{
    return defaultConfig();
}

TokenTypeMap & Calculator::typeAttributeMap()
{
    static TokenTypeMap type_map;
    return type_map;
}

Calculator::Calculator()
{
    this->m_rpn.push(new TokenNone());
}

Calculator::Calculator(const Calculator & calc)
{
    TokenQueue _rpn = calc.m_rpn;

    // Deep copy the token list, so everything can be
    // safely deallocated:
    while (!_rpn.empty())
    {
        Token * base = _rpn.front();
        _rpn.pop();
        this->m_rpn.push(base->clone());
    }
}

Calculator::Calculator(const QString & expr, const TokenMap & vars,
                       const QString & delim, int * rest, const Config & config)
{
    this->m_rpn = Calculator::toRPN(expr, vars, delim, rest, config);
}

Calculator::~Calculator()
{
    RpnBuilder::clearRPN(&this->m_rpn);
}

PackToken Calculator::calculate(const QString & expr, const TokenMap & vars,
                                const QString & delim, int * rest)
{
    RaiiTokenQueue rpn = Calculator::toRPN(expr, vars, delim, rest);
    Token * ret = Calculator::calculate(rpn, vars);
    return PackToken(resolve_reference(ret));
}

Token * Calculator::calculate(const TokenQueue & rpn, const TokenMap & scope,
                              const Config & config)
{
    EvaluationData data(rpn, scope, config.opMap);

    // Evaluate the expression in RPN form.
    std::stack<Token *> evaluation;

    while (!data.rpn.empty())
    {
        Token * base = data.rpn.front()->clone();
        data.rpn.pop();

        // Operator:
        if (base->m_type == OP)
        {
            data.op = static_cast<TokenTyped<QString>*>(base)->m_val;
            delete base;

            /* * * * * Resolve operands Values and References: * * * * */

            if (evaluation.size() < 2)
            {
                cleanStack(evaluation);
                throw std::domain_error("Invalid equation.");
            }

            Token * r_token = evaluation.top();
            evaluation.pop();
            Token * l_token = evaluation.top();
            evaluation.pop();

            if (r_token->m_type & REF)
            {
                data.right.reset(static_cast<RefToken *>(r_token));
                r_token = data.right->resolve(&data.scope);
            }
            else if (r_token->m_type == VAR)
            {
                auto key = PackToken(static_cast<TokenTyped<QString>*>(r_token)->m_val);
                data.right = std::make_unique<RefToken>(key);
            }
            else
            {
                data.right = std::make_unique<RefToken>();
            }

            if (l_token->m_type & REF)
            {
                data.left.reset(static_cast<RefToken *>(l_token));
                l_token = data.left->resolve(&data.scope);
            }
            else if (l_token->m_type == VAR)
            {
                auto key = PackToken(static_cast<TokenTyped<QString>*>(l_token)->m_val);
                data.left = std::make_unique<RefToken>(key);
            }
            else
            {
                data.left = std::make_unique<RefToken>();
            }

            if (l_token->m_type == FUNC && data.op == "()")
            {
                // * * * * * Resolve Function Calls: * * * * * //

                auto * l_func = static_cast<Function *>(l_token);

                // Collect the parameter tuple:
                Tuple right;

                if (r_token->m_type == TUPLE)
                {
                    right = *static_cast<Tuple *>(r_token);
                }
                else
                {
                    right = Tuple(r_token);
                }

                delete r_token;

                PackToken _this;

                if (data.left->m_origin->m_type != NONE)
                {
                    _this = data.left->m_origin;
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

                data.opID = Operation::buildMask(l_token->m_type, r_token->m_type);
                PackToken l_pack(l_token);
                PackToken r_pack(r_token);
                Token * result = nullptr;

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
        else if (base->m_type == VAR)      // Variable
        {
            PackToken * value = nullptr;
            QString key = static_cast<TokenTyped<QString>*>(base)->m_val;

            value = data.scope.find(key);

            if (value)
            {
                Token * copy = (*value)->clone();
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

void Calculator::compile(const QString & expr, const TokenMap & vars,
                         const QString & delim, int * rest)
{
    // Make sure it is empty:
    RpnBuilder::clearRPN(&this->m_rpn);
    this->m_rpn = Calculator::toRPN(expr, vars, delim, rest, config());
}

PackToken Calculator::eval(const TokenMap & vars, bool keep_refs) const
{
    Token * value = calculate(this->m_rpn, vars, config());
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
    RpnBuilder::clearRPN(&this->m_rpn);

    // Deep copy the token list, so everything can be
    // safely deallocated:
    TokenQueue _rpn = calc.m_rpn;

    while (!_rpn.empty())
    {
        Token * base = _rpn.front();
        _rpn.pop();
        this->m_rpn.push(base->clone());
    }

    return *this;
}

/* * * * * For Debug Only * * * * */

QString Calculator::str() const
{
    return str(this->m_rpn);
}

QString Calculator::str(TokenQueue rpn)
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

// Work as a sub-parser:
// - Stops at delim or '\0'
// - Returns the rest of the string as char* rest
TokenQueue Calculator::toRPN(const QString & exprStr, TokenMap vars,
                             const QString & delimStr, int * rest, Config config)
{
    RpnBuilder data(vars, config.opPrecedence);
    char * nextChar = nullptr;

    std::string exprStd = exprStr.toStdString();
    std::string delimStd = delimStr.toStdString();

    const char * expr = exprStd.c_str();
    const char * delim = delimStd.c_str();

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
    while (*expr && (data.bracketLevel() || !strchr(delim, *expr)))
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
                data.handleToken(new TokenTyped<int64_t>(_int, INT));
            }
            else
            {
                double digit = strtod(expr, &nextChar);
                data.handleToken(new TokenTyped<double>(digit, REAL));
            }

            expr = nextChar;
        }
        else if (RpnBuilder::isvarchar(*expr))
        {
            WordParserFunc * parser;

            // If the token is a variable, resolve it and
            // add the parsed number to the output queue.
            QString key = RpnBuilder::parseVar(expr, &expr);

            if ((parser = config.parserMap.find(key)))
            {
                // Parse reserved words:
                try
                {
                    parser(expr, &expr, &data);
                }
                catch (...)
                {
                    data.clear();
                    throw;
                }
            }
            else
            {
                PackToken * value = vars.find(key);

                if (value)
                {
                    // Save a reference token:
                    Token * copy = (*value)->clone();
                    data.handleToken(new RefToken(PackToken(key), copy));
                }
                else
                {
                    // Save the variable name:
                    data.handleToken(new TokenTyped<QString>(key, VAR));
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
                data.clear();
                throw syntax_error("Expected quote (" + squote + ") at end of string declaration: " + squote + ss + ".");
            }

            ++expr;
            data.handleToken(new TokenTyped<QString>(ss, STR));
        }
        else
        {
            // Otherwise, the variable is an operator or paranthesis.
            switch (*expr)
            {
                case '(':

                    // If it is a function call:
                    if (!data.lastTokenWasOp())
                    {
                        // This counts as a bracket and as an operator:
                        data.handleOp("()");
                        // Add it as a bracket to the op stack:
                    }

                    data.openBracket("(");
                    ++expr;
                    break;

                case '[':
                    if (!data.lastTokenWasOp())
                    {
                        // If it is an operator:
                        data.handleOp("[]");
                    }
                    else
                    {
                        // If it is the list constructor:
                        // Add the list constructor to the rpn:
                        data.handleToken(new CppFunction(&TokenList::default_constructor, "list"));

                        // We make the program see it as a normal function call:
                        data.handleOp("()");
                    }

                    // Add it as a bracket to the op stack:
                    data.openBracket("[");
                    ++expr;
                    break;

                case '{':
                    // Add a map constructor call to the rpn:
                    data.handleToken(new CppFunction(&TokenMap::default_constructor, "map"));

                    // We make the program see it as a normal function call:
                    data.handleOp("()");
                    data.openBracket("{");
                    ++expr;
                    break;

                case ')':
                    data.closeBracket("(");
                    ++expr;
                    break;

                case ']':
                    data.closeBracket("[");
                    ++expr;
                    break;

                case '}':
                    data.closeBracket("{");
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
                    auto * parser = config.parserMap.find(op);

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
                            data.clear();
                            throw;
                        }
                    }
                    else if (data.opExists(op))
                    {
                        data.handleOp(op);
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
                            data.clear();
                            throw;
                        }
                    }
                    else
                    {
                        data.clear();
                        throw syntax_error("Invalid operator: " + op);
                    }
                }
            }
        }

        // Ignore spaces but stop on delimiter if not inside brackets.
        while (*expr && isspace(*expr)
               && (data.bracketLevel() || !strchr(delim, *expr)))
        {
            ++expr;
        }
    }

    // Check for syntax errors (excess of operators i.e. 10 + + -1):
    if (data.lastTokenWasUnary())
    {
        data.clear();
        throw syntax_error("Expected operand after unary operator `" + data.topOp() + "`");
    }

    data.processOpStack();

    if (rest)
    {
        *rest = expr - exprStd.c_str();
    }

    return data.rpn();
}

QDebug & operator<<(QDebug & os, const cparse::Calculator & t)
{
    return os << t.str();
}
