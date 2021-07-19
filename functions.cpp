#include <string>
#include <utility>

#include "./rpnbuilder.h"
#include "./functions.h"
#include "./exceptions.h"

using cparse::PackToken;
using cparse::Function;
using cparse::TokenList;
using cparse::TokenMap;
using cparse::CppFunction;

/* * * * * class Function * * * * */
PackToken Function::call(const PackToken & _this, const Function * func,
                         TokenList * args, TokenMap scope)
{
    // Build the local namespace:
    TokenMap kwargs;
    TokenMap local = scope.getChild();

    args_t arg_names = func->args();

    auto args_it = args->list().begin();
    args_t::const_iterator names_it = arg_names.begin();

    /* * * * * Parse positional arguments: * * * * */

    while (args_it != args->list().end() && names_it != arg_names.end())
    {
        // If the positional argument list is over:
        if ((*args_it)->m_type == STUPLE)
        {
            break;
        }

        // Else add it to the local namespace:
        local[*names_it] = *args_it;

        ++args_it;
        ++names_it;
    }

    /* * * * * Parse extra positional arguments: * * * * */

    TokenList arglist;

    for (; args_it != args->list().end(); ++args_it)
    {
        // If there is a keyword argument:
        if ((*args_it)->m_type == STUPLE)
        {
            break;
        }

        // Else add it to arglist:
        arglist.list().push_back(*args_it);
    }

    /* * * * * Parse keyword arguments: * * * * */

    for (; args_it != args->list().end(); ++args_it)
    {
        PackToken & arg = *args_it;

        if (arg->m_type != STUPLE)
        {
            throw syntax_error("Positional argument follows keyword argument");
        }

        auto * st = static_cast<STuple *>(arg.token());

        if (st->list().size() != 2)
        {
            throw syntax_error("Keyword tuples must have exactly 2 items!");
        }

        if (st->list()[0]->m_type != STR)
        {
            throw syntax_error("Keyword first argument should be of type string!");
        }

        // Save it:
        QString key = st->list()[0].asString();
        PackToken & value = st->list()[1];
        kwargs[key] = value;
    }

    /* * * * * Set missing positional arguments: * * * * */

    for (; names_it != arg_names.end(); ++names_it)
    {
        // If not set by a keyword argument:
        auto kw_it = kwargs.map().find(*names_it);

        if (kw_it == kwargs.map().end())
        {
            local[*names_it] = PackToken::None();
        }
        else
        {
            local[*names_it] = kw_it->second;
            kwargs.map().erase(kw_it);
        }
    }

    /* * * * * Set built-in variables: * * * * */

    local["this"] = _this;
    local["args"] = arglist;
    local["kwargs"] = kwargs;

    return func->exec(local);
}

/* * * * * class CppFunction * * * * */
CppFunction::CppFunction()
{
    this->_name = "";
    this->isStdFunc = false;
    this->func = nullptr;
}

CppFunction::CppFunction(PackToken(*func)(TokenMap), const args_t & args,
                         QString name)
    : func(func), _args(args)
{
    this->_name = std::move(name);
    this->isStdFunc = false;
}

CppFunction::CppFunction(PackToken(*func)(TokenMap), unsigned int nargs,
                         const char ** args, QString name)
    : func(func)
{
    this->_name = std::move(name);
    this->isStdFunc = false;

    // Add all strings to args list:
    for (uint32_t i = 0; i < nargs; ++i)
    {
        this->_args.push_back(args[i]);
    }
}

// Build a function with no named args:
CppFunction::CppFunction(PackToken(*func)(TokenMap), QString name)
    : func(func)
{
    this->_name = std::move(name);
    this->isStdFunc = false;
}


CppFunction::CppFunction(std::function<PackToken(TokenMap)> func, const args_t & args,
                         QString name)
    : stdFunc(std::move(func)), _args(args)
{
    this->_name = std::move(name);
    this->isStdFunc = true;
}

CppFunction::CppFunction(const args_t & args, std::function<PackToken(TokenMap)> func,
                         QString name)
    : stdFunc(std::move(func)), _args(args)
{
    this->_name = std::move(name);
    this->isStdFunc = true;
}

CppFunction::CppFunction(std::function<PackToken(TokenMap)> func, unsigned int nargs,
                         const char ** args, QString name)
    : stdFunc(std::move(func))
{
    this->_name = std::move(name);
    this->isStdFunc = true;

    // Add all strings to args list:
    for (uint32_t i = 0; i < nargs; ++i)
    {
        this->_args.push_back(args[i]);
    }
}

// Build a function with no named args:
CppFunction::CppFunction(std::function<PackToken(TokenMap)> func, QString name)
    : stdFunc(std::move(func))
{
    this->_name = std::move(name);
    this->isStdFunc = true;
}

