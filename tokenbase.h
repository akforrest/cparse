#ifndef CPARSE_TOKENBASE_H
#define CPARSE_TOKENBASE_H

#include "cparse-types.h"

#include <QtGlobal>

#include <queue>

namespace cparse
{
    using tokType_t = quint8;
    using opID_t = quint64;

    struct TokenBase
    {
        tokType_t type{};

        virtual ~TokenBase() {}
        TokenBase() {}
        TokenBase(tokType_t type) : type(type) {}

        virtual TokenBase * clone() const = 0;
    };

    template<class T> class Token : public TokenBase
    {
        public:
            T val;
            Token(T t, tokType_t type) : TokenBase(type), val(t) {}
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
