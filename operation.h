#ifndef CPARSE_OPERATION_H
#define CPARSE_OPERATION_H

#include "packtoken.h"
#include "containers.h"

#include <set>

namespace cparse
{
    class RefToken;

    using OpId = quint64;

    struct OpSignature
    {
        TokenType left;
        QString op;
        TokenType right;
        OpSignature(TokenType L, const QString & op, TokenType R);
    };

    class OpMap;
    struct EvaluationData
    {
        TokenQueue rpn;
        TokenMap scope;
        const OpMap & opMap;

        std::unique_ptr<RefToken> left;
        std::unique_ptr<RefToken> right;

        QString op;
        OpId opID{};

        EvaluationData(TokenQueue rpn, const TokenMap & scope, const OpMap & opMap);
    };

    class Operation
    {
        public:

            using OpFunc = PackToken(*)(const PackToken &, const PackToken &, EvaluationData *);

            Operation(const OpSignature & sig, OpFunc func);

            static inline uint32_t mask(TokenType type);
            static OpId buildMask(TokenType left, TokenType right);

            OpId getMask() const;

            PackToken exec(const PackToken & left,
                           const PackToken & right,
                           EvaluationData * data) const;

        private:

            OpId m_mask;
            OpFunc m_exec;
    };

    class OpMap : public std::map<QString, std::vector<Operation>>
    {
        public:

            void add(const OpSignature & sig, Operation::OpFunc func);
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
