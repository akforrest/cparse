#ifndef FUNCTIONS_H_
#define FUNCTIONS_H_

#include <list>
#include <functional>

#include <QString>

namespace cparse
{

    using args_t = std::list<QString>;

    class Function : public TokenBase
    {
        public:
            static PackToken call(const PackToken & _this, const Function * func,
                                  TokenList * args, TokenMap scope);
        public:
            Function() : TokenBase(FUNC) {}
            virtual ~Function() {}

        public:
            virtual const QString name() const = 0;
            virtual const args_t args() const = 0;
            virtual PackToken exec(TokenMap scope) const = 0;
            virtual TokenBase * clone() const = 0;
    };

    class CppFunction : public Function
    {
        public:
            PackToken(*func)(TokenMap) {};
            std::function<PackToken(TokenMap)> stdFunc;
            args_t _args;
            QString _name;
            bool isStdFunc;

            CppFunction();
            CppFunction(PackToken(*func)(TokenMap), const args_t & args,
                        QString name = "");
            CppFunction(PackToken(*func)(TokenMap), unsigned int nargs,
                        const char ** args, QString name = "");
            CppFunction(PackToken(*func)(TokenMap), QString name = "");
            CppFunction(std::function<PackToken(TokenMap)> func, const args_t & args,
                        QString name = "");
            CppFunction(const args_t & args, std::function<PackToken(TokenMap)> func,
                        QString name = "");
            CppFunction(std::function<PackToken(TokenMap)> func, unsigned int nargs,
                        const char ** args, QString name = "");
            CppFunction(std::function<PackToken(TokenMap)> func, QString name = "");

            virtual const QString name() const
            {
                return _name;
            }
            virtual const args_t args() const
            {
                return _args;
            }
            virtual PackToken exec(TokenMap scope) const
            {
                return isStdFunc ? stdFunc(scope) : func(scope);
            }

            virtual TokenBase * clone() const
            {
                return new CppFunction(static_cast<const CppFunction &>(*this));
            }
    };

}  // namespace cparse

#endif  // FUNCTIONS_H_
