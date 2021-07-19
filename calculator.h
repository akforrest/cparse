#ifndef CPARSE_CALCULATOR_H
#define CPARSE_CALCULATOR_H

#include <QString>

#include "packtoken.h"
#include "containers.h"
#include "config.h"

namespace cparse
{
    using typeMap_t = std::map<cparse::TokenType, TokenMap>;

    class Calculator
    {
        public:
            static Config_t & Default();

            static typeMap_t & type_attribute_map();

            static PackToken calculate(const char * expr, const TokenMap & vars = &TokenMap::empty,
                                       const char * delim = nullptr, const char ** rest = nullptr);

            static TokenBase * calculate(const TokenQueue & RPN, const TokenMap & scope,
                                         const Config_t & config = Default());
            static TokenQueue toRPN(const char * expr, TokenMap vars,
                                      const char * delim = nullptr, const char ** rest = nullptr,
                                      Config_t config = Default());

            virtual ~Calculator();
            Calculator();
            Calculator(const Calculator & calc);
            Calculator(const char * expr, const TokenMap & vars = &TokenMap::empty,
                       const char * delim = 0, const char ** rest = nullptr,
                       const Config_t & config = Default());
            void compile(const char * expr, const TokenMap & vars = &TokenMap::empty,
                         const char * delim = 0, const char ** rest = nullptr);
            PackToken eval(const TokenMap & vars = &TokenMap::empty, bool keep_refs = false) const;

            // Serialization:
            QString str() const;
            static QString str(TokenQueue rpn);

            // Operators:
            Calculator & operator=(const Calculator & calc);

        protected:

            virtual const Config_t Config() const
            {
                return Default();
            }

        private:

            // Used to dealloc a TokenQueue_t safely.
            struct RAII_TokenQueue_t;
            TokenQueue RPN;
    };
}

#endif // CPARSE_CALCULATOR_H
