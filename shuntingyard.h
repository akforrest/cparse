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

#include "tokenbase.h"
#include "tokentype.h"
#include "config.h"

namespace cparse
{
    class PackToken;

}  // namespace cparse

namespace cparse
{

    struct TokenMap;
    class TokenList;
    class Tuple;
    class STuple;
    class Function;

}

#include "./packtoken.h"

// Define the Tuple, TokenMap and TokenList classes:
#include "./containers.h"

// Define the `Function` class
// as well as some built-in functions:
#include "./functions.h"

namespace cparse
{
    // This struct was created to expose internal toRPN() variables
    // to custom parsers, in special to the rWordParser_t functions.
    class rpnBuilder
    {
        public:

            TokenQueue_t rpn;
            std::stack<QString> opStack;
            uint8_t lastTokenWasOp = true;
            bool lastTokenWasUnary = false;
            TokenMap scope;
            const OppMap_t & opp;

            // Used to make sure the expression won't
            // end inside a bracket evaluation just because
            // found a delimiter like '\n' or ')'
            uint32_t bracketLevel = 0;

            rpnBuilder(const TokenMap & scope, const OppMap_t & opp) : scope(scope), opp(opp) {}

            static void cleanRPN(TokenQueue_t * rpn);

        public:
            void handle_op(const QString & op);
            void handle_token(TokenBase * token);
            void open_bracket(const QString & bracket);
            void close_bracket(const QString & bracket);

            // * * * * * Static parsing helpers: * * * * * //

            // Check if a character is the first character of a variable:
            static bool isvarchar(const char c);

            static QString parseVar(const char * expr, const char ** rest = nullptr);

        private:
            void handle_opStack(const QString & op);
            void handle_binary(const QString & op);
            void handle_left_unary(const QString & op);
            void handle_right_unary(const QString & op);
    };

    class RefToken;
    class opMap_t;


    // The RefToken keeps information about the context
    // in which a variable was originally evaluated
    // and allow a final value to be correctly resolved
    // afterwards.
    class RefToken : public TokenBase
    {
            PackToken original_value;

        public:
            PackToken key;
            PackToken origin;
            RefToken(PackToken k, TokenBase * v, PackToken m = PackToken::None());
            RefToken(PackToken k = PackToken::None(), PackToken v = PackToken::None(), PackToken m = PackToken::None());

            TokenBase * resolve(TokenMap * localScope = nullptr) const;

            TokenBase * clone() const override;
    };

    template<class T>
    TokenBase * Token<T>::clone() const
    {
        return new Token(*this);
    }

}  // namespace cparse

#endif  // CPARSE_SHUNTING_YARD_H_
