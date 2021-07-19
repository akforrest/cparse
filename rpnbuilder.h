#ifndef CPARSE_SHUNTING_YARD_H_
#define CPARSE_SHUNTING_YARD_H_

#include <iostream>

#include <map>
#include <stack>
#include <queue>
#include <list>
#include <utility>
#include <vector>
#include <set>
#include <sstream>
#include <memory>
#include <utility>

#include <QString>

#include "token.h"
#include "tokentype.h"
#include "config.h"
#include "packtoken.h"
#include "containers.h"
#include "functions.h"

namespace cparse
{
    // This struct was created to expose internal toRPN() variables
    // to custom parsers, in special to the rWordParser_t functions.
    class RpnBuilder
    {
        public:

            RpnBuilder(const TokenMap & scope, const OpPrecedenceMap & opp) : scope(scope), opp(opp) {}

            TokenQueue rpn;
            std::stack<QString> opStack;
            uint8_t lastTokenWasOp = true;
            bool lastTokenWasUnary = false;
            TokenMap scope;
            const OpPrecedenceMap & opp;

            // Used to make sure the expression won't
            // end inside a bracket evaluation just because
            // found a delimiter like '\n' or ')'
            uint32_t bracketLevel = 0;

            static void cleanRPN(TokenQueue * rpn);

            void handle_op(const QString & op);
            void handle_token(Token * token);
            void open_bracket(const QString & bracket);
            void close_bracket(const QString & bracket);

            // * * * * * Static parsing helpers: * * * * * //

            // Check if a character is the first character of a variable:
            static bool isvarchar(char c);

            static QString parseVar(const char * expr, const char ** rest = nullptr);

        private:
            void handle_opStack(const QString & op);
            void handle_binary(const QString & op);
            void handle_left_unary(const QString & op);
            void handle_right_unary(const QString & op);
    };

}  // namespace cparse

#endif  // CPARSE_SHUNTING_YARD_H_
