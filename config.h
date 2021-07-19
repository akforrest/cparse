#ifndef CPARSE_CONFIG_H
#define CPARSE_CONFIG_H

#include <map>
#include <set>
#include <vector>

#include <QString>

#include "operation.h"

namespace cparse
{
    class RpnBuilder;

    // The reservedWordParser_t is the function type called when
    // a reserved word or character is found at parsing time.
    using WordParserFunc = void (const char *, const char **, RpnBuilder *);
    using WordParserFuncMap = std::map<QString, WordParserFunc *>;
    using CharParserFuncMap = std::map<char, WordParserFunc *>;

    struct ParserMap
    {
        WordParserFuncMap wmap;
        CharParserFuncMap cmap;

        // Add reserved word:
        void add(const QString & word, WordParserFunc * parser);

        // Add reserved character:
        void add(char c,  WordParserFunc * parser);

        WordParserFunc * find(const QString & text);
        WordParserFunc * find(char c);
    };

    class OpPrecedenceMap
    {
        public:

            OpPrecedenceMap();

            void add(const QString & op, int precedence);

            void addUnary(const QString & op, int precedence);

            void addRightUnary(const QString & op, int precedence);

            int prec(const QString & op) const;
            bool assoc(const QString & op) const;
            bool exists(const QString & op) const;

        private:

            // Set of operators that should be evaluated from right to left:
            std::set<QString> RtoL;
            // Map of operators precedence:
            std::map<QString, int> pr_map;
    };

    class OpMap : public std::map<QString, std::vector<Operation>>
    {
        public:

            void add(const opSignature_t & sig, Operation::opFunc_t func);
            QString str() const;
    };

    struct Config
    {
        Config() {}
        Config(ParserMap p, OpPrecedenceMap opp, OpMap opMap);

        ParserMap parserMap;
        OpPrecedenceMap opPrecedence;
        OpMap opMap;
    };
}
#endif // CPARSE_CONFIG_H
