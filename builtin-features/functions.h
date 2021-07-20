#ifndef CPARSE_BUILTIN_FUNCTION_H
#define CPARSE_BUILTIN_FUNCTION_H

#include <cmath>

#include "../cparse.h"
#include "../calculator.h"
#include "../containers.h"
#include "../functions.h"

namespace cparse::builtin_functions
{
    using namespace cparse;

    /* * * * * Built-in Functions: * * * * */

    PackToken default_print(const TokenMap & scope)
    {
        // Get the argument list:
        TokenList list = scope["args"].asList();

        bool first = true;

        auto dbg = qInfo();

        for (const PackToken & item : list.list())
        {
            if (first)
            {
                first = false;
            }
            else
            {
                dbg << " ";
            }

            if (item->m_type == TokenType::STR)
            {
                dbg << item.asString();
            }
            else
            {
                dbg << item.str();
            }
        }

        return PackToken::None();
    }

    PackToken default_sum(const TokenMap & scope)
    {
        // Get the arguments:
        TokenList list = scope["args"].asList();

        if (list.list().size() == 1 && list.list().front()->m_type == TokenType::LIST)
        {
            list = list.list().front().asList();
        }

        qreal sum = 0;

        for (const PackToken & num : list.list())
        {
            sum += num.asReal();
        }

        return PackToken(sum);
    }

    PackToken default_eval(const TokenMap & scope)
    {
        auto code = scope["value"].asString();
        // Evaluate it as a Calculator expression:
        return Calculator::calculate(code.toStdString().c_str(), scope);
    }

    PackToken default_real(const TokenMap & scope)
    {
        PackToken tok = scope["value"];

        if (tok->m_type & TokenType::NUM)
        {
            return PackToken(tok.asReal());
        }

        if (!tok.canConvertToString())
        {
            qWarning(cparseLog) << "could not convert \"" << tok << "\" to qreal!";
            return PackToken::Error();
        }

        const QString & str = tok.asString();

        bool ok = false;
        auto ret = str.toDouble(&ok);

        if (!ok)
        {
            qWarning(cparseLog) << "failed to convert to real!";
            return PackToken::Error();
        }

        return PackToken(ret);
    }

    PackToken default_int(const TokenMap & scope)
    {
        PackToken tok = scope["value"];

        if (tok->m_type & TokenType::NUM)
        {
            return PackToken(tok.asInt());
        }

        const QString & str = tok.asString();

        bool ok = false;
        auto ret = str.toInt(&ok);

        if (!ok)
        {
            qWarning(cparseLog) << "failed to convert to int!";
            return PackToken::Error();
        }

        return PackToken(ret);
    }

    PackToken default_str(const TokenMap & scope)
    {
        // Return its string representation:
        PackToken tok = scope["value"];

        if (tok->m_type == TokenType::STR)
        {
            return tok;
        }

        return PackToken(tok.str());
    }

    PackToken default_type(const TokenMap & scope)
    {
        PackToken tok = scope["value"];
        PackToken * p_type;

        switch (tok->m_type)
        {
            case TokenType::NONE:
                return PackToken("none");

            case TokenType::VAR:
                return PackToken("variable");

            case TokenType::REAL:
                return PackToken("real");

            case TokenType::INT:
                return PackToken("integer");

            case TokenType::BOOL:
                return PackToken("boolean");

            case TokenType::STR:
                return PackToken("string");

            case TokenType::FUNC:
                return PackToken("function");

            case TokenType::IT:
                return PackToken("iterable");

            case TokenType::TUPLE:
                return PackToken("tuple");

            case TokenType::STUPLE:
                return PackToken("argument tuple");

            case TokenType::LIST:
                return PackToken("list");

            case TokenType::MAP:
                p_type = tok.asMap().find("__type__");

                if (p_type && (*p_type)->m_type == TokenType::STR)
                {
                    return *p_type;
                }

                return PackToken("map");

            default:
                return PackToken("unknown_type");
        }
    }

    PackToken default_sqrt(const TokenMap & scope)
    {
        // Get a single argument:
        auto number = scope["num"].asReal();
        return PackToken(sqrt(number));
    }
    PackToken default_sin(const TokenMap & scope)
    {
        // Get a single argument:
        auto number = scope["num"].asReal();
        return PackToken(sin(number));
    }
    PackToken default_cos(const TokenMap & scope)
    {
        // Get a single argument:
        auto number = scope["num"].asReal();
        return PackToken(cos(number));
    }
    PackToken default_tan(const TokenMap & scope)
    {
        // Get a single argument:
        auto number = scope["num"].asReal();
        return PackToken(tan(number));
    }
    PackToken default_abs(const TokenMap & scope)
    {
        // Get a single argument:
        auto number = scope["num"].asReal();
        return PackToken(std::abs(number));
    }

    const FunctionArgs pow_args = {"number", "exp"};
    PackToken default_pow(const TokenMap & scope)
    {
        // Get two arguments:
        auto number = scope["number"].asReal();
        auto exp = scope["exp"].asReal();

        return PackToken(pow(number, exp));
    }

    const FunctionArgs min_max_args = {"left", "right"};
    PackToken default_max(const TokenMap & scope)
    {
        // Get two arguments:
        auto left = scope["left"].asReal();
        auto right = scope["right"].asReal();

        return PackToken(std::max(left, right));
    }

    PackToken default_min(const TokenMap & scope)
    {
        // Get two arguments:
        auto left = scope["left"].asReal();
        auto right = scope["right"].asReal();

        return PackToken(std::min(left, right));
    }

    /* * * * * default constructor functions * * * * */

    PackToken default_list(const TokenMap & scope)
    {
        // Get the arguments:
        TokenList list = scope["args"].asList();

        // If the only argument is iterable:
        if (list.list().size() == 1 && list.list()[0]->m_type & TokenType::IT)
        {
            TokenList new_list;
            TokenIterator * it = static_cast<IterableToken *>(list.list()[0].token())->getIterator();

            PackToken * next = it->next();

            while (next)
            {
                new_list.list().push_back(*next);
                next = it->next();
            }

            delete it;
            return new_list;
        }

        return list;
    }

    PackToken default_map(const TokenMap & scope)
    {
        return scope["kwargs"];
    }

    /* * * * * Object inheritance tools: * * * * */

    PackToken default_extend(const TokenMap & scope)
    {
        PackToken tok = scope["value"];

        if (tok->m_type == TokenType::MAP)
        {
            return tok.asMap().getChild();
        }

        qWarning(cparseLog) << tok.str() << " is not extensible!";
        return PackToken::Error();
    }

    // Example of replacement function for PackToken::str():
    QString PackToken_str(const Token * base, quint32 nest)
    {
        const Function * func;

        // Find the TokenMap with the type specific functions
        // for the type of the base token:
        const TokenMap * typeFuncs;

        if (base->m_type == TokenType::MAP)
        {
            typeFuncs = static_cast<const TokenMap *>(base);
        }
        else
        {
            typeFuncs = &ObjectTypeRegistry::typeMap(base->m_type);
        }

        // Check if this type has a custom stringify function:
        const PackToken * p_func = typeFuncs->find("__str__");

        if (p_func && (*p_func)->m_type == TokenType::FUNC)
        {
            // Return the result of this function passing the
            // nesting level as first (and only) argument:
            func = p_func->asFunc();
            PackToken _this = PackToken(base->clone());
            TokenList args;
            args.push(PackToken(static_cast<qint64>(nest)));
            return Function::call(_this, func, &args, TokenMap()).asString();
        }

        // Return "" to ask for the normal `PackToken::str()`
        // function to complete the job.
        return QString();
    }

    struct Register
    {
        Register(Config & config, Config::BuiltInDefinition def)
        {
            TokenMap & scope = config.scope;

            if (def & Config::BuiltInDefinition::SystemFunctions)
            {
                scope["print"] = CppFunction(&default_print, "print");
            }

            if (def & Config::BuiltInDefinition::MathFunctions)
            {
                scope["sum"] = CppFunction(&default_sum, "sum");
                scope["sqrt"] = CppFunction(&default_sqrt, {"num"}, "sqrt");
                scope["sin"] = CppFunction(&default_sin, {"num"}, "sin");
                scope["cos"] = CppFunction(&default_cos, {"num"}, "cos");
                scope["tan"] = CppFunction(&default_tan, {"num"}, "tan");
                scope["abs"] = CppFunction(&default_abs, {"num"}, "abs");
                scope["pow"] = CppFunction(&default_pow, pow_args, "pow");
                scope["min"] = CppFunction(&default_min, min_max_args, "min");
                scope["max"] = CppFunction(&default_max, min_max_args, "max");
                scope["float"] = CppFunction(&default_real, {"value"}, "float");
                scope["real"] = CppFunction(&default_real, {"value"}, "real");
                scope["double"] = CppFunction(&default_real, {"value"}, "double");
                scope["int"] = CppFunction(&default_int, {"value"}, "int");
            }

            if (def & Config::BuiltInDefinition::SystemFunctions)
            {
                scope["str"] = CppFunction(&default_str, {"value"}, "str");
                scope["eval"] = CppFunction(&default_eval, {"value"}, "eval");
                scope["type"] = CppFunction(&default_type, {"value"}, "type");
                scope["extend"] = CppFunction(&default_extend, {"value"}, "extend");
            }

            if (def & Config::BuiltInDefinition::ObjectOperators)
            {
                // Default constructors:
                scope["list"] = CppFunction(&default_list, "list");
                scope["map"] = CppFunction(&default_map, "map");
            }

            // Set the custom str function to `PackToken_str()`
            PackToken::str_custom() = PackToken_str;
        }
    };

}  // namespace builtin_functions

#endif // CPARSE_BUILTIN_FUNCTION_H
