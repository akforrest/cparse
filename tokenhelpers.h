#ifndef CPARSE_TOKENHELPERS_H
#define CPARSE_TOKENHELPERS_H

#include "token.h"
#include "containers.h"

#include <stack>

namespace cparse
{

    // Use this function to discard a reference to an object
    // And obtain the original Token*.
    // Please note that it only deletes memory if the token
    // is of type REF.
    Token * resolveReferenceToken(Token * b, const TokenMap * localScope = nullptr, const TokenMap * configScope = nullptr);
    void cleanStack(std::stack<Token *> st);

    inline QString normalizeOp(QString op)
    {
        if (op[0] == 'L' || op[0] == 'R')
        {
            op.remove(0, 1);
            return op;
        }

        return op;
    }
}

#endif // CPARSE_TOKENHELPERS_H
