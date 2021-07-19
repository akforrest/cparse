#ifndef CPARSE_CONFIG_H
#define CPARSE_CONFIG_H

#include <map>
#include <set>
#include <vector>

#include <QString>

#include "operation.h"

namespace cparse
{
    class rpnBuilder;

    // The reservedWordParser_t is the function type called when
    // a reserved word or character is found at parsing time.
    using rWordParser_t = void (const char *, const char **, rpnBuilder *);
    using rWordMap_t = std::map<QString, rWordParser_t *>;
    using rCharMap_t = std::map<char, rWordParser_t *>;

    struct parserMap_t
    {
        rWordMap_t wmap;
        rCharMap_t cmap;

        // Add reserved word:
        void add(const QString & word,  rWordParser_t * parser);

        // Add reserved character:
        void add(char c,  rWordParser_t * parser);

        rWordParser_t * find(const QString & text);

        rWordParser_t * find(char c);
    };

    class OppMap_t
    {
            // Set of operators that should be evaluated from right to left:
            std::set<QString> RtoL;
            // Map of operators precedence:
            std::map<QString, int> pr_map;

        public:
            OppMap_t();

            void add(const QString & op, int precedence);

            void addUnary(const QString & op, int precedence);

            void addRightUnary(const QString & op, int precedence);

            int prec(const QString & op) const;
            bool assoc(const QString & op) const;
            bool exists(const QString & op) const;
    };

    using opList_t = std::vector<Operation>;
    class opMap_t : public std::map<QString, opList_t>
    {
        public:
            void add(const opSignature_t & sig, Operation::opFunc_t func);

            QString str() const;
    };

    struct Config_t
    {
        parserMap_t parserMap;
        OppMap_t opPrecedence;
        opMap_t opMap;

        Config_t() {}
        Config_t(parserMap_t p, OppMap_t opp, opMap_t opMap);
    };
}
#endif // CPARSE_CONFIG_H
