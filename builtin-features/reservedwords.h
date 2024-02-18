#ifndef CPARSE_BUILTIN_RESERVEDWORDS_H
#define CPARSE_BUILTIN_RESERVEDWORDS_H

#include "cparse/cparse.h"
#include "cparse/calculator.h"
#include "cparse/containers.h"
#include "cparse/functions.h"
#include "cparse/rpnbuilder.h"

#include <locale>

namespace cparse::builtin_reservedWords {
    using namespace cparse;
    // Literal Tokens: True, False and None:
    PackToken trueToken = PackToken(true);
    PackToken falseToken = PackToken(false);
    PackToken noneToken = PackToken::None();

    bool True(const QChar *, const QChar *, const QChar **, RpnBuilder *data)
    {
        return data->handleToken(trueToken->clone());
    }

    bool False(const QChar *, const QChar *, const QChar **, RpnBuilder *data)
    {
        return data->handleToken(falseToken->clone());
    }

    bool None(const QChar *, const QChar *, const QChar **, RpnBuilder *data)
    {
        return data->handleToken(noneToken->clone());
    }

    bool LineComment(const QChar *expr, const QChar *expressionEnd, const QChar **rest, RpnBuilder *)
    {
        while (expr != expressionEnd && *expr != '\n') {
            ++expr;
        }

        *rest = expr;
        return true;
    }

    bool SlashStarComment(const QChar *expr, const QChar *expressionEnd, const QChar **rest, RpnBuilder *)
    {
        while (expr != expressionEnd && !(expr[0] == '*' && expr[1] == '/')) {
            ++expr;
        }

        if (expr == expressionEnd) {
            qWarning(cparseLog) << "Unexpected end of file after '/*' comment!";
            return false;
        }

        // Drop the characters `*/`:
        expr += 2;
        *rest = expr;
        return true;
    }

    bool KeywordOperator(const QChar *, const QChar *, const QChar **, RpnBuilder *data)
    {
        // Convert any STuple like `a : 10` to `'a': 10`:
        if (data->lastTokenType() == VAR) {
            data->setLastTokenType(STR);
        }

        return data->handleOp(":");
    }

    bool DotOperator(const QChar *expr, const QChar *expressionEnd, const QChar **rest, RpnBuilder *data)
    {
        data->handleOp(".");

        while (expr != expressionEnd && expr->isSpace()) {
            ++expr;
        }

        // If it did not find a valid variable name after it:
        if (expr == expressionEnd || !RpnBuilder::isVariableNameChar(*expr)) {
            qWarning(cparseLog) << "Expected variable name after '.' operator";
            return false;
        }

        // Parse the variable name and save it as a string:
        auto key = RpnBuilder::parseVariableName(expr, expressionEnd, rest, true, false);
        return data->handleToken(new TokenTyped<QString>(key, STR));
    }

    struct Register
    {
        Register(Config &config, Config::BuiltInDefinition def)
        {
            ParserMap &parser = config.parserMap;

            if (def & Config::BuiltInDefinition::LogicalOperators) {
                parser.add("true", &True);
                parser.add("false", &False);
            }

            if (def & Config::BuiltInDefinition::SystemFunctions) {
                parser.add("none", &None);
                parser.add("#", &LineComment);
                parser.add("//", &LineComment);
                parser.add("/*", &SlashStarComment);
                parser.add(":", &KeywordOperator);
                parser.add(':', &KeywordOperator);
                parser.add(".", &DotOperator);
                parser.add('.', &DotOperator);
            }

            if (def & Config::BuiltInDefinition::NumberConstants) {
                config.scope["pi"] = std::atan(1) * 4;
            }
        }
    };

} // namespace builtin_reservedWords

#endif // CPARSE_BUILTIN_RESERVEDWORDS_H
