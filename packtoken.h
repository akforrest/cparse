#ifndef PACKTOKEN_H_
#define PACKTOKEN_H_

#include <QString>
#include <QDebug>

namespace cparse
{
    struct TokenBase;
    struct TokenNone;
    using tokType_t = uint8_t;

    // Encapsulate TokenBase* into a friendlier interface
    class PackToken
    {
            TokenBase * base;

        public:
            static const PackToken & None();

            typedef QString(*strFunc_t)(const TokenBase *, uint32_t);
            static strFunc_t & str_custom();

        public:
            PackToken();
            PackToken(const TokenBase & t);
            PackToken(const PackToken & t);
            PackToken(PackToken && t) noexcept;
            PackToken & operator=(const PackToken & t);

            template<class C>
            PackToken(C c, tokType type) : base(new Token<C>(c, type)) {}
            PackToken(int i) : base(new Token<int64_t>(i, INT)) {}
            PackToken(int64_t l) : base(new Token<int64_t>(l, INT)) {}
            PackToken(bool b) : base(new Token<uint8_t>(b, BOOL)) {}
            PackToken(size_t s) : base(new Token<int64_t>(s, INT)) {}
            PackToken(float f) : base(new Token<double>(f, REAL)) {}
            PackToken(double d) : base(new Token<double>(d, REAL)) {}
            PackToken(const char * s) : base(new Token<QString>(s, STR)) {}
            PackToken(const QString & s) : base(new Token<QString>(s, STR)) {}
            PackToken(const TokenMap & map);
            PackToken(const TokenList & list);
            ~PackToken();

            TokenBase * operator->() const;
            bool operator==(const PackToken & t) const;
            bool operator!=(const PackToken & t) const;
            PackToken & operator[](const QString & key);
            PackToken & operator[](const char * key);
            const PackToken & operator[](const QString & key) const;
            const PackToken & operator[](const char * key) const;
            TokenBase * token();
            const TokenBase * token() const;

            bool asBool() const;
            double asDouble() const;
            int64_t asInt() const;
            QString asString() const;
            TokenMap & asMap() const;
            TokenList & asList() const;
            Tuple & asTuple() const;
            STuple & asSTuple() const;
            Function * asFunc() const;

            // Specialize this template to your types, e.g.:
            // MyType& m = packToken.as<MyType>();
            template<typename T> T & as() const;

            // The nest argument defines how many times
            // it will recursively print nested structures:
            QString str(uint32_t nest = 3) const;
            static QString str(const TokenBase * t, uint32_t nest = 3);

        public:
            // This constructor makes sure the TokenBase*
            // will be deleted when the packToken destructor is called.
            //
            // If you still plan to use your TokenBase* use instead:
            //
            // - packToken(token->clone())
            //
            explicit PackToken(TokenBase * t) : base(t) {}

        public:
            // Used to recover the original pointer.
            // The intance whose pointer was removed must be an rvalue.
            TokenBase * release() &&;
    };

    // To allow cout to print it:

}  // namespace cparse

QDebug & operator<<(QDebug & os, const cparse::PackToken & t);
std::ostream & operator<<(std::ostream & os, const cparse::PackToken & t);

#endif  // PACKTOKEN_H_