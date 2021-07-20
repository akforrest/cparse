#ifndef CPARSE_BUILTIN_RESERVEDWORDS_H
#define CPARSE_BUILTIN_RESERVEDWORDS_H

#include "../cparse.h"
#include "../calculator.h"
#include "../containers.h"
#include "../functions.h"
#include "../rpnbuilder.h"

namespace cparse::builtin_reservedWords
{
    using namespace cparse;
    // Literal Tokens: True, False and None:
    PackToken trueToken = PackToken(true);
    PackToken falseToken = PackToken(false);
    PackToken noneToken = PackToken::None();

    bool True(const char *, const char **, RpnBuilder * data)
    {
        return data->handleToken(trueToken->clone());
    }

    bool False(const char *, const char **, RpnBuilder * data)
    {
        return data->handleToken(falseToken->clone());
    }

    bool None(const char *, const char **, RpnBuilder * data)
    {
        return data->handleToken(noneToken->clone());
    }

    bool LineComment(const char * expr, const char ** rest, RpnBuilder *)
    {
        while (*expr && *expr != '\n')
        {
            ++expr;
        }

        *rest = expr;
        return true;
    }

    bool SlashStarComment(const char * expr, const char ** rest, RpnBuilder *)
    {
        while (*expr && !(expr[0] == '*' && expr[1] == '/'))
        {
            ++expr;
        }

        if (*expr == '\0')
        {
            qWarning(cparseLog) << "Unexpected end of file after '/*' comment!";
            return false;
        }

        // Drop the characters `*/`:
        expr += 2;
        *rest = expr;
        return true;
    }

    bool KeywordOperator(const char *, const char **, RpnBuilder * data)
    {
        // Convert any STuple like `a : 10` to `'a': 10`:
        if (data->lastTokenType() == VAR)
        {
            data->setLastTokenType(STR);
        }

        return data->handleOp(":");
    }

    bool DotOperator(const char * expr, const char ** rest, RpnBuilder * data)
    {
        data->handleOp(".");

        while (*expr && isspace(*expr))
        {
            ++expr;
        }

        // If it did not find a valid variable name after it:
        if (!RpnBuilder::isVariableNameChar(*expr))
        {
            qWarning(cparseLog) << "Expected variable name after '.' operator";
            return false;
        }

        // Parse the variable name and save it as a string:
        auto key = RpnBuilder::parseVariableName(expr, rest);
        return data->handleToken(new TokenTyped<QString>(key, STR));
    }

    struct Register
    {
        Register(Config & config, Config::BuiltInDefinition def)
        {
            ParserMap & parser = Config::defaultConfig().parserMap;
            parser.add("True", &True);
            parser.add("False", &False);
            parser.add("None", &None);
            parser.add("#", &LineComment);
            parser.add("//", &LineComment);
            parser.add("/*", &SlashStarComment);
            parser.add(":", &KeywordOperator);
            parser.add(':', &KeywordOperator);
            parser.add(".", &DotOperator);
            parser.add('.', &DotOperator);
        }
    };

}  // namespace builtin_reservedWords

#endif // CPARSE_BUILTIN_RESERVEDWORDS_H
