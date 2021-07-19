#include <sstream>
#include <string>
#include <iostream>

#include "rpnbuilder.h"
#include "packtoken.h"
#include "reftoken.h"
#include "exceptions.h"

using cparse::PackToken;
using cparse::Token;
using cparse::TokenMap;
using cparse::TokenList;
using cparse::Tuple;
using cparse::STuple;
using cparse::Function;

namespace
{
    using namespace cparse;

    PackToken & noneToken()
    {
        static PackToken none = PackToken(TokenNone());
        return none;
    }

    PackToken & errorToken()
    {
        static PackToken none = PackToken(TokenError());
        return none;
    }

    PackToken & rejectToken()
    {
        static PackToken none = PackToken(TokenReject());
        return none;
    }
}

PackToken & PackToken::None()
{
    return noneToken();
}

PackToken & PackToken::Error()
{
    return errorToken();
}

PackToken & PackToken::Reject()
{
    return rejectToken();
}

PackToken::ToStringFunc & PackToken::str_custom()
{
    static ToStringFunc func = nullptr;
    return func;
}

PackToken::PackToken(const Token & t) : m_base(t.clone()) {}

PackToken::PackToken() : m_base(new TokenNone()) {}

PackToken::PackToken(const TokenMap & map) : m_base(new TokenMap(map)) {}
PackToken::PackToken(const TokenList & list) : m_base(new TokenList(list)) {}

PackToken::~PackToken()
{
    delete m_base;
}

PackToken::PackToken(PackToken && t) noexcept : m_base(t.m_base)
{
    t.m_base = nullptr;
}

PackToken::PackToken(const PackToken & t) : m_base(t.m_base->clone()) {}

PackToken & PackToken::operator=(const PackToken & t)
{
    delete m_base;
    m_base = t.m_base->clone();
    return *this;
}

bool PackToken::operator==(const PackToken & token) const
{
    if (NUM & token.m_base->m_type & m_base->m_type)
    {
        return token.asReal() == asReal();
    }

    if (token.m_base->m_type != m_base->m_type)
    {
        return false;
    }

    // Compare strings to simplify code
    return token.str() == str();
}

bool PackToken::operator!=(const PackToken & token) const
{
    return !(*this == token);
}

Token * PackToken::operator->() const
{
    return m_base;
}

QDebug cparse::operator<<(QDebug os, const PackToken & t)
{
    return os << t.str();
}

std::ostream & cparse::operator<<(std::ostream & os, const PackToken & t)
{
    return os << t.str().toStdString().c_str();
}

PackToken & PackToken::operator[](const QString & key)
{
    if (m_base->m_type != MAP)
    {
        Q_ASSERT(false);
        return errorToken();
    }

    return (*static_cast<TokenMap *>(m_base))[key];
}

Token * PackToken::token()
{
    return m_base;
}

const Token * PackToken::token() const
{
    return m_base;
}

TokenType PackToken::type() const
{
    return m_base->m_type;
}

bool PackToken::isError() const
{
    return m_base->m_type == TokenType::ERROR;
}

const PackToken & PackToken::operator[](const QString & key) const
{
    if (m_base->m_type != MAP)
    {
        Q_ASSERT(false);
        return errorToken();
    }

    return (*static_cast<TokenMap *>(m_base))[key];
}
PackToken & PackToken::operator[](const char * key)
{
    if (m_base->m_type != MAP)
    {
        Q_ASSERT(false);
        return errorToken();
    }

    return (*static_cast<TokenMap *>(m_base))[key];
}

const PackToken & PackToken::operator[](const char * key) const
{
    if (m_base->m_type != MAP)
    {
        Q_ASSERT(false);
        return errorToken();
    }

    return (*static_cast<TokenMap *>(m_base))[key];
}

bool PackToken::canConvertToBool() const
{
    switch (m_base->m_type)
    {
        case REAL:
        case INT:
        case BOOL:
        case STR:
        case MAP:
        case FUNC:
        case NONE:
        case TUPLE:
        case STUPLE:
            return true;

        default:
            break;
    }

    return false;
}

bool PackToken::asBool() const
{
    switch (m_base->m_type)
    {
        case REAL:
            return static_cast<TokenTyped<qreal>*>(m_base)->m_val != 0;

        case INT:
            return static_cast<TokenTyped<qint64>*>(m_base)->m_val != 0;

        case BOOL:
            return static_cast<TokenTyped<uint8_t>*>(m_base)->m_val != 0;

        case STR:
            return !static_cast<TokenTyped<QString>*>(m_base)->m_val.isEmpty();

        case MAP:
        case FUNC:
            return true;

        case NONE:
            return false;

        case TUPLE:
        case STUPLE:
            return !static_cast<Tuple *>(m_base)->list().empty();

        default:
            Q_ASSERT_X(false, "PackToken::asBool", "internal type can not be cast to boolean!");
    }

    return false;
}

bool PackToken::canConvertToReal() const
{
    switch (m_base->m_type)
    {
        case REAL:
        case INT:
        case BOOL:
            return true;

        default:
            break;
    }

    return false;
}

qreal PackToken::asReal() const
{
    switch (m_base->m_type)
    {
        case REAL:
            return static_cast<TokenTyped<qreal>*>(m_base)->m_val;

        case INT:
            return static_cast<qreal>(static_cast<TokenTyped<qint64>*>(m_base)->m_val);

        case BOOL:
            return static_cast<TokenTyped<uint8_t>*>(m_base)->m_val;

        default:
        {
            if (!(m_base->m_type & NUM))
            {
                Q_ASSERT_X(false, "PackToken::asReal", "internal type is not a number!");
            }
            else
            {
                Q_ASSERT_X(false, "PackToken::asReal", "internal type is an unsupported number type!");
            }
        }
    }

    return 0.0;
}

bool PackToken::canConvertToInt() const
{
    switch (m_base->m_type)
    {
        case REAL:
        case INT:
        case BOOL:
            return true;

        default:
            break;
    }

    return false;
}

qint64 PackToken::asInt() const
{
    switch (m_base->m_type)
    {
        case REAL:
            return static_cast<qint64>(static_cast<TokenTyped<qreal>*>(m_base)->m_val);

        case INT:
            return static_cast<TokenTyped<qint64>*>(m_base)->m_val;

        case BOOL:
            return static_cast<TokenTyped<uint8_t>*>(m_base)->m_val;

        default:
            if (!(m_base->m_type & NUM))
            {
                Q_ASSERT_X(false, "PackToken::asInt", "internal type is not a number!");
            }
            else
            {
                Q_ASSERT_X(false, "PackToken::asInt", "internal type is an unsupported number type!");
            }
    }

    return 0;
}

bool PackToken::canConvertToString() const
{
    return (!(m_base->m_type != STR && m_base->m_type != VAR && m_base->m_type != OP));
}

QString PackToken::asString() const
{
    if (!this->canConvertToString())
    {
        Q_ASSERT_X(false, "PackToken::asString", "internal type is not a string!");
        return QString();
    }

    return static_cast<TokenTyped<QString>*>(m_base)->m_val;
}

bool PackToken::canConvertToMap() const
{
    return m_base->m_type == MAP;
}
bool PackToken::canConvertToList() const
{
    return m_base->m_type == LIST;
}
bool PackToken::canConvertToTuple() const
{
    return m_base->m_type == TUPLE;
}
bool PackToken::canConvertToSTuple() const
{
    return m_base->m_type == STUPLE;
}
bool PackToken::canConvertToFunc() const
{
    return m_base->m_type == FUNC;
}

TokenMap & PackToken::asMap() const
{
    if (m_base->m_type != MAP)
    {
        Q_ASSERT(false);
        static TokenMap nonType;
        return nonType;
    }

    return *static_cast<TokenMap *>(m_base);
}

TokenList & PackToken::asList() const
{
    if (m_base->m_type != LIST)
    {
        Q_ASSERT(false);
        static TokenList nonType;
        return nonType;
    }

    return *static_cast<TokenList *>(m_base);
}

Tuple & PackToken::asTuple() const
{
    if (m_base->m_type != TUPLE)
    {
        Q_ASSERT(false);
        static Tuple nonType;
        return nonType;
    }

    return *static_cast<Tuple *>(m_base);
}

STuple & PackToken::asSTuple() const
{
    if (m_base->m_type != STUPLE)
    {
        Q_ASSERT(false);
        static STuple nonType;
        return nonType;
    }

    return *static_cast<STuple *>(m_base);
}

Function * PackToken::asFunc() const
{
    if (m_base->m_type != FUNC)
    {
        Q_ASSERT(false);
        static Function * nonType = nullptr;
        return nonType;
    }

    return static_cast<Function *>(m_base);
}

QString PackToken::str(uint32_t nest) const
{
    return PackToken::str(m_base, nest);
}

QString PackToken::str(const Token * base, uint32_t nest)
{
    QString ss;
    TokenMapData::MapType * tmap;
    TokenMapData::MapType::iterator m_it;

    TokenList::ListType * tlist;
    TokenList::ListType::iterator l_it;
    const Function * func;
    bool first, boolval;
    QString name;

    if (!base)
    {
        return "undefined";
    }

    if (base->m_type & REF)
    {
        base = static_cast<const RefToken *>(base)->resolve();
        name = static_cast<const RefToken *>(base)->m_key.str();
    }

    /* * * * * Check for a user defined functions: * * * * */

    if (PackToken::str_custom())
    {
        auto result = PackToken::str_custom()(base, nest);

        if (!result.isEmpty())
        {
            return result;
        }
    }

    /* * * * * Stringify the token: * * * * */

    switch (base->m_type)
    {
        case NONE:
            return "None";

        case UNARY:
            return "UnaryToken";

        case OP:
            return static_cast<const TokenTyped<QString>*>(base)->m_val;

        case VAR:
            return static_cast<const TokenTyped<QString>*>(base)->m_val;

        case REAL:
            ss += QString::number(static_cast<const TokenTyped<qreal>*>(base)->m_val);
            return ss;

        case INT:
            ss += QString::number(static_cast<const TokenTyped<qint64>*>(base)->m_val);
            return ss;

        case BOOL:
            boolval = static_cast<const TokenTyped<uint8_t>*>(base)->m_val;
            return boolval ? "true" : "false";

        case STR:
            return "\"" + static_cast<const TokenTyped<QString>*>(base)->m_val + "\"";

        case FUNC:
            func = static_cast<const Function *>(base);

            if (!func->name().isEmpty())
            {
                return "[Function: " + func->name() + "]";
            }

            if (!name.isEmpty())
            {
                return "[Function: " + name + "]";
            }

            return "[Function]";

        case TUPLE:
        case STUPLE:
            if (nest == 0)
            {
                return "[Tuple]";
            }

            ss += "(";
            first = true;

            for (const PackToken & token : static_cast<const Tuple *>(base)->list())
            {
                if (!first)
                {
                    ss += ", ";
                }
                else
                {
                    first = false;
                }

                ss += str(token.token(), nest - 1);
            }

            if (first)
            {
                // Its an empty tuple:
                // Add a `,` to make it different than ():
                ss += ",)";
            }
            else
            {
                ss += ")";
            }

            return ss;

        case MAP:
            if (nest == 0)
            {
                return "[Map]";
            }

            tmap = &(static_cast<const TokenMap *>(base)->map());

            if (tmap->empty())
            {
                return "{}";
            }

            ss += "{";

            for (m_it = tmap->begin(); m_it != tmap->end(); ++m_it)
            {
                ss += (m_it == tmap->begin() ? "" : ",");
                ss += " \"" + m_it->first + "\": " + m_it->second.str(nest - 1);
            }

            ss += " }";
            return ss;

        case LIST:
            if (nest == 0)
            {
                return "[List]";
            }

            tlist = &(static_cast<const TokenList *>(base)->list());

            if (tlist->empty())
            {
                return "[]";
            }

            ss += "[";

            for (l_it = tlist->begin(); l_it != tlist->end(); ++l_it)
            {
                ss += (l_it == tlist->begin() ? "" : ",");
                ss += " " + l_it->str(nest - 1);
            }

            ss += " ]";
            return ss;

        default:
            if (base->m_type & IT)
            {
                return "[Iterator]";
            }

            return "unknown_type";
    }
}

Token * PackToken::release() &&
{
    Token * b = m_base;
    // Setting base to 0 leaves the class in an invalid state,
    // except for destruction.
    m_base = nullptr;
    return b;
}
