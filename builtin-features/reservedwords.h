#ifndef CPARSE_BUILTIN_RESERVEDWORDS_H
#define CPARSE_BUILTIN_RESERVEDWORDS_H

#include "../calculator.h"
#include "../containers.h"
#include "../functions.h"
#include "../shuntingyard.h"
#include "../shuntingyardexceptions.h"

namespace cparse::builtin_reservedWords
{
    using namespace cparse;
    // Literal Tokens: True, False and None:
    PackToken trueToken = PackToken(true);
    PackToken falseToken = PackToken(false);
    PackToken noneToken = PackToken::None();

    void True(const char *, const char **, RpnBuilder * data)
    {
        data->handle_token(trueToken->clone());
    }

    void False(const char *, const char **, RpnBuilder * data)
    {
        data->handle_token(falseToken->clone());
    }

    void None(const char *, const char **, RpnBuilder * data)
    {
        data->handle_token(noneToken->clone());
    }

    void LineComment(const char * expr, const char ** rest, RpnBuilder *)
    {
        while (*expr && *expr != '\n')
        {
            ++expr;
        }

        *rest = expr;
    }

    void SlashStarComment(const char * expr, const char ** rest, RpnBuilder *)
    {
        while (*expr && !(expr[0] == '*' && expr[1] == '/'))
        {
            ++expr;
        }

        if (*expr == '\0')
        {
            throw syntax_error("Unexpected end of file after '/*' comment!");
        }

        // Drop the characters `*/`:
        expr += 2;
        *rest = expr;
    }

    void KeywordOperator(const char *, const char **, RpnBuilder * data)
    {
        // Convert any STuple like `a : 10` to `'a': 10`:
        if (data->rpn.back()->m_type == VAR)
        {
            data->rpn.back()->m_type = STR;
        }

        data->handle_op(":");
    }

    void DotOperator(const char * expr, const char ** rest, RpnBuilder * data)
    {
        data->handle_op(".");

        while (*expr && isspace(*expr))
        {
            ++expr;
        }

        // If it did not find a valid variable name after it:
        if (!RpnBuilder::isvarchar(*expr))
        {
            throw syntax_error("Expected variable name after '.' operator");
        }

        // Parse the variable name and save it as a string:
        auto key = RpnBuilder::parseVar(expr, rest);
        data->handle_token(new TokenTyped<QString>(key, STR));
    }

    struct Startup
    {
        Startup()
        {
            ParserMap & parser = Calculator::defaultConfig().parserMap;
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
    } __CPARSE_STARTUP;

}  // namespace builtin_reservedWords

#endif // CPARSE_BUILTIN_RESERVEDWORDS_H
