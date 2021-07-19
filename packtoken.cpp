#include <sstream>
#include <string>
#include <iostream>

#include "./shunting-yard.h"
#include "./packtoken.h"
#include "./shunting-yard-exceptions.h"

using cparse::PackToken;
using cparse::TokenBase;
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

PackToken::PackToken(const TokenBase & t) : base(t.clone()) {}

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
    if (NUM & token.base->type & base->type)
    {
        return token.asDouble() == asDouble();
    }

    if (token.base->type != base->type)
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

TokenBase * PackToken::operator->() const
{
    return base;
}

QDebug & operator<<(QDebug & os, const PackToken & t)
{
    return os << t.str();
}

std::ostream & operator<<(std::ostream & os, const PackToken & t)
{
    return os << t.str().toStdString().c_str();
}

PackToken & PackToken::operator[](const QString & key)
{
    if (base->type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return (*static_cast<TokenMap *>(base))[key];
}

TokenBase * PackToken::token()
{
    return base;
}

const TokenBase * PackToken::token() const
{
    return base;
}
const PackToken & PackToken::operator[](const QString & key) const
{
    if (base->type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return (*static_cast<TokenMap *>(base))[key];
}
PackToken & PackToken::operator[](const char * key)
{
    if (base->type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return (*static_cast<TokenMap *>(base))[key];
}
const PackToken & PackToken::operator[](const char * key) const
{
    if (base->type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return (*static_cast<TokenMap *>(base))[key];
}

bool PackToken::asBool() const
{
    switch (base->type)
    {
        case REAL:
            return static_cast<Token<double>*>(base)->val != 0;

        case INT:
            return static_cast<Token<int64_t>*>(base)->val != 0;

        case BOOL:
            return static_cast<Token<uint8_t>*>(base)->val != 0;

        case STR:
            return !static_cast<Token<std::string>*>(base)->val.empty();

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
    switch (base->type)
    {
        case REAL:
            return static_cast<Token<double>*>(base)->val;

        case INT:
            return static_cast<double>(static_cast<Token<int64_t>*>(base)->val);

        case BOOL:
            return static_cast<Token<uint8_t>*>(base)->val;

        default:
            if (!(base->type & NUM))
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
    switch (base->type)
    {
        case REAL:
            return static_cast<int64_t>(static_cast<Token<double>*>(base)->val);

        case INT:
            return static_cast<Token<int64_t>*>(base)->val;

        case BOOL:
            return static_cast<Token<uint8_t>*>(base)->val;

        default:
            if (!(base->type & NUM))
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
    if (base->type != STR && base->type != VAR && base->type != OP)
    {
        throw bad_cast(
            "The Token is not a string!");
    }

    return static_cast<Token<QString>*>(base)->val;
}

TokenMap & PackToken::asMap() const
{
    if (base->type != MAP)
    {
        throw bad_cast(
            "The Token is not a map!");
    }

    return *static_cast<TokenMap *>(base);
}

TokenList & PackToken::asList() const
{
    if (base->type != LIST)
    {
        throw bad_cast(
            "The Token is not a list!");
    }

    return *static_cast<TokenList *>(base);
}

Tuple & PackToken::asTuple() const
{
    if (base->type != TUPLE)
    {
        throw bad_cast(
            "The Token is not a tuple!");
    }

    return *static_cast<Tuple *>(base);
}

STuple & PackToken::asSTuple() const
{
    if (base->type != STUPLE)
    {
        throw bad_cast(
            "The Token is not an special tuple!");
    }

    return *static_cast<STuple *>(base);
}

Function * PackToken::asFunc() const
{
    if (base->type != FUNC)
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

QString PackToken::str(const TokenBase * base, uint32_t nest)
{
    QString ss;
    TokenMap_t * tmap;
    TokenMap_t::iterator m_it;

    TokenList_t * tlist;
    TokenList_t::iterator l_it;
    const Function * func;
    bool first, boolval;
    QString name;

    if (!base)
    {
        return "undefined";
    }

    if (base->type & REF)
    {
        base = static_cast<const RefToken *>(base)->resolve();
        name = static_cast<const RefToken *>(base)->key.str();
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

    switch (base->type)
    {
        case NONE:
            return "None";

        case UNARY:
            return "UnaryToken";

        case OP:
            return static_cast<const Token<QString>*>(base)->val;

        case VAR:
            return static_cast<const Token<QString>*>(base)->val;

        case REAL:
            ss += QString::number(static_cast<const Token<double>*>(base)->val);
            return ss;

        case INT:
            ss += QString::number(static_cast<const Token<int64_t>*>(base)->val);
            return ss;

        case BOOL:
            boolval = static_cast<const Token<uint8_t>*>(base)->val;
            return boolval ? "true" : "false";

        case STR:
            return "\"" + static_cast<const Token<QString>*>(base)->val + "\"";

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
            if (base->type & IT)
            {
                return "[Iterator]";
            }

            return "unknown_type";
    }
}

TokenBase * PackToken::release() &&
{
    TokenBase * b = base;
    // Setting base to 0 leaves the class in an invalid state,
    // except for destruction.
    base = 0;
    return b;
}
