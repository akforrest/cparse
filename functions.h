#ifndef CPARSE_FUNCTIONS_H_
#define CPARSE_FUNCTIONS_H_

#include <list>
#include <functional>

#include <QString>

#include "token.h"
#include "packtoken.h"
#include "containers.h"

namespace cparse
{
    using FunctionArgs = std::list<QString>;

    class Function : public Token
    {
        public:

            Function() : Token(FUNC) {}

            static PackToken call(const PackToken & _this, const Function * func,
                                  TokenList * args, const TokenMap & scope);

            virtual const QString name() const = 0;
            virtual const FunctionArgs args() const = 0;
            virtual PackToken exec(const TokenMap & scope) const = 0;
    };

    class CppFunction : public Function
    {
        public:

            CppFunction();
            CppFunction(PackToken(*func)(const TokenMap &), const FunctionArgs & args,
                        QString name = QString());
            CppFunction(PackToken(*func)(const TokenMap &), unsigned int nargs,
                        const char ** args, QString name = QString());
            CppFunction(PackToken(*func)(const TokenMap &), QString name = QString());
            CppFunction(std::function<PackToken(const TokenMap &)> func, const FunctionArgs & args,
                        QString name = QString());
            CppFunction(const FunctionArgs & args, std::function<PackToken(const TokenMap &)> func,
                        QString name = QString());
            CppFunction(std::function<PackToken(const TokenMap &)> func, unsigned int nargs,
                        const char ** args, QString name = QString());
            CppFunction(std::function<PackToken(const TokenMap &)> func, QString name = QString());

            const QString name() const override
            {
                return m_name;
            }
            const FunctionArgs args() const override
            {
                return m_args;
            }
            PackToken exec(const TokenMap & scope) const override
            {
                return m_isStdFunc ? m_stdFunc(scope) : m_func(scope);
            }

            Token * clone() const override
            {
                return new CppFunction(static_cast<const CppFunction &>(*this));
            }

        private:

            PackToken(*m_func)(const TokenMap &) {};
            std::function<PackToken(const TokenMap &)> m_stdFunc;
            FunctionArgs m_args;
            QString m_name;
            bool m_isStdFunc;
    };

}  // namespace cparse

#endif  // CPARSE_FUNCTIONS_H_
