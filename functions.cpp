#include <string>
#include <utility>

#include "cparse.h"
#include "rpnbuilder.h"
#include "functions.h"

using namespace cparse;

/* * * * * class Function * * * * */
PackToken Function::call(const PackToken & _this, const Function * func,
                         TokenList * args, const TokenMap & scope)
{
    // Build the local namespace:
    TokenMap kwargs;
    TokenMap local(scope);

    FunctionArgs arg_names = func->args();

    auto args_it = args->list().begin();
    FunctionArgs::const_iterator names_it = arg_names.begin();

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
            qWarning(cparseLog) << "Positional argument follows keyword argument";
            return PackToken::Error();
        }

        auto * st = static_cast<STuple *>(arg.token());

        if (st->list().size() != 2)
        {
            qWarning(cparseLog) << "Keyword tuples must have exactly 2 items";
            return PackToken::Error();
        }

        if (st->list()[0]->m_type != STR)
        {
            qWarning(cparseLog) << "Keyword first argument should be of type string";
            return PackToken::Error();
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
    this->m_name = "";
    this->m_isStdFunc = false;
    this->m_func = nullptr;
}

CppFunction::CppFunction(PackToken(*func)(const TokenMap &), const FunctionArgs & args,
                         QString name)
    : m_func(func), m_args(args)
{
    this->m_name = std::move(name);
    this->m_isStdFunc = false;
}

CppFunction::CppFunction(PackToken(*func)(const TokenMap &), unsigned int nargs,
                         const char ** args, QString name)
    : m_func(func)
{
    this->m_name = std::move(name);
    this->m_isStdFunc = false;

    // Add all strings to args list:
    for (uint32_t i = 0; i < nargs; ++i)
    {
        this->m_args.push_back(args[i]);
    }
}

// Build a function with no named args:
CppFunction::CppFunction(PackToken(*func)(const TokenMap &), QString name)
    : m_func(func)
{
    this->m_name = std::move(name);
    this->m_isStdFunc = false;
}


CppFunction::CppFunction(std::function<PackToken(const TokenMap &)> func, const FunctionArgs & args,
                         QString name)
    : m_stdFunc(std::move(func)), m_args(args)
{
    this->m_name = std::move(name);
    this->m_isStdFunc = true;
}

CppFunction::CppFunction(const FunctionArgs & args, std::function<PackToken(const TokenMap &)> func,
                         QString name)
    : m_stdFunc(std::move(func)), m_args(args)
{
    this->m_name = std::move(name);
    this->m_isStdFunc = true;
}

CppFunction::CppFunction(std::function<PackToken(const TokenMap &)> func, unsigned int nargs,
                         const char ** args, QString name)
    : m_stdFunc(std::move(func))
{
    this->m_name = std::move(name);
    this->m_isStdFunc = true;

    // Add all strings to args list:
    for (uint32_t i = 0; i < nargs; ++i)
    {
        this->m_args.push_back(args[i]);
    }
}

// Build a function with no named args:
CppFunction::CppFunction(std::function<PackToken(const TokenMap &)> func, QString name)
    : m_stdFunc(std::move(func))
{
    this->m_name = std::move(name);
    this->m_isStdFunc = true;
}

