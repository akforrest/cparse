#include <sstream>
#include <string>
#include <iostream>

#include "shuntingyard.h"
#include "packtoken.h"
#include "reftoken.h"
#include "shuntingyardexceptions.h"

using cparse::PackToken;
using cparse::Token;
using cparse::TokenMap;
using cparse::TokenList;
using cparse::Tuple;
using cparse::STuple;
using cparse::Function;

const PackToken & PackToken::None()
{
    static PackToken none = PackToken(TokenNone());
    return none;
}

PackToken::strFunc_t & PackToken::str_custom()
{
    static strFunc_t func = nullptr;
    return func;
}

PackToken::PackToken(const Token & t) : base(t.clone()) {}

PackToken::PackToken() : base(new TokenNone()) {}

PackToken::PackToken(const TokenMap & map) : base(new TokenMap(map)) {}
PackToken::PackToken(const TokenList & list) : base(new TokenList(list)) {}

PackToken::~PackToken()
{
    delete base;
}

PackToken::PackToken(PackToken && t) noexcept : base(t.base)
{
    t.base = nullptr;
}

PackToken::PackToken(const PackToken & t) : base(t.base->clone()) {}

PackToken & PackToken::operator=(const PackToken & t)
{
    delete base;
    base = t.base->clone();
    return *this;
}

bool PackToken::operator==(const PackToken & token) const
{
    if (NUM & token.base->m_type & base->m_type)
    {
        return token.asDouble() == asDouble();
    }

    if (token.base->m_type != base->m_type)
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
    return base;
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
    if (base->m_type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return (*static_cast<TokenMap *>(base))[key];
}

Token * PackToken::token()
{
    return base;
}

const Token * PackToken::token() const
{
    return base;
}
const PackToken & PackToken::operator[](const QString & key) const
{
    if (base->m_type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return (*static_cast<TokenMap *>(base))[key];
}
PackToken & PackToken::operator[](const char * key)
{
    if (base->m_type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return (*static_cast<TokenMap *>(base))[key];
}
const PackToken & PackToken::operator[](const char * key) const
{
    if (base->m_type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return (*static_cast<TokenMap *>(base))[key];
}

bool PackToken::asBool() const
{
    switch (base->m_type)
    {
        case REAL:
            return static_cast<TokenTyped<double>*>(base)->m_val != 0;

        case INT:
            return static_cast<TokenTyped<int64_t>*>(base)->m_val != 0;

        case BOOL:
            return static_cast<TokenTyped<uint8_t>*>(base)->m_val != 0;

        case STR:
            return !static_cast<TokenTyped<QString>*>(base)->m_val.isEmpty();

        case MAP:
        case FUNC:
            return true;

        case NONE:
            return false;

        case TUPLE:
        case STUPLE:
            return !static_cast<Tuple *>(base)->list().empty();

        default:
            throw bad_cast("Token type can not be cast to boolean!");
    }
}

double PackToken::asDouble() const
{
    switch (base->m_type)
    {
        case REAL:
            return static_cast<TokenTyped<double>*>(base)->m_val;

        case INT:
            return static_cast<double>(static_cast<TokenTyped<int64_t>*>(base)->m_val);

        case BOOL:
            return static_cast<TokenTyped<uint8_t>*>(base)->m_val;

        default:
            if (!(base->m_type & NUM))
            {
                throw bad_cast(
                    "The Token is not a number!");
            }
            else
            {
                throw bad_cast(
                    "Unknown numerical type, can't convert it to double!");
            }
    }
}

int64_t PackToken::asInt() const
{
    switch (base->m_type)
    {
        case REAL:
            return static_cast<int64_t>(static_cast<TokenTyped<double>*>(base)->m_val);

        case INT:
            return static_cast<TokenTyped<int64_t>*>(base)->m_val;

        case BOOL:
            return static_cast<TokenTyped<uint8_t>*>(base)->m_val;

        default:
            if (!(base->m_type & NUM))
            {
                throw bad_cast(
                    "The Token is not a number!");
            }
            else
            {
                throw bad_cast(
                    "Unknown numerical type, can't convert it to integer!");
            }
    }
}

QString PackToken::asString() const
{
    if (base->m_type != STR && base->m_type != VAR && base->m_type != OP)
    {
        throw bad_cast(
            "The Token is not a string!");
    }

    return static_cast<TokenTyped<QString>*>(base)->m_val;
}

TokenMap & PackToken::asMap() const
{
    if (base->m_type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return *static_cast<TokenMap *>(base);
}

TokenList & PackToken::asList() const
{
    if (base->m_type != LIST)
    {
        throw bad_cast(
            "The Token is not a list!");
    }

    return *static_cast<TokenList *>(base);
}

Tuple & PackToken::asTuple() const
{
    if (base->m_type != TUPLE)
    {
        throw bad_cast(
            "The Token is not a tuple!");
    }

    return *static_cast<Tuple *>(base);
}

STuple & PackToken::asSTuple() const
{
    if (base->m_type != STUPLE)
    {
        throw bad_cast(
            "The Token is not an special tuple!");
    }

    return *static_cast<STuple *>(base);
}

Function * PackToken::asFunc() const
{
    if (base->m_type != FUNC)
    {
        throw bad_cast(
            "The Token is not a function!");
    }

    return static_cast<Function *>(base);
}

QString PackToken::str(uint32_t nest) const
{
    return PackToken::str(base, nest);
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
            ss += QString::number(static_cast<const TokenTyped<double>*>(base)->m_val);
            return ss;

        case INT:
            ss += QString::number(static_cast<const TokenTyped<int64_t>*>(base)->m_val);
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
    Token * b = base;
    // Setting base to 0 leaves the class in an invalid state,
    // except for destruction.
    base = 0;
    return b;
}
