#include <string>

#include "./rpnbuilder.h"

#include "./containers.h"
#include "./functions.h"

using cparse::TokenMap;
using cparse::PackToken;
using cparse::TokenIterator;
using cparse::TokenList;
using cparse::TokenMapData;

/* * * * * Initialize TokenMap * * * * */

// Using the "Construct On First Use Idiom"
// to avoid the "static initialization order fiasco",
// for more information read:
//
// - https://isocpp.org/wiki/faq/ctors#static-init-order
//
TokenMap & TokenMap::base_map()
{
    static TokenMap _base_map(nullptr);
    return _base_map;
}

TokenMap & TokenMap::default_global()
{
    static TokenMap global_map(base_map());
    return global_map;
}

PackToken TokenMap::default_constructor(TokenMap scope)
{
    return scope["kwargs"];
}

TokenMap TokenMap::empty = TokenMap(&default_global());


/* * * * * Iterator functions * * * * */

TokenIterator * TokenIterator::getIterator() const
{
    return static_cast<TokenIterator *>(this->clone());
}

/* * * * * TokenMap iterator implemented functions * * * * */

PackToken * TokenMap::MapIterator::next()
{
    if (it != map.end())
    {
        last = PackToken(it->first);
        ++it;
        return &last;
    }

    it = map.begin();
    return nullptr;
}

void TokenMap::MapIterator::reset()
{
    it = map.begin();
}

/* * * * * TokenList functions: * * * * */

PackToken TokenList::default_constructor(TokenMap scope)
{
    // Get the arguments:
    TokenList list = scope["args"].asList();

    // If the only argument is iterable:
    if (list.list().size() == 1 && list.list()[0]->m_type & IT)
    {
        TokenList new_list;
        TokenIterator * it = static_cast<IterableToken *>(list.list()[0].token())->getIterator();

        PackToken * next = it->next();

        while (next)
        {
            new_list.list().push_back(*next);
            next = it->next();
        }

        delete it;
        return new_list;
    }

    return list;
}

/* * * * * TokenList iterator implemented functions * * * * */

PackToken * TokenList::ListIterator::next()
{
    if (i < list->size())
    {
        return &list->at(i++);
    }

    i = 0;
    return nullptr;
}

void TokenList::ListIterator::reset()
{
    i = 0;
}

/* * * * * MapData_t struct: * * * * */
TokenMapData::TokenMapData() {}
TokenMapData::TokenMapData(TokenMap * p) : parent(p ? new TokenMap(*p) : nullptr) {}
TokenMapData::TokenMapData(const TokenMapData & other)
{
    map = other.map;

    if (other.parent)
    {
        parent = new TokenMap(*(other.parent));
    }
    else
    {
        parent = nullptr;
    }
}

TokenMapData::~TokenMapData()
{
    delete parent;
}

TokenMapData & TokenMapData::operator=(const TokenMapData & other)
{
    if (this != &other)
    {
        delete parent;

        map = other.map;
        parent = other.parent;
    }

    return *this;
}

/* * * * * TokenMap Class: * * * * */

PackToken * TokenMap::find(const QString & key)
{
    auto it = map().find(key);

    if (it != map().end())
    {
        return &it->second;
    }

    if (parent())
    {
        return parent()->find(key);
    }

    return nullptr;
}

const PackToken * TokenMap::find(const QString & key) const
{
    TokenMapData::MapType::const_iterator it = map().find(key);

    if (it != map().end())
    {
        return &it->second;
    }

    if (parent())
    {
        return parent()->find(key);
    }

    return nullptr;
}

TokenMap * TokenMap::findMap(const QString & key)
{
    auto it = map().find(key);

    if (it != map().end())
    {
        return this;
    }

    if (parent())
    {
        return parent()->findMap(key);
    }

    return nullptr;
}

void TokenMap::assign(const QString & key, Token * value)
{
    if (value)
    {
        value = value->clone();
    }
    else
    {
        throw std::invalid_argument("TokenMap assignment expected a non NULL argument as value!");
    }

    PackToken * variable = find(key);

    if (variable)
    {
        (*variable) = PackToken(value);
    }
    else
    {
        map()[key] = PackToken(value);
    }
}

void TokenMap::insert(const QString & key, Token * value)
{
    (*this)[key] = PackToken(value->clone());
}

PackToken & TokenMap::operator[](const QString & key)
{
    return map()[key];
}

TokenMap TokenMap::getChild()
{
    return TokenMap(this);
}

void TokenMap::erase(const QString & key)
{
    map().erase(key);
}
