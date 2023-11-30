#include "reftoken.h"

#include "containers.h"
#include "config.h"

using cparse::RefToken;
using cparse::Token;

RefToken::RefToken(PackToken k, Token *v, PackToken m)
    : Token(TokenType(v->m_type | TokenType::REF)),
      m_key(std::forward<PackToken>(k)),
      m_origin(std::forward<PackToken>(m)),
      m_originalValue(v)
{
}

RefToken::RefToken(PackToken k, PackToken v, PackToken m)
    : Token(TokenType(v->m_type | TokenType::REF)),
      m_key(std::forward<PackToken>(k)),
      m_origin(std::forward<PackToken>(m)),
      m_originalValue(std::forward<PackToken>(v))
{
}

Token *RefToken::resolve(const TokenMap *localScope, const TokenMap *configScope) const
{
    const PackToken *pack = nullptr;

    // Local variables have no origin == NONE,
    // thus, require a localScope to be resolved:
    if (m_origin->m_type == NONE && localScope) {
        // Get the most recent value from the local scope:
        pack = localScope->find(m_key.asString());
    }

    if (pack == nullptr && configScope && m_origin->m_type == NONE) {
        // Get the most recent value from the local scope:
        pack = configScope->find(m_key.asString());
    }

    if (pack == nullptr && m_origin->m_type != NONE && m_key.canConvertToString()) {
        const auto &typeMap = ObjectTypeRegistry::typeMap(m_origin->m_type);
        pack = typeMap.find(m_key.asString());
    }

    Token *result = nullptr;

    if (pack != nullptr) {
        result = (*pack)->clone();
    }

    // In last case return the compilation-time value:
    return result ? result : m_originalValue->clone();
}

Token *RefToken::clone() const
{
    return new RefToken(*this);
}
