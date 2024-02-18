#include <memory>
#include <string>

#include "cparse.h"
#include "rpnbuilder.h"
#include "containers.h"
#include "functions.h"

using namespace cparse;

bool TokenMap::operator==(const TokenMap &other) const
{
    return m_ref == other.m_ref && m_type == other.m_type;
}

TokenMap TokenMap::detachedCopy(const TokenMap &other)
{
    TokenMap m(other);
    m.m_ref = std::make_shared<TokenMapData>(*other.m_ref);
    return m;
}

/* * * * * Iterator functions * * * * */

TokenIterator *TokenIterator::getIterator() const
{
    return static_cast<TokenIterator *>(this->clone());
}

/* * * * * TokenMap iterator implemented functions * * * * */

PackToken *TokenMap::MapIterator::next()
{
    if (it != map.end()) {
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

PackToken &TokenList::operator[](const quint64 idx) const
{
    if (list().size() <= idx) {
        qWarning(cparseLog) << "list index out of range";
        static auto listErrorToken = PackToken::Error("list index out of range");
        return listErrorToken;
    }

    return list()[idx];
}

/* * * * * TokenList iterator implemented functions * * * * */

PackToken *TokenList::ListIterator::next()
{
    if (i < list->size()) {
        return &list->at(i++);
    }

    i = 0;
    return nullptr;
}

void TokenList::ListIterator::reset()
{
    i = 0;
}

TokenMap::TokenMapData::TokenMapData() = default;

TokenMap::TokenMapData::TokenMapData(TokenMap *p) : m_parentMap(p ? new TokenMap(*p) : nullptr) { }

TokenMap::TokenMapData::TokenMapData(const TokenMapData &other)
{
    m_map = other.m_map;

    if (other.m_parentMap) {
        m_parentMap = std::make_unique<TokenMap>(*(other.m_parentMap));
    } else {
        m_parentMap = nullptr;
    }
}

TokenMap::TokenMapData::~TokenMapData() { }

TokenMap::TokenMapData &TokenMap::TokenMapData::operator=(const TokenMapData &other)
{
    if (this != &other) {
        m_map = other.m_map;

        if (other.m_parentMap) {
            m_parentMap = std::make_unique<TokenMap>(*(other.m_parentMap));
        } else {
            m_parentMap = nullptr;
        }
    }

    return *this;
}

/* * * * * TokenMap Class: * * * * */

PackToken *TokenMap::find(const QString &key)
{
    auto it = map().find(key);

    if (it != map().end()) {
        return &it->second;
    }

    if (parent()) {
        return parent()->find(key);
    }

    return nullptr;
}

const PackToken *TokenMap::find(const QString &key) const
{
    auto it = map().find(key);

    if (it != map().end()) {
        return &it->second;
    }

    if (key.contains('.')) {
        const auto path = key.split('.', Qt::SkipEmptyParts);
        if (!path.isEmpty()) {
            it = map().find(path.constFirst());

            if (it != map().end()) {
                if (path.size() == 1) {
                    return &it->second;
                }
                int index = 1;
                const PackToken *token = &it->second;
                while (index < path.size() && token != nullptr && token->type() == MAP) {
                    auto nextToken = token->asMap().find(path.at(index));
                    if (nextToken == nullptr) {
                        break;
                    }
                    token = nextToken;
                    index++;
                }
                if (index == path.size() && token != nullptr) {
                    return token;
                }
            }
        }
    }

    if (parent()) {
        return parent()->find(key);
    }

    return nullptr;
}

TokenMap *TokenMap::findMap(const QString &key)
{
    auto it = map().find(key);

    if (it != map().end()) {
        return this;
    }

    if (parent()) {
        return parent()->findMap(key);
    }

    return nullptr;
}

const TokenMap *TokenMap::findMap(const QString &key) const
{
    auto it = map().find(key);

    if (it != map().end()) {
        return this;
    }

    if (parent()) {
        return parent()->findMap(key);
    }

    return nullptr;
}

void TokenMap::assign(const QString &key, Token *value)
{
    if (value) {
        value = value->clone();
    } else {
        qWarning(cparseLog) << "TokenMap assignment expected a non NULL argument as value!";
        Q_ASSERT(false);
        return;
    }

    PackToken *variable = find(key);

    if (variable) {
        (*variable) = PackToken(value);
    } else {
        map()[key] = PackToken(value);
    }
}

void TokenMap::insert(const QString &key, Token *value)
{
    (*this)[key] = PackToken(value->clone());
}

void TokenMap::insert(const QString &key, PackToken &&value)
{
    (*this)[key] = value;
}

PackToken &TokenMap::operator[](const QString &key)
{
    return map()[key];
}

const PackToken &TokenMap::operator[](const QString &key) const
{
    return map()[key];
}

TokenMap TokenMap::getChild()
{
    return TokenMap(this);
}

void TokenMap::erase(const QString &key)
{
    map().erase(key);
}
