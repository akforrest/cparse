#ifndef CPARSE_CALCULATOR_H
#define CPARSE_CALCULATOR_H

#include <QString>

#include "packtoken.h"
#include "containers.h"
#include "config.h"

namespace cparse
{
    using TokenTypeMap = std::map<TokenType, TokenMap>;

    class Calculator
    {
        public:

            Calculator();
            Calculator(const Calculator & calc);
            Calculator(Calculator && calc) noexcept;
            Calculator(const QString & expr, const TokenMap & vars = &TokenMap::empty,
                       const QString & delim = QString(), int * rest = nullptr,
                       const Config & config = Config::defaultConfig());
            virtual ~Calculator();

            Calculator & operator=(const Calculator & calc);
            Calculator & operator=(Calculator &&) noexcept;

            bool compiled() const;

            bool compile(const QString & expr, const TokenMap & vars = &TokenMap::empty,
                         const QString & delim = QString(), int * rest = nullptr);

            static PackToken calculate(const QString & expr, const TokenMap & vars = &TokenMap::empty,
                                       const QString & delim = QString(), int * rest = nullptr);

            static Token * calculate(const TokenQueue & RPN, const TokenMap & scope,
                                     const Config & config = Config::defaultConfig());

            PackToken eval(const TokenMap & vars = &TokenMap::empty, bool keep_refs = false) const;

            static TokenTypeMap & typeAttributeMap();

            QString str() const;
            static QString str(TokenQueue rpn);

        protected:

            virtual const Config & config() const;

        private:

            TokenQueue m_rpn;
            bool m_compiled = false;
    };

    QDebug & operator<<(QDebug & os, const cparse::Calculator & t);
}


#endif // CPARSE_CALCULATOR_H
