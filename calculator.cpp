#include "calculator.h"

#include "cparse.h"
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
        quint64 result = id & mask;
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
                auto * execToken = operation.exec(left, right, data).release();

                if (execToken->m_type == TokenType::REJECT)
                {
                    continue;
                }

                return execToken;
            }
        }

        return nullptr;
    }

    /* * * * * RAII_TokenQueue_t struct  * * * * */

    // Used to make sure an rpn is dealloc'd correctly
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
            Q_ASSERT_X(false, "RaiiTokenQueue", "You should not copy this class!");
        }
        RaiiTokenQueue & operator=(const RaiiTokenQueue &)
        {
            Q_ASSERT_X(false, "RaiiTokenQueue", "You should not copy this class!");
            return *this;
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
    m_rpn = Calculator::toRPN(expr, vars, delim, rest, config);
    m_compiled = !m_rpn.empty();
}

Calculator::~Calculator()
{
    RpnBuilder::clearRPN(&this->m_rpn);
}

bool Calculator::compiled() const
{
    return m_compiled;
}

PackToken Calculator::calculate(const QString & expr, const TokenMap & vars,
                                const QString & delim, int * rest)
{
    RaiiTokenQueue rpn = Calculator::toRPN(expr, vars, delim, rest);

    if (rpn.empty())
    {
        return PackToken::Error();
    }

    Token * ret = Calculator::calculate(rpn, vars);

    if (ret == nullptr)
    {
        return PackToken::Error();
    }

    return PackToken(resolve_reference(ret));
}

Token * Calculator::calculate(const TokenQueue & rpn, const TokenMap & scope,
                              const Config & config)
{
    if (rpn.empty())
    {
        return nullptr;
    }

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
                qWarning(cparseLog) << "Invalid equation.";
                return nullptr;
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
                PackToken ret = Function::call(_this, l_func, &right, data.scope);

                if (ret->m_type == TokenType::ERROR)
                {
                    cleanStack(evaluation);
                    delete l_func;
                    return nullptr;
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

                // Resolve the operation:
                Token * result = exec_operation(l_pack, r_pack, &data, data.op);

                if (!result)
                {
                    result = exec_operation(l_pack, r_pack, &data, "");
                }

                if (result)
                {
                    if (result->m_type == TokenType::ERROR)
                    {
                        cleanStack(evaluation);
                        return nullptr;
                    }

                    evaluation.push(result);
                }
                else
                {
                    cleanStack(evaluation);
                    log_undefined_operation(data.op, l_pack, r_pack);
                    return nullptr;
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

bool Calculator::compile(const QString & expr, const TokenMap & vars,
                         const QString & delim, int * rest)
{
    // Make sure it is empty:
    RpnBuilder::clearRPN(&this->m_rpn);
    m_rpn = Calculator::toRPN(expr, vars, delim, rest, config());
    m_compiled = !m_rpn.empty();
    return this->compiled();
}

PackToken Calculator::eval(const TokenMap & vars, bool keep_refs) const
{
    Token * value = calculate(this->m_rpn, vars, config());

    if (value == nullptr)
    {
        return PackToken::Error();
    }

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
        qWarning(cparseLog) << "Cannot build a Calculator from an empty expression!";
        return {};
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
            qint64 _int = strtoll(expr, &nextChar, base);

            // If the number was not a float:
            if (base != 10 || !strchr(".eE", *nextChar))
            {
                if (!data.handleToken(new TokenTyped<qint64>(_int, INT)))
                {
                    return {};
                }
            }
            else
            {
                qreal digit = strtod(expr, &nextChar);

                if (!data.handleToken(new TokenTyped<qreal>(digit, REAL)))
                {
                    return {};
                }
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
                if (!parser(expr, &expr, &data))
                {
                    data.clear();
                    return {};
                }
            }
            else
            {
                PackToken * value = vars.find(key);

                if (value)
                {
                    // Save a reference token:
                    Token * copy = (*value)->clone();

                    if (!data.handleToken(new RefToken(PackToken(key), copy)))
                    {
                        return {};
                    }
                }
                else
                {
                    // Save the variable name:
                    if (!data.handleToken(new TokenTyped<QString>(key, VAR)))
                    {
                        return {};
                    }
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
                qWarning(cparseLog) << "Expected quote (" + squote + ") at end of string declaration: " + squote + ss + ".";
                return {};
            }

            ++expr;

            if (!data.handleToken(new TokenTyped<QString>(ss, STR)))
            {
                return {};
            }
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
                        if (!data.handleOp("()"))
                        {
                            return {};
                        }

                        // Add it as a bracket to the op stack:
                    }

                    data.openBracket("(");
                    ++expr;
                    break;

                case '[':
                    if (!data.lastTokenWasOp())
                    {
                        // If it is an operator:
                        if (!data.handleOp("[]"))
                        {
                            return {};
                        }
                    }
                    else
                    {
                        // If it is the list constructor:
                        // Add the list constructor to the rpn:
                        if (!data.handleToken(new CppFunction(&TokenList::default_constructor, "list")))
                        {
                            return {};
                        }

                        // We make the program see it as a normal function call:
                        if (!data.handleOp("()"))
                        {
                            return {};
                        }
                    }

                    // Add it as a bracket to the op stack:
                    data.openBracket("[");
                    ++expr;
                    break;

                case '{':

                    // Add a map constructor call to the rpn:
                    if (!data.handleToken(new CppFunction(&TokenMap::default_constructor, "map")))
                    {
                        return {};
                    }

                    // We make the program see it as a normal function call:
                    if (!data.handleOp("()"))
                    {
                        return {};
                    }

                    if (!data.openBracket("{"))
                    {
                        return {};
                    }

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

                        if (!parser(expr, &expr, &data))
                        {
                            data.clear();
                            return {};
                        }
                    }
                    else if (data.opExists(op))
                    {
                        if (!data.handleOp(op))
                        {
                            return {};
                        }
                    }
                    else if ((parser = config.parserMap.find(QString(op[0]))))
                    {
                        expr = start + 1;

                        if (!parser(expr, &expr, &data))
                        {
                            data.clear();
                            return {};
                        }
                    }
                    else
                    {
                        data.clear();
                        qWarning(cparseLog) << "Invalid operator: " + op;
                        return {};
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
        qWarning(cparseLog) << "Expected operand after unary operator `" << data.topOp() << "`";
        return {};
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
