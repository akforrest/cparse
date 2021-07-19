#ifndef CPARSE_TOKENBASE_H
#define CPARSE_TOKENBASE_H

#include "tokentype.h"

#include <queue>

namespace cparse
{
    class TokenBase
    {
        public:

            TokenBase() {}
            TokenBase(TokenType type) : m_type(type) {}
            virtual ~TokenBase() {}

            virtual TokenBase * clone() const = 0;

            TokenType m_type = TokenType::ANY_TYPE;
    };

    template<class T> class TokenTyped : public TokenBase
    {
        public:

            TokenTyped(T t, TokenType type) : TokenBase(type), m_val(t) {}
            TokenBase * clone() const override;

            T m_val;
    };

    class TokenNone : public TokenBase
    {
        public:

            TokenNone() : TokenBase(NONE) {}
            TokenBase * clone() const override
            {
                return new TokenNone(*this);
            }
    };

    class TokenUnary : public TokenBase
    {
        public:

            TokenUnary() : TokenBase(UNARY) {}
            TokenBase * clone() const override
            {
                return new TokenUnary(*this);
            }
    };

    using TokenQueue = std::queue<TokenBase *>;
}

#endif // CPARSE_TOKENBASE_H
