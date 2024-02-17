#ifndef CPARSE_PACKTOKEN_H_
#define CPARSE_PACKTOKEN_H_

#include <QString>
#include <QDebug>

#include "token.h"

namespace cparse {
    class TokenMap;
    class TokenList;
    class Token;
    class TokenNone;
    class Tuple;
    class STuple;
    class Function;

    // Encapsulate Token* into a friendlier interface
    class PackToken
    {
    public:
        PackToken();
        PackToken(const Token &t);
        PackToken(const PackToken &t);
        PackToken(PackToken &&t) noexcept;
        PackToken &operator=(const PackToken &t);

        // This constructor makes sure the Token*
        // will be deleted when the packToken destructor is called.
        //
        // If you still plan to use your Token* use instead:
        //
        // - packToken(token->clone())
        //
        explicit PackToken(Token *t) : m_base(t) { }

        template <class C>
        PackToken(C c, TokenType type) : m_base(new TokenTyped<C>(c, type))
        {
        }
        PackToken(int i) : m_base(new TokenTyped<qint64>(i, INT)) { }
        PackToken(qint64 l) : m_base(new TokenTyped<qint64>(l, INT)) { }
        PackToken(bool b) : m_base(new TokenTyped<uint8_t>(b, BOOL)) { }
        PackToken(size_t s) : m_base(new TokenTyped<qint64>(s, INT)) { }
        PackToken(qreal d) : m_base(new TokenTyped<qreal>(d, REAL)) { }
        PackToken(const char *s) : m_base(new TokenTyped<QString>(s, STR)) { }
        PackToken(const QString &s) : m_base(new TokenTyped<QString>(s, STR)) { }
        PackToken(const TokenMap &map);
        PackToken(const TokenList &list);
        ~PackToken();

        Token *operator->() const;
        bool operator==(const PackToken &t) const;
        bool operator!=(const PackToken &t) const;
        PackToken &operator[](const QString &key);
        PackToken &operator[](const char *key);
        const PackToken &operator[](const QString &key) const;
        const PackToken &operator[](const char *key) const;
        Token *token();
        const Token *token() const;

        TokenType type() const;

        bool isError() const;
        bool isEmpty() const;

        bool canConvertToBool() const;
        bool canConvertToString() const;
        bool canConvertToReal() const;
        bool canConvertToInt() const;
        bool canConvertToMap() const;
        bool canConvertToList() const;
        bool canConvertToTuple() const;
        bool canConvertToSTuple() const;
        bool canConvertToFunc() const;

        bool canConvertTo(TokenType type) const;

        bool asBool() const;
        qreal asReal() const;
        qint64 asInt() const;
        QString asString() const;
        TokenMap &asMap() const;
        TokenList &asList() const;
        Tuple &asTuple() const;
        STuple &asSTuple() const;
        Function *asFunc() const;

        // Specialize this template to your types, e.g.:
        // MyType& m = packToken.as<MyType>();
        template <typename T>
        T &as() const;

        static const PackToken &None();
        static const PackToken &Error();
        static PackToken Error(const QString &cause);
        static const PackToken &Reject();

        using ToStringFunc = QString (*)(const Token *, quint32);
        static ToStringFunc &str_custom();

        // The nest argument defines how many times
        // it will recursively print nested structures:
        QString str(uint32_t nest = 3) const;
        static QString str(const Token *t, uint32_t nest = 3);

        // Used to recover the original pointer.
        // The intance whose pointer was removed must be an rvalue.
        Token *release() &&;

    private:
        Token *m_base;
    };

    QDebug operator<<(QDebug os, const cparse::PackToken &t);
    std::ostream &operator<<(std::ostream &os, const cparse::PackToken &t);

} // namespace cparse

#endif // CPARSE_PACKTOKEN_H_
