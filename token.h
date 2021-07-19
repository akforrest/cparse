#ifndef CPARSE_TOKENBASE_H
#define CPARSE_TOKENBASE_H

#include "tokentype.h"

#include <queue>

namespace cparse
{
    class Token
    {
        public:

            Token() {}
            Token(TokenType type) : m_type(type) {}
            virtual ~Token() {}

            virtual Token * clone() const = 0;

            TokenType m_type = TokenType::ANY_TYPE;
    };

    template<class T> class TokenTyped : public Token
    {
        public:

            TokenTyped(T t, TokenType type) : Token(type), m_val(t) {}
            Token * clone() const override;

            T m_val;
    };

    class TokenNone : public Token
    {
        public:

            TokenNone() : Token(NONE) {}
            Token * clone() const override
            {
                return new TokenNone(*this);
            }
    };

    class TokenUnary : public Token
    {
        public:

            TokenUnary() : Token(UNARY) {}
            Token * clone() const override
            {
                return new TokenUnary(*this);
            }
    };

    template<class T>
    Token * TokenTyped<T>::clone() const
    {
        return new TokenTyped(*this);
    }

    using TokenQueue = std::queue<Token *>;
}

#endif // CPARSE_TOKENBASE_H
