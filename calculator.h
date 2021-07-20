#ifndef CPARSE_CALCULATOR_H
#define CPARSE_CALCULATOR_H

#include <QString>

#include "packtoken.h"
#include "containers.h"
#include "config.h"

namespace cparse
{
    class Calculator
    {
        public:

            Calculator(const Config & config = Config::defaultConfig());

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

            PackToken evaluate(const TokenMap & vars = &TokenMap::empty) const;
            PackToken evaluate(const QString & expr, const TokenMap & vars = &TokenMap::empty,
                               const QString & delim = QString(), int * rest = nullptr);

            static PackToken calculate(const QString & expr, const TokenMap & vars = &TokenMap::empty,
                                       const QString & delim = QString(), int * rest = nullptr,
                                       const Config & config = Config::defaultConfig());

            const Config & config() const;
            void setConfig(const Config & config);

            ////

            QString str() const;
            static QString str(TokenQueue rpn);

        private:

            Config m_config;
            TokenQueue m_rpn;
            bool m_compiled = false;
    };

    QDebug & operator<<(QDebug & os, const cparse::Calculator & t);
}

#endif // CPARSE_CALCULATOR_H
