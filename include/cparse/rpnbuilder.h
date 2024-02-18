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

namespace cparse {
    // This struct was created to expose internal toRPN() variables
    // to custom parsers, in special to the rWordParser_t functions.
    class RpnBuilder
    {
    public:
        static TokenQueue toRPN(const QString &expr, const TokenMap &vars, const QString &delim, int *rest, const Config &config);

        static Token *calculate(const TokenQueue &RPN, const TokenMap &scope, const Config &config = Config::defaultConfig());

        static void clearRPN(TokenQueue *rpn);

        // Check if a character is the first character of a variable:
        static bool isVariableNameChar(QChar c);
        static QString parseVariableName(const QChar *expr, const QChar *exprEnd, const QChar **rest, bool allowDigits, bool allowDots);

        bool handleOp(const QString &op);
        bool handleToken(Token *token);
        bool openBracket(const QString &bracket);
        bool closeBracket(const QString &bracket);

        TokenType lastTokenType() const;
        void setLastTokenType(TokenType type);

    private:
        RpnBuilder(const OpPrecedenceMap &opp) : m_opp(opp) { }

        void processOpStack();

        QString topOp() const;

        uint32_t bracketLevel() const;

        bool lastTokenWasOp() const;
        bool lastTokenWasUnary() const;

        const TokenQueue &rpn() const;

        bool opExists(const QString &op) const;

        void clear();

        void handleOpStack(const QString &op);
        void handleBinary(const QString &op);
        void handleLeftUnary(const QString &op);
        void handleRightUnary(const QString &op);

        TokenQueue m_rpn;
        std::stack<QString> m_opStack;
        uint8_t m_lastTokenWasOp = true;
        bool m_lastTokenWasUnary = false;
        const OpPrecedenceMap &m_opp;

        // Used to make sure the expression won't
        // end inside a bracket evaluation just because
        // found a delimiter like '\n' or ')'
        uint32_t m_bracketLevel = 0;
    };

} // namespace cparse

#endif // CPARSE_SHUNTING_YARD_H_
