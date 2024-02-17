#ifndef CPARSE_TOKENBASE_H
#define CPARSE_TOKENBASE_H

#include "tokentype.h"

#include <QString>

#include <queue>

namespace cparse {
    class Token
    {
    public:
        Token() { }
        Token(TokenType type) : m_type(type) { }
        virtual ~Token() { }

        virtual Token *clone() const = 0;

        virtual bool canConvertTo(TokenType) const;

        virtual bool asBool() const;
        virtual qreal asReal() const;
        virtual qint64 asInt() const;
        virtual QString asString() const;

        TokenType m_type = TokenType::ANY_TYPE;
    };

    template <class T>
    class TokenTyped : public Token
    {
    public:
        TokenTyped(T t, TokenType type) : Token(type), m_val(t) { }
        Token *clone() const override;

        T m_val;
    };

    class TokenNone : public Token
    {
    public:
        TokenNone() : Token(NONE) { }
        Token *clone() const override { return new TokenNone(*this); }
    };

    class TokenReject : public Token
    {
    public:
        TokenReject() : Token(REJECT) { }
        Token *clone() const override { return new TokenReject(*this); }
    };

    class TokenError : public Token
    {
    public:
        TokenError(const QString &cause = {}) : Token(ERROR), m_cause(cause) { }
        Token *clone() const override { return new TokenError(*this); }
        QString cause() const { return m_cause; }

    private:
        QString m_cause;
    };

    class TokenUnary : public Token
    {
    public:
        TokenUnary() : Token(UNARY) { }
        Token *clone() const override { return new TokenUnary(*this); }
    };

    template <class T>
    Token *TokenTyped<T>::clone() const
    {
        return new TokenTyped(*this);
    }

    using TokenQueue = std::queue<Token *>;
}

#endif // CPARSE_TOKENBASE_H
