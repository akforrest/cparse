#include "calculator.h"

#include "cparse.h"
#include "rpnbuilder.h"
#include "tokenhelpers.h"

namespace
{
    using namespace cparse;

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

    m_config = calc.m_config;
    m_compiled = calc.m_compiled;
    m_compileTimeVars = calc.m_compileTimeVars;
}

Calculator::Calculator(Calculator && calc) noexcept
    : m_compiled(calc.m_compiled)
{
    std::swap(calc.m_rpn, m_rpn);
    std::swap(calc.m_config, m_config);
    std::swap(calc.m_compileTimeVars, m_compileTimeVars);
}

Calculator::Calculator(const Config & config)
    : m_config(config)
{
}

Calculator::Calculator(const QString & expr, const TokenMap & vars,
                       const QString & delim, int * rest, const Config & config)
    : m_config(config)
{
    m_rpn = RpnBuilder::toRPN(expr, vars, delim, rest, config);
    m_compiled = !m_rpn.empty();
    m_compileTimeVars = TokenMap::detachedCopy(vars);
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

    m_config = calc.m_config;
    m_compiled = calc.m_compiled;
    m_compileTimeVars = calc.m_compileTimeVars;

    return *this;
}

Calculator & Calculator::operator=(Calculator && calc) noexcept
{
    std::swap(calc.m_rpn, m_rpn);
    std::swap(calc.m_compileTimeVars, m_compileTimeVars);
    std::swap(calc.m_compiled, m_compiled);
    std::swap(calc.m_config, m_config);
    return *this;
}

bool Calculator::compiled() const
{
    return m_compiled;
}

PackToken Calculator::calculate(const QString & expr, const TokenMap & vars,
                                const QString & delim, int * rest,
                                const Config & config)
{
    RaiiTokenQueue rpn = RpnBuilder::toRPN(expr, vars, delim, rest, config);

    if (rpn.empty())
    {
        return PackToken::Error();
    }

    Token * ret = RpnBuilder::calculate(rpn, vars, config);

    if (ret == nullptr)
    {
        return PackToken::Error();
    }

    return PackToken(resolveReferenceToken(ret));
}

bool Calculator::compile(const QString & expr, const TokenMap & vars,
                         const QString & delim, int * rest)
{
    // Make sure it is empty:
    RpnBuilder::clearRPN(&this->m_rpn);
    m_rpn = RpnBuilder::toRPN(expr, vars, delim, rest, m_config);
    m_compiled = !m_rpn.empty();
    m_compileTimeVars = TokenMap::detachedCopy(vars);
    return this->compiled();
}

PackToken Calculator::evaluate() const
{
    return this->evaluate(m_compileTimeVars);
}

PackToken Calculator::evaluate(const TokenMap & vars) const
{
    if (!m_compiled)
    {
        return PackToken::Error();
    }

    Token * value = RpnBuilder::calculate(this->m_rpn, vars, m_config);

    if (value == nullptr)
    {
        return PackToken::Error();
    }

    return PackToken(resolveReferenceToken(value));
}

PackToken Calculator::evaluate(const QString & expr, const TokenMap & vars, const QString & delim, int * rest)
{
    this->compile(expr, vars, delim, rest);
    return this->evaluate(vars);
}

const Config & Calculator::config() const
{
    return m_config;
}

void Calculator::setConfig(const Config & config)
{
    m_config = config;
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
        ss += PackToken(resolveReferenceToken(rpn.front()->clone())).str();
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
