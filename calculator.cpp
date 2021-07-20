#include "calculator.h"

#include "cparse.h"
#include "rpnbuilder.h"
#include "tokenhelpers.h"
#include "packtoken.h"
#include "reftoken.h"
#include "operation.h"

namespace
{
    using namespace cparse;

    [[maybe_unused]] void log_undefined_operation(const QString & op, const PackToken & left, const PackToken & right)
    {
        qWarning(cparseLog) << "Unexpected operation with operator '" << op << "' and operands: " << left.str() << " and " << right.str();
    }

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

const Config & Calculator::config() const
{
    return Config::defaultConfig();
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

Calculator::Calculator(Calculator && calc) noexcept
    : m_compiled(calc.m_compiled)
{
    std::swap(calc.m_rpn, m_rpn);
}

Calculator::Calculator(const QString & expr, const TokenMap & vars,
                       const QString & delim, int * rest, const Config & config)
{
    m_rpn = RpnBuilder::toRPN(expr, vars, delim, rest, config);
    m_compiled = !m_rpn.empty();
}

Calculator::~Calculator()
{
    RpnBuilder::clearRPN(&this->m_rpn);
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

Calculator & Calculator::operator=(Calculator && calc) noexcept
{
    std::swap(calc.m_rpn, m_rpn);
    std::swap(calc.m_compiled, m_compiled);
    return *this;
}

bool Calculator::compiled() const
{
    return m_compiled;
}

PackToken Calculator::calculate(const QString & expr, const TokenMap & vars,
                                const QString & delim, int * rest)
{
    RaiiTokenQueue rpn = RpnBuilder::toRPN(expr, vars, delim, rest, Config::defaultConfig());

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
    m_rpn = RpnBuilder::toRPN(expr, vars, delim, rest, config());
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

QDebug & operator<<(QDebug & os, const cparse::Calculator & t)
{
    return os << t.str();
}
