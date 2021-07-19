#include <cmath>

namespace builtin_functions
{
    /* * * * * Built-in Functions: * * * * */

    PackToken default_print(TokenMap scope)
    {
        // Get the argument list:
        TokenList list = scope["args"].asList();

        bool first = true;

        auto dbg = qInfo();

        for (PackToken item : list.list())
        {
            if (first)
            {
                first = false;
            }
            else
            {
                dbg << " ";
            }

            if (item->type == STR)
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

    PackToken default_sum(TokenMap scope)
    {
        // Get the arguments:
        TokenList list = scope["args"].asList();

        if (list.list().size() == 1 && list.list().front()->type == LIST)
        {
            list = list.list().front().asList();
        }

        double sum = 0;

        for (PackToken num : list.list())
        {
            sum += num.asDouble();
        }

        return PackToken(sum);
    }

    PackToken default_eval(TokenMap scope)
    {
        auto code = scope["value"].asString();
        // Evaluate it as a Calculator expression:
        return Calculator::calculate(code.toStdString().c_str(), scope);
    }

    PackToken default_float(TokenMap scope)
    {
        PackToken tok = scope["value"];

        if (tok->type & NUM)
        {
            return PackToken(tok.asDouble());
        }

        // Convert it to double:
        char * rest;
        const QString & str = tok.asString();
        errno = 0;
        double ret = strtod(str.toStdString().c_str(), &rest);

        if (str == rest)
        {
            throw std::runtime_error("Could not convert \"" + str.toStdString() + "\" to float!");
        }
        else if (errno)
        {
            std::range_error("Value too big or too small to fit a Double!");
        }

        return PackToken(ret);
    }

    PackToken default_int(TokenMap scope)
    {
        PackToken tok = scope["value"];

        if (tok->type & NUM)
        {
            return PackToken(tok.asInt());
        }

        // Convert it to double:
        char * rest;
        const QString & str = tok.asString();
        errno = 0;
        int64_t ret = strtol(str.toStdString().c_str(), &rest, 10);

        if (str == rest)
        {
            throw std::runtime_error("Could not convert \"" + str.toStdString() + "\" to integer!");
        }
        else if (errno)
        {
            std::range_error("Value too big or too small to fit an Integer!");
        }

        return PackToken(ret);
    }

    PackToken default_str(TokenMap scope)
    {
        // Return its string representation:
        PackToken tok = scope["value"];

        if (tok->type == STR)
        {
            return tok;
        }

        return PackToken(tok.str());
    }

    PackToken default_type(TokenMap scope)
    {
        PackToken tok = scope["value"];
        PackToken * p_type;

        switch (tok->type)
        {
            case NONE:
                return PackToken("none");

            case VAR:
                return PackToken("variable");

            case REAL:
                return PackToken("real");

            case INT:
                return PackToken("integer");

            case BOOL:
                return PackToken("boolean");

            case STR:
                return PackToken("string");

            case FUNC:
                return PackToken("function");

            case IT:
                return PackToken("iterable");

            case TUPLE:
                return PackToken("tuple");

            case STUPLE:
                return PackToken("argument tuple");

            case LIST:
                return PackToken("list");

            case MAP:
                p_type = tok.asMap().find("__type__");

                if (p_type && (*p_type)->type == STR)
                {
                    return *p_type;
                }
                else
                {
                    return PackToken("map");
                }

            default:
                return PackToken("unknown_type");
        }
    }

    PackToken default_sqrt(TokenMap scope)
    {
        // Get a single argument:
        double number = scope["num"].asDouble();
        return PackToken(sqrt(number));
    }
    PackToken default_sin(TokenMap scope)
    {
        // Get a single argument:
        double number = scope["num"].asDouble();
        return PackToken(sin(number));
    }
    PackToken default_cos(TokenMap scope)
    {
        // Get a single argument:
        double number = scope["num"].asDouble();
        return PackToken(cos(number));
    }
    PackToken default_tan(TokenMap scope)
    {
        // Get a single argument:
        double number = scope["num"].asDouble();
        return PackToken(tan(number));
    }
    PackToken default_abs(TokenMap scope)
    {
        // Get a single argument:
        double number = scope["num"].asDouble();
        return PackToken(std::abs(number));
    }

    const args_t pow_args = {"number", "exp"};
    PackToken default_pow(TokenMap scope)
    {
        // Get two arguments:
        double number = scope["number"].asDouble();
        double exp = scope["exp"].asDouble();

        return PackToken(pow(number, exp));
    }

    const args_t min_max_args = {"left", "right"};
    PackToken default_max(TokenMap scope)
    {
        // Get two arguments:
        double left = scope["left"].asDouble();
        double right = scope["right"].asDouble();

        return PackToken(std::max(left, right));
    }

    PackToken default_min(TokenMap scope)
    {
        // Get two arguments:
        double left = scope["left"].asDouble();
        double right = scope["right"].asDouble();

        return PackToken(std::min(left, right));
    }

    /* * * * * default constructor functions * * * * */

    PackToken default_list(TokenMap scope)
    {
        // Get the arguments:
        TokenList list = scope["args"].asList();

        // If the only argument is iterable:
        if (list.list().size() == 1 && list.list()[0]->type & IT)
        {
            TokenList new_list;
            Iterator * it = static_cast<Iterable *>(list.list()[0].token())->getIterator();

            PackToken * next = it->next();

            while (next)
            {
                new_list.list().push_back(*next);
                next = it->next();
            }

            delete it;
            return new_list;
        }
        else
        {
            return list;
        }
    }

    PackToken default_map(TokenMap scope)
    {
        return scope["kwargs"];
    }

    /* * * * * Object inheritance tools: * * * * */

    PackToken default_extend(TokenMap scope)
    {
        PackToken tok = scope["value"];

        if (tok->type == MAP)
        {
            return tok.asMap().getChild();
        }
        else
        {
            throw std::runtime_error(tok.str().toStdString() + " is not extensible!");
        }
    }

    // Example of replacement function for PackToken::str():
    QString PackToken_str(const TokenBase * base, uint32_t nest)
    {
        const Function * func;

        // Find the TokenMap with the type specific functions
        // for the type of the base token:
        const TokenMap * typeFuncs;

        if (base->type == MAP)
        {
            typeFuncs = static_cast<const TokenMap *>(base);
        }
        else
        {
            typeFuncs = &Calculator::type_attribute_map()[base->type];
        }

        // Check if this type has a custom stringify function:
        const PackToken * p_func = typeFuncs->find("__str__");

        if (p_func && (*p_func)->type == FUNC)
        {
            // Return the result of this function passing the
            // nesting level as first (and only) argument:
            func = p_func->asFunc();
            PackToken _this = PackToken(base->clone());
            TokenList args;
            args.push(PackToken(static_cast<int64_t>(nest)));
            return Function::call(_this, func, &args, TokenMap()).asString();
        }

        // Return "" to ask for the normal `PackToken::str()`
        // function to complete the job.
        return "";
    }

    struct Startup
    {
        Startup()
        {
            TokenMap & global = TokenMap::default_global();

            global["print"] = CppFunction(&default_print, "print");
            global["sum"] = CppFunction(&default_sum, "sum");
            global["sqrt"] = CppFunction(&default_sqrt, {"num"}, "sqrt");
            global["sin"] = CppFunction(&default_sin, {"num"}, "sin");
            global["cos"] = CppFunction(&default_cos, {"num"}, "cos");
            global["tan"] = CppFunction(&default_tan, {"num"}, "tan");
            global["abs"] = CppFunction(&default_abs, {"num"}, "abs");
            global["pow"] = CppFunction(&default_pow, pow_args, "pow");
            global["min"] = CppFunction(&default_min, min_max_args, "min");
            global["max"] = CppFunction(&default_max, min_max_args, "max");
            global["float"] = CppFunction(&default_float, {"value"}, "float");
            global["real"] = CppFunction(&default_float, {"value"}, "real");
            global["int"] = CppFunction(&default_int, {"value"}, "int");
            global["str"] = CppFunction(&default_str, {"value"}, "str");
            global["eval"] = CppFunction(&default_eval, {"value"}, "eval");
            global["type"] = CppFunction(&default_type, {"value"}, "type");
            global["extend"] = CppFunction(&default_extend, {"value"}, "extend");

            // Default constructors:
            global["list"] = CppFunction(&default_list, "list");
            global["map"] = CppFunction(&default_map, "map");

            // Set the custom str function to `PackToken_str()`
            PackToken::str_custom() = PackToken_str;
        }
    } __CPARSE_STARTUP;

}  // namespace builtin_functions
