#ifndef TOKENHELPERS_H
#define TOKENHELPERS_H

#include "tokenbase.h"
#include "containers.h"

#include <stack>

namespace cparse
{

    // Use this function to discard a reference to an object
    // And obtain the original TokenBase*.
    // Please note that it only deletes memory if the token
    // is of type REF.
    TokenBase * resolve_reference(TokenBase * b, TokenMap * scope = nullptr);
    void cleanStack(std::stack<TokenBase *> st);

    inline QString normalize_op(QString op)
    {
        if (op[0] == 'L' || op[0] == 'R')
        {
            op.remove(0, 1);
            return op;
        }

        return op;
    }
}

#endif // TOKENHELPERS_H
