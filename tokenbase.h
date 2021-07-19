#ifndef CPARSE_TOKENBASE_H
#define CPARSE_TOKENBASE_H

#include "tokentype.h"

#include <QtGlobal>

#include <queue>

namespace cparse
{
    using opID_t = quint64;

    struct TokenBase
    {
        TokenType type = TokenType::ANY_TYPE;

        virtual ~TokenBase() {}
        TokenBase() {}
        TokenBase(TokenType type) : type(type) {}

        virtual TokenBase * clone() const = 0;
    };

    template<class T> class Token : public TokenBase
    {
        public:
            T val;
            Token(T t, TokenType type) : TokenBase(type), val(t) {}
            TokenBase * clone() const override;
    };

    struct TokenNone : public TokenBase
    {
        TokenNone() : TokenBase(NONE) {}
        TokenBase * clone() const override
        {
            return new TokenNone(*this);
        }
    };

    struct TokenUnary : public TokenBase
    {
        TokenUnary() : TokenBase(UNARY) {}
        TokenBase * clone() const override
        {
            return new TokenUnary(*this);
        }
    };
    using TokenQueue_t = std::queue<TokenBase *>;
}

#endif // CPARSE_TOKENBASE_H
