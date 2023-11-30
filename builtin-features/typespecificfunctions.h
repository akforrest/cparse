#ifndef CPARSE_BUILTIN_TYPESPECIFICFUNCTIONS_H
#define CPARSE_BUILTIN_TYPESPECIFICFUNCTIONS_H

#include "cparse/calculator.h"
#include "cparse/containers.h"
#include "cparse/functions.h"

namespace cparse::builtin_typeSpecificFunctions {
    using namespace cparse;

    /* * * * * MAP Type built-in functions * * * * */

    const FunctionArgs map_pop_args = {"key", "default"};
    PackToken map_pop(const TokenMap &scope)
    {
        TokenMap map = scope["this"].asMap();
        QString key = scope["key"].asString();

        // Check if the item is available and remove it:
        if (map.map().count(key)) {
            PackToken value = map[key];
            map.erase(key);
            return value;
        }

        // If not available return the default value or None
        auto def = scope.find("default");

        if (def) {
            return *def;
        } else {
            return PackToken::None();
        }
    }

    PackToken map_len(const TokenMap &scope)
    {
        TokenMap map = scope.find("this")->asMap();
        return map.map().size();
    }

    PackToken default_instanceof(const TokenMap &scope)
    {
        TokenMap _super = scope["value"].asMap();
        TokenMap *_this = scope["this"].asMap().parent();

        TokenMap *parent = _this;

        while (parent) {
            if ((*parent) == _super) {
                return true;
            }

            parent = parent->parent();
        }

        return false;
    }

    /* * * * * LIST Type built-in functions * * * * */

    const FunctionArgs push_args = {"item"};
    PackToken list_push(const TokenMap &scope)
    {
        auto list = scope.find("this");
        auto token = scope.find("item");

        // If "this" is not a list it will throw here:
        list->asList().list().push_back(*token);

        return *list;
    }

    const FunctionArgs list_pop_args = {"pos"};
    PackToken list_pop(const TokenMap &scope)
    {
        TokenList list = scope.find("this")->asList();
        auto token = scope.find("pos");

        qint64 pos;

        if ((*token)->m_type & NUM) {
            pos = token->asInt();

            // So that pop(-1) is the same as pop(last_idx):
            if (pos < 0) {
                pos = list.list().size() - pos;
            }
        } else {
            pos = list.list().size() - 1;
        }

        PackToken result = list.list()[pos];

        // Erase the item from the list:
        // Note that this operation is optimal if pos == list.size()-1
        list.list().erase(list.list().begin() + pos);

        return result;
    }

    PackToken list_len(const TokenMap &scope)
    {
        TokenList list = scope.find("this")->asList();
        return list.list().size();
    }

    PackToken list_join(const TokenMap &scope)
    {
        TokenList list = scope["this"].asList();
        QString chars = scope["chars"].asString();

        QStringList strList;

        for (const auto &it : list.list()) {
            strList << it.asString();
        }

        return strList.join(chars);
    }

    /* * * * * STR Type built-in functions * * * * */

    PackToken string_len(const TokenMap &scope)
    {
        QString str = scope["this"].asString();
        return str.length();
    }

    PackToken string_lower(const TokenMap &scope)
    {
        QString str = scope["this"].asString();
        return str.toLower();
    }

    PackToken string_upper(const TokenMap &scope)
    {
        QString str = scope["this"].asString();
        return str.toUpper();
    }

    PackToken string_strip(const TokenMap &scope)
    {
        QString str = scope["this"].asString();
        return str.trimmed();
    }

    PackToken string_split(const TokenMap &scope)
    {
        TokenList list;
        QString str = scope["this"].asString();
        QString split_chars = scope["chars"].asString();

        auto split = str.split(split_chars);

        for (auto ss : split) {
            list.push(ss);
        }

        return list;
    }

    /* * * * * Type-Specific Functions Startup: * * * * */

    struct Register
    {
        Register(Config &, Config::BuiltInDefinition)
        {
            // we only register type definitions for internal types here
            static bool init = false;

            if (init) {
                return;
            }

            init = true;

            TokenMap &listScope = ObjectTypeRegistry::typeMap(LIST);
            listScope["push"] = CppFunction(list_push, push_args, "push");
            listScope["pop"] = CppFunction(list_pop, list_pop_args, "pop");
            listScope["len"] = CppFunction(list_len, "len");
            listScope["join"] = CppFunction(list_join, {"chars"}, "join");

            TokenMap &strScope = ObjectTypeRegistry::typeMap(STR);
            strScope["len"] = CppFunction(&string_len, "len");
            strScope["lower"] = CppFunction(&string_lower, "lower");
            strScope["upper"] = CppFunction(&string_upper, "upper");
            strScope["strip"] = CppFunction(&string_strip, "strip");
            strScope["split"] = CppFunction(&string_split, {"chars"}, "split");

            TokenMap &mapScope = ObjectTypeRegistry::typeMap(MAP);
            mapScope["pop"] = CppFunction(map_pop, map_pop_args, "pop");
            mapScope["len"] = CppFunction(map_len, "len");
            mapScope["instanceof"] = CppFunction(&default_instanceof, {"value"}, "instanceof");
        }
    };

} // namespace builtin_typeSpecificFunctions

#endif // CPARSE_BUILTIN_TYPESPECIFICFUNCTIONS_H
