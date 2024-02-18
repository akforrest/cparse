#ifndef CPARSE_CONFIG_H
#define CPARSE_CONFIG_H

#include <map>
#include <set>
#include <vector>

#include <QString>

#include "operation.h"

namespace cparse {
    class RpnBuilder;

    // WordParserFunc is the function type called when
    // a reserved word or character is found at parsing time

    using WordParserFunc = bool(const QChar *expression, const QChar *expressionEnd, const QChar **rest, RpnBuilder *);
    using WordParserFuncMap = std::map<QString, WordParserFunc *>;
    using CharParserFuncMap = std::map<QChar, WordParserFunc *>;

    class ParserMap
    {
    public:
        // Add reserved word:
        void add(const QString &word, WordParserFunc *parser);

        // Add reserved character:
        void add(QChar c, WordParserFunc *parser);

        WordParserFunc *find(const QString &text) const;
        WordParserFunc *find(QChar c) const;

    private:
        WordParserFuncMap wmap;
        CharParserFuncMap cmap;
    };

    using TokenTypeMap = std::map<TokenType, TokenMap>;

    class Config
    {
    public:
        enum BuiltInDefinition {
            NumberConstants = 1 << 0, // e.g. pi
            NumberOperators = 1 << 1, // e.g. +, -
            LogicalOperators = 1 << 2, // || == etc
            ObjectOperators = 1 << 3, // [], {}, join, .
            MathFunctions = 1 << 4, // sin, tan, max
            SystemFunctions = 1 << 5, // print
            AllDefinitions = NumberConstants | NumberOperators | LogicalOperators | ObjectOperators | MathFunctions | SystemFunctions
        };

        Config() = default;
        Config(TokenMap scope, ParserMap p, OpPrecedenceMap opp, OpMap opMap);

        void registerBuiltInDefinitions(BuiltInDefinition def);

        static Config &defaultConfig();

        TokenMap scope;
        ParserMap parserMap;
        OpPrecedenceMap opPrecedence;
        OpMap opMap;
        std::function<PackToken(const QString &)> variableResolver;
    };

    class ObjectTypeRegistry
    {
    public:
        static TokenMap &typeMap(TokenType type);

    private:
        ObjectTypeRegistry() = default;
    };

    Config::BuiltInDefinition operator|(Config::BuiltInDefinition l, Config::BuiltInDefinition r);
}
#endif // CPARSE_CONFIG_H
