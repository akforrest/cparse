#ifndef CPARSE_OPERATION_H
#define CPARSE_OPERATION_H

#include "packtoken.h"
#include "containers.h"

#include <set>

namespace cparse
{
    class RefToken;

    struct opSignature_t
    {
        TokenType left;
        QString op;
        TokenType right;
        opSignature_t(TokenType L, const QString & op, TokenType R);
    };

    using opID_t = quint64;

    class OpMap;
    struct evaluationData
    {
        TokenQueue rpn;
        TokenMap scope;
        const OpMap & opMap;

        std::unique_ptr<RefToken> left;
        std::unique_ptr<RefToken> right;

        QString op;
        opID_t opID{};

        evaluationData(TokenQueue rpn, const TokenMap & scope, const OpMap & opMap);
    };

    class Operation
    {
        public:
            using opFunc_t = PackToken(*)(const PackToken &, const PackToken &, evaluationData *);

        public:
            // Use this exception to reject an operation.
            // Without stoping the operation matching process.
            struct Reject : public std::exception {};

        public:
            static inline uint32_t mask(TokenType type);
            static opID_t build_mask(TokenType left, TokenType right);

        private:
            opID_t _mask;
            opFunc_t _exec;

        public:
            Operation(const opSignature_t & sig, opFunc_t func);

        public:
            opID_t getMask() const;
            PackToken exec(const PackToken & left, const PackToken & right,
                           evaluationData * data) const;
    };

    class OpMap : public std::map<QString, std::vector<Operation>>
    {
        public:

            void add(const opSignature_t & sig, Operation::opFunc_t func);
            QString str() const;
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
            std::set<QString> m_rtol;
            // Map of operators precedence:
            std::map<QString, int> m_prMap;
    };
}

#endif // CPARSE_OPERATION_H
