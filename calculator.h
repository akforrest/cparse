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
            Calculator(const QString & expr, const TokenMap & vars = &TokenMap::empty,
                       const QString & delim = QString(), int * rest = nullptr,
                       const Config & config = defaultConfig());
            virtual ~Calculator();

            void compile(const QString & expr, const TokenMap & vars = &TokenMap::empty,
                         const QString & delim = QString(), int * rest = nullptr);

            static PackToken calculate(const QString & expr, const TokenMap & vars = &TokenMap::empty,
                                       const QString & delim = QString(), int * rest = nullptr);

            static Token * calculate(const TokenQueue & RPN, const TokenMap & scope,
                                     const Config & config = defaultConfig());

            PackToken eval(const TokenMap & vars = &TokenMap::empty, bool keep_refs = false) const;

            static TokenQueue toRPN(const QString & expr, TokenMap vars,
                                    const QString & delim = QString(), int * rest = nullptr,
                                    Config config = defaultConfig());

            static Config & defaultConfig();
            static TokenTypeMap & typeAttributeMap();

            QString str() const;
            static QString str(TokenQueue rpn);

            Calculator & operator=(const Calculator & calc);

        protected:

            virtual const Config & config() const;

        private:

            TokenQueue m_rpn;
    };

    QDebug & operator<<(QDebug & os, const cparse::Calculator & t);
}


#endif // CPARSE_CALCULATOR_H
