#include "reftoken.h"

#include "containers.h"

using cparse::Token;
using cparse::RefToken;

RefToken::RefToken(PackToken k, Token * v, PackToken m)
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

Token * RefToken::resolve(TokenMap * localScope) const
{
    Token * result = nullptr;

    // Local variables have no origin == NONE,
    // thus, require a localScope to be resolved:
    if (m_origin->m_type == NONE && localScope)
    {
        // Get the most recent value from the local scope:
        PackToken * r_value = localScope->find(m_key.asString());

        if (r_value)
        {
            result = (*r_value)->clone();
        }
    }

    // In last case return the compilation-time value:
    return result ? result : m_originalValue->clone();
}

Token * RefToken::clone() const
{
    return new RefToken(*this);
}
