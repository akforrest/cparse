#ifndef CPARSE_OPERATION_H
#define CPARSE_OPERATION_H

#include "packtoken.h"
#include "containers.h"

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
}

#endif // CPARSE_OPERATION_H
