#include "rpnbuilder.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <exception>
#include <locale>
#include <string>
#include <stack>
#include <utility> // For std::pair
#include <cstring> // For strchr()

#include "cparse.h"
#include "calculator.h"
#include "tokenhelpers.h"
#include "reftoken.h"

using namespace cparse;

Q_LOGGING_CATEGORY(cparseLog, "cparse")

namespace {
    using namespace cparse;

    [[maybe_unused]] void log_undefined_operation(const QString &op, const PackToken &left, const PackToken &right)
    {
        qWarning(cparseLog) << "Unexpected rpn operation with operator '" << op
                            << "' and operands: " << left.str() << " and " << right.str();
    }

    bool match_op_id(OpId id, OpId mask)
    {
        quint64 result = id & mask;
        auto *val = reinterpret_cast<uint32_t *>(&result);
        return (val[0] && val[1]);
    }

    Token *exec_operation(const PackToken &left, const PackToken &right, EvaluationData *data, const QString &OP_MASK)
    {
        auto it = data->opMap.find(OP_MASK);

        if (it == data->opMap.end()) {
            if (!OP_MASK.isEmpty()) {
                return exec_operation(left, right, data, {});
            }
            return nullptr;
        }

        for (const Operation &operation : it->second) {
            if (match_op_id(data->opID, operation.getMask())) {
                auto *execToken = operation.exec(left, right, data).release();

                if (execToken->m_type == TokenType::REJECT) {
                    continue;
                }

                return execToken;
            }
        }

        if (!OP_MASK.isEmpty()) {
            return exec_operation(left, right, data, {});
        }

        return nullptr;
    }
}

void cparse::initialize()
{
    Config::defaultConfig().registerBuiltInDefinitions(Config::BuiltInDefinition::AllDefinitions);
}

Token *cparse::resolveReferenceToken(Token *b, const TokenMap *localScope, const TokenMap *configScope)
{
    if (b->m_type & REF) {
        // Resolve the reference:
        auto *ref = static_cast<RefToken *>(b);
        Token *value = ref->resolve(localScope, configScope);

        delete ref;
        return value;
    }

    return b;
}

void cparse::cleanStack(std::stack<Token *> st)
{
    while (!st.empty()) {
        delete cparse::resolveReferenceToken(st.top());
        st.pop();
    }
}

/* * * * * Operation class: * * * * */

// Convert a type into an unique mask for bit wise operations:
uint32_t Operation::mask(TokenType type)
{
    if (type == ANY_TYPE) {
        return 0xFFFF;
    }

    return ((type & 0xE0) << 24) | (1 << (type & 0x1F));
}

// Build a mask for each pair of operands
OpId Operation::buildMask(TokenType left, TokenType right)
{
    OpId result = mask(left);
    return (result << 32) | mask(right);
}

Operation::Operation(const OpSignature &sig, OpFunc func)
    : m_mask(buildMask(sig.left, sig.right)), m_exec(func)
{
}

cparse::OpId Operation::getMask() const
{
    return m_mask;
}

PackToken Operation::exec(const PackToken &left, const PackToken &right, EvaluationData *data) const
{
    return m_exec(left, right, data);
}

/* * * * * rpnBuilder Class: * * * * */

void RpnBuilder::clearRPN(TokenQueue *rpn)
{
    while (!rpn->empty()) {
        delete cparse::resolveReferenceToken(rpn->front());
        rpn->pop();
    }
}

void RpnBuilder::clear()
{
    clearRPN(&m_rpn);
}

/**
 * Consume operators with precedence >= than op
 * and add them to the RPN
 *
 * The algorithm works as follows:
 *
 * Let p(o) denote the precedence of an operator o.
 *
 * If the token is an operator, o1, then
 *   While there is an operator token, o2, at the top
 *       and p(o1) >= p(o2) (`>` for Right to Left associativity)
 *     then:
 *
 *     pop o2 off the stack onto the output queue.
 *   Push o1 on the stack.
 */
void RpnBuilder::handleOpStack(const QString &op)
{
    QString cur_op;

    // If it associates from left to right:
    if (m_opp.assoc(op) == 0) {
        while (!m_opStack.empty() && m_opp.prec(op) >= m_opp.prec(m_opStack.top())) {
            cur_op = normalizeOp(m_opStack.top());
            m_rpn.push(new TokenTyped<QString>(cur_op, OP));
            m_opStack.pop();
        }
    } else {
        while (!m_opStack.empty() && m_opp.prec(op) > m_opp.prec(m_opStack.top())) {
            cur_op = normalizeOp(m_opStack.top());
            m_rpn.push(new TokenTyped<QString>(cur_op, OP));
            m_opStack.pop();
        }
    }
}

void RpnBuilder::handleBinary(const QString &op)
{
    // Handle OP precedence
    handleOpStack(op);
    // Then push the current op into the stack:
    m_opStack.push(op);
}

// Convert left unary operators to binary and handle them:
void RpnBuilder::handleLeftUnary(const QString &unary_op)
{
    this->m_rpn.push(new TokenUnary());
    // Only put it on the stack and wait to check op precedence:
    m_opStack.push(unary_op);
}

// Convert right unary operators to binary and handle them:
void RpnBuilder::handleRightUnary(const QString &unary_op)
{
    // Handle OP precedence:
    handleOpStack(unary_op);
    // Add the unary token:
    this->m_rpn.push(new TokenUnary());
    // Then add the current op directly into the rpn:
    m_rpn.push(new TokenTyped<QString>(normalizeOp(unary_op), OP));
}

namespace {
    using namespace cparse;
    PackToken defaultMapConstructor(const TokenMap &scope)
    {
        return scope["kwargs"];
    }

    PackToken defaultListConstructor(const TokenMap &scope)
    {
        // Get the arguments:
        TokenList list = scope["args"].asList();

        // If the only argument is iterable:
        if (list.list().size() == 1 && list.list()[0]->m_type & IT) {
            TokenList new_list;
            TokenIterator *it = static_cast<IterableToken *>(list.list()[0].token())->getIterator();

            PackToken *next = it->next();

            while (next) {
                new_list.list().push_back(*next);
                next = it->next();
            }

            delete it;
            return new_list;
        }

        return list;
    }
}

namespace {
    bool isDeliminator(QChar character, const QString &delimiters)
    {
        const QChar *delimiterData = delimiters.constData();
        const QChar *delimiterEnd = delimiterData + delimiters.size();

        while (delimiterData != delimiterEnd) {
            if (character == *delimiterData) {
                return true;
            }
            ++delimiterData;
        }
        return false;
    }

    qint64 extractInteger(const QChar *str, const QChar *strEnd, const QChar **iterOut, int base)
    {
        qint64 result = 0;
        const QChar *ptr = str;

        // Skip leading whitespace
        while (ptr != strEnd && ptr->isSpace())
            ++ptr;

        // Handle optional sign
        bool negative = false;
        if (*ptr == QChar('-')) {
            negative = true;
            ++ptr;
        } else if (*ptr == QChar('+')) {
            ++ptr;
        }

        // Handle optional base prefixes
        if (base == 0) {
            if (*ptr == QChar('0')) {
                if (*(ptr + 1) == QChar('x') || *(ptr + 1) == QChar('X')) {
                    base = 16;
                    ptr += 2;
                } else {
                    base = 8;
                    ++ptr;
                }
            } else {
                base = 10;
            }
        } else if (base == 16 && *ptr == QChar('0') && (*(ptr + 1) == QChar('x') || *(ptr + 1) == QChar('X'))) {
            ptr += 2;
        }

        // Parse digits
        while (ptr != strEnd) {
            qint64 digitValue;
            if (ptr->isDigit())
                digitValue = ptr->digitValue();
            else if (ptr->isLetter() && ptr->toLower().unicode() >= QChar('a').unicode()
                     && ptr->toLower().unicode() <= QChar('f').unicode())
                digitValue = 10 + ptr->toLower().unicode() - QChar('a').unicode();
            else
                break; // End of number

            if (digitValue >= base)
                break; // Invalid digit for base

            result = result * base + digitValue;
            ++ptr;
        }

        *iterOut = ptr;

        return (negative ? -result : result);
    }

    qreal extractReal(const QChar *str, const QChar *str_end, const QChar **str_end_out)
    {
        QString qstr;
        const QChar *ptr = str;

        // Skip leading whitespace
        while (ptr < str_end && ptr->isSpace())
            ++ptr;

        // Handle optional sign
        if (ptr < str_end && (*ptr == QChar('-') || *ptr == QChar('+'))) {
            qstr.append(*ptr);
            ++ptr;
        }

        // Parse digits and decimal point
        while (ptr < str_end && (ptr->isDigit() || *ptr == QChar('.'))) {
            qstr.append(*ptr);
            ++ptr;
        }

        // Parse exponent if present
        if (ptr < str_end && (*ptr == QChar('e') || *ptr == QChar('E'))) {
            qstr.append(*ptr);
            ++ptr;

            // Handle optional exponent sign
            if (ptr < str_end && (*ptr == QChar('-') || *ptr == QChar('+'))) {
                qstr.append(*ptr);
                ++ptr;
            }

            // Parse digits for exponent
            while (ptr < str_end && ptr->isDigit()) {
                qstr.append(*ptr);
                ++ptr;
            }
        }

        // Call QString::toDouble for conversion
        bool ok = false;
        qreal result = qstr.toDouble(&ok);
        if (!ok) {
            // Conversion failed, set result to NaN
            result = std::numeric_limits<qreal>::quiet_NaN();
        }

        *str_end_out = ptr;

        return result;
    }

    bool isPunctuation(const QChar &ch)
    {
        static const QString specialCharacters = "!\"#£$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
        return specialCharacters.contains(ch);
    }
}

// Work as a sub-parser:
// - Stops at delim or '\0'
// - Returns the rest of the string as char* rest
TokenQueue RpnBuilder::toRPN(const QString &exprStr, const TokenMap &vars, const QString &deliminators, int *rest, const Config &config)
{
    RpnBuilder data(config.opPrecedence);
    const QChar *nextChar = nullptr;

    const auto *expr = exprStr.constData();
    const auto *exprEnd = expr + exprStr.size();

    while (expr != exprEnd && expr->isSpace() && !isDeliminator(*expr, deliminators)) {
        ++expr;
    }

    if (expr == exprEnd || isDeliminator(*expr, deliminators)) {
        qWarning(cparseLog) << "Cannot build a Calculator from an empty expression!";
        return {};
    }

    // In one pass, ignore whitespace and parse the expression into RPN
    // using Dijkstra's Shunting-yard algorithm.
    while (expr != exprEnd && (data.bracketLevel() || !isDeliminator(*expr, deliminators))) {
        if (expr->isDigit()) {
            int base = 10;

            // Parse the prefix notation for octal and hex numbers:
            if (expr[0] == '0') {
                if (expr[1] == 'x') {
                    // 0x1 == 1 in hex notation
                    base = 16;
                    expr += 2;
                } else if (expr[1].isDigit()) {
                    // 01 == 1 in octal notation
                    base = 8;
                    expr++;
                }
            }

            // If the token is a number, add it to the output queue.
            qint64 _int = extractInteger(expr, exprEnd, &nextChar, base);

            const auto atEnd = nextChar == exprEnd;

            // If the number was not a float:
            if (base != 10 || atEnd || !isDeliminator(*nextChar, ".eE")) {
                if (!data.handleToken(new TokenTyped<qint64>(_int, INT))) {
                    return {};
                }
            } else {
                const auto intEnd = nextChar;
                qreal digit = extractReal(expr, exprEnd, &nextChar);

                if (nextChar == intEnd) { // no new chars parsed
                    if (!data.handleToken(new TokenTyped<qint64>(_int, INT))) {
                        return {};
                    }
                } else if (!data.handleToken(new TokenTyped<qreal>(digit, REAL))) {
                    return {};
                }
            }

            expr = nextChar;
        } else if (RpnBuilder::isVariableNameChar(*expr)) {
            WordParserFunc *parser;

            // If the token is a variable, resolve it and
            // add the parsed number to the output queue.
            const QChar *expr2 = expr;
            QString key = RpnBuilder::parseVariableName(expr, exprEnd, &expr, true, false);

            if ((parser = config.parserMap.find(key))) {
                // Parse reserved words:
                if (!parser(expr, exprEnd, &expr, &data)) {
                    data.clear();
                    return {};
                }
            } else {
                auto *value = vars.find(key);

                if (!value) {
                    value = config.scope.find(key);
                }

                if (value) {
                    // Save a reference token:
                    Token *copy = (*value)->clone();

                    if (!data.handleToken(new RefToken(PackToken(key), copy))) {
                        return {};
                    }
                } else {
                    // Save the variable name:
                    if (!data.m_lastTokenWasOp || !data.handleToken(new TokenTyped<QString>(key, VAR))) {
                        expr = expr2;
                        key = RpnBuilder::parseVariableName(expr, exprEnd, &expr, false, false);
                        // Check if the word parser applies:
                        auto *parser = config.parserMap.find(key);

                        // Evaluate the meaning of this operator in the following order:
                        // 1. Is there a word parser for it?
                        // 2. Is it a valid operator?
                        // 3. Is there a character parser for its first character?
                        if (parser) {
                            // Parse reserved operators:

                            if (!parser(expr, exprEnd, &expr, &data)) {
                                data.clear();
                                return {};
                            }
                        } else if (data.opExists(key)) {
                            if (!data.handleOp(key)) {
                                return {};
                            }
                        } else {
                            data.clear();
                            qWarning(cparseLog) << "Invalid variable name or operator: " + key;
                            return {};
                        }
                    }
                }
            }
        } else if (*expr == '\'' || *expr == '"') {
            // If it is a string literal, parse it and
            // add to the output queue.
            QChar quote = *expr;

            ++expr;
            QString ss;

            while (expr != exprEnd && *expr != quote && *expr != '\n') {
                if (*expr == '\\') {
                    switch (expr[1].unicode()) {
                    case 'n':
                        expr += 2;
                        ss += '\n';
                        break;

                    case 't':
                        expr += 2;
                        ss += '\t';
                        break;

                    default:
                        if (isDeliminator(expr[1], "\"'\n")) {
                            ++expr;
                        }

                        ss += *expr;
                        ++expr;
                    }
                } else {
                    ss += *expr;
                    ++expr;
                }
            }

            if (*expr != quote) {
                QString squote = (quote == '"' ? "\"" : "'");
                data.clear();
                qWarning(cparseLog) << "Expected quote (" + squote
                                + ") at end of string declaration: " + squote + ss + ".";
                return {};
            }

            ++expr;

            if (!data.handleToken(new TokenTyped<QString>(ss, STR))) {
                return {};
            }
        } else {
            // Otherwise, the variable is an operator or paranthesis.
            switch (expr->unicode()) {
            case '(':

                // If it is a function call:
                if (!data.lastTokenWasOp()) {
                    // This counts as a bracket and as an operator:
                    if (!data.handleOp("()")) {
                        return {};
                    }

                    // Add it as a bracket to the op stack:
                }

                data.openBracket("(");
                ++expr;
                break;

            case '[':
                if (!data.lastTokenWasOp()) {
                    // If it is an operator:
                    if (!data.handleOp("[]")) {
                        return {};
                    }
                } else {
                    // If it is the list constructor:
                    // Add the list constructor to the rpn:
                    if (!data.handleToken(new CppFunction(&defaultListConstructor, "list"))) {
                        return {};
                    }

                    // We make the program see it as a normal function call:
                    if (!data.handleOp("()")) {
                        return {};
                    }
                }

                // Add it as a bracket to the op stack:
                data.openBracket("[");
                ++expr;
                break;

            case '{':

                // Add a map constructor call to the rpn:
                if (!data.handleToken(new CppFunction(&defaultMapConstructor, "map"))) {
                    return {};
                }

                // We make the program see it as a normal function call:
                if (!data.handleOp("()")) {
                    return {};
                }

                if (!data.openBracket("{")) {
                    return {};
                }

                ++expr;
                break;

            case ')':
                if (!data.closeBracket("(")) {
                    return {};
                }

                ++expr;
                break;

            case ']':
                if (!data.closeBracket("[")) {
                    return {};
                }

                ++expr;
                break;

            case '}':
                if (!data.closeBracket("{")) {
                    return {};
                }

                ++expr;
                break;

            default: {
                // Then the token is an operator

                const QChar *start = expr;
                QString ss;
                ss += *expr;
                ++expr;

                while (expr != exprEnd && isPunctuation(*expr) && !isDeliminator(*expr, "+-'\"()[]{}_")) {
                    ss += *expr;
                    ++expr;
                }

                QString op = ss;

                // Check if the word parser applies:
                auto *parser = config.parserMap.find(op);

                // Evaluate the meaning of this operator in the following order:
                // 1. Is there a word parser for it?
                // 2. Is it a valid operator?
                // 3. Is there a character parser for its first character?
                if (parser) {
                    // Parse reserved operators:

                    if (!parser(expr, exprEnd, &expr, &data)) {
                        data.clear();
                        return {};
                    }
                } else if (data.opExists(op)) {
                    if (!data.handleOp(op)) {
                        return {};
                    }
                } else if ((parser = config.parserMap.find(QString(op[0])))) {
                    expr = start + 1;

                    if (!parser(expr, exprEnd, &expr, &data)) {
                        data.clear();
                        return {};
                    }
                } else {
                    qWarning(cparseLog) << "Invalid operator: " + op;
                    data.clear();
                    return {};
                }
            }
            }
        }

        // Ignore spaces but stop on delimiter if not inside brackets.
        while (expr != exprEnd && expr->isSpace() && (data.bracketLevel() || !isDeliminator(*expr, deliminators))) {
            ++expr;
        }
    }

    // Check for syntax errors (excess of operators i.e. 10 + + -1):
    if (data.lastTokenWasUnary()) {
        data.clear();
        qWarning(cparseLog) << "Expected operand after unary operator `" << data.topOp() << "`";
        return {};
    }

    data.processOpStack();

    if (rest) {
        *rest = expr - exprStr.constData();
    }

    return data.rpn();
}

Token *RpnBuilder::calculate(const TokenQueue &rpn, const TokenMap &scope, const Config &config)
{
    if (rpn.empty()) {
        return nullptr;
    }

    EvaluationData data(rpn, scope, config.opMap, config.variableResolver);

    // Evaluate the expression in RPN form.
    std::stack<Token *> evaluation;

    auto tryResolveVariable = [&](Token *base, const QString &key) -> bool {
        if (scope.find(key)) {
            evaluation.push(base);
            return true;
        }

        if (config.scope.find(key)) {
            evaluation.push(base);
            return true;
        }

        if (!config.variableResolver) {
            evaluation.push(base);
            return true;
        }

        auto resolverValue = config.variableResolver(key);

        if (resolverValue->m_type == TokenType::ERROR) {
            cleanStack(evaluation);
            return false;
        }

        if (resolverValue->m_type == TokenType::REJECT) {
            evaluation.push(base);
        } else {
            evaluation.push(new RefToken(PackToken(key), resolverValue->clone()));
            delete base;
        }
        return true;
    };

    while (!data.rpn.empty()) {
        Token *base = data.rpn.front()->clone();
        data.rpn.pop();

        // Operator:
        if (base->m_type == OP) {
            data.op = static_cast<TokenTyped<QString> *>(base)->m_val;
            delete base;

            /* * * * * Resolve operands Values and References: * * * * */

            if (evaluation.size() < 2) {
                cleanStack(evaluation);
                qWarning(cparseLog) << "Invalid equation.";
                return nullptr;
            }

            Token *r_token = evaluation.top();
            evaluation.pop();
            Token *l_token = evaluation.top();
            evaluation.pop();

            if (r_token->m_type & REF) {
                data.right.reset(static_cast<RefToken *>(r_token));
                r_token = data.right->resolve(&data.scope, &config.scope);
            } else if (r_token->m_type == VAR) {
                auto key = PackToken(static_cast<TokenTyped<QString> *>(r_token)->m_val);
                data.right = std::make_unique<RefToken>(key);
            } else {
                data.right = std::make_unique<RefToken>();
            }

            if (l_token->m_type & REF) {
                data.left.reset(static_cast<RefToken *>(l_token));
                l_token = data.left->resolve(&data.scope, &config.scope);
            } else if (l_token->m_type == VAR) {
                auto key = PackToken(static_cast<TokenTyped<QString> *>(l_token)->m_val);
                data.left = std::make_unique<RefToken>(key);
            } else {
                data.left = std::make_unique<RefToken>();
            }

            if (l_token->m_type == FUNC && data.op == "()") {
                // * * * * * Resolve Function Calls: * * * * * //

                auto *l_func = static_cast<Function *>(l_token);

                // Collect the parameter tuple:
                Tuple right;

                if (r_token->m_type == TUPLE) {
                    right = *static_cast<Tuple *>(r_token);
                } else {
                    right = Tuple(r_token);
                }

                delete r_token;

                PackToken _this;

                if (data.left->m_origin->m_type != NONE) {
                    _this = data.left->m_origin;
                } else {
                    _this = data.scope;
                }

                // Execute the function:
                PackToken ret = Function::call(_this, l_func, &right, data.scope);

                if (ret->m_type == TokenType::ERROR) {
                    cleanStack(evaluation);
                    delete l_func;
                    return ret->clone();
                }

                delete l_func;
                evaluation.push(ret->clone());
            } else {
                // * * * * * Resolve All Other Operations: * * * * * //

                data.opID = Operation::buildMask(l_token->m_type, r_token->m_type);
                PackToken l_pack(l_token);
                PackToken r_pack(r_token);

                // Resolve the operation:
                Token *result = exec_operation(l_pack, r_pack, &data, data.op);

                if (result) {
                    if (result->m_type == TokenType::ERROR) {
                        cleanStack(evaluation);
                        return result;
                    }

                    if (result->m_type == TokenType::VAR) {
                        // op returned variable which we can now try to resolve;
                        const auto varName = static_cast<TokenTyped<QString> *>(result)->m_val;

                        if (!tryResolveVariable(result, varName)) {
                            return new TokenError("failed to resolve variable: " + varName);
                        }
                        continue;
                    }

                    evaluation.push(result);
                } else {
                    cleanStack(evaluation);
                    log_undefined_operation(data.op, l_pack, r_pack);
                    return new TokenError("failed to execute op: " + data.op);
                }
            }
        } else if (base->m_type == VAR) {
            PackToken *value = nullptr;
            QString key = static_cast<TokenTyped<QString> *>(base)->m_val;

            // if the next thing in evaluation is a '.' op do not resolve this yet
            // . is either a map name, in which case we do not need to resolve the variable
            // probably because it is already a map token type, or it a external variable name
            // like env.ENV_VAR, in which case we do not want to resolve the right hand side of this in the
            // wrong context

            if (!data.rpn.empty() && data.rpn.front()->m_type == TokenType::OP) {
                auto op = static_cast<TokenTyped<QString> *>(data.rpn.front())->m_val;

                if (op == ".") {
                    evaluation.push(base);
                    continue;
                }
            }

            value = data.scope.find(key);

            if (value) {
                Token *copy = (*value)->clone();
                evaluation.push(new RefToken(PackToken(key), copy));
                delete base;
            } else {
                if (!tryResolveVariable(base, key)) {
                    return new TokenError("failed to resolve variable: " + key);
                }
            }
        } else {
            evaluation.push(base);
        }
    }

    return evaluation.empty() ? nullptr : evaluation.top();
}

void RpnBuilder::processOpStack()
{
    while (!m_opStack.empty()) {
        QString cur_op = normalizeOp(m_opStack.top());
        m_rpn.push(new TokenTyped<QString>(cur_op, OP));
        m_opStack.pop();
    }

    // In case one of the custom parsers left an empty expression:
    if (m_rpn.empty()) {
        m_rpn.push(new TokenNone());
    }
}

cparse::TokenType RpnBuilder::lastTokenType() const
{
    return m_rpn.back()->m_type;
}

void RpnBuilder::setLastTokenType(TokenType type)
{
    m_rpn.back()->m_type = type;
}

QString RpnBuilder::topOp() const
{
    return m_opStack.top();
}

uint32_t RpnBuilder::bracketLevel() const
{
    return m_bracketLevel;
}

bool RpnBuilder::lastTokenWasOp() const
{
    return m_lastTokenWasOp;
}

bool RpnBuilder::lastTokenWasUnary() const
{
    return m_lastTokenWasUnary;
}

const cparse::TokenQueue &RpnBuilder::rpn() const
{
    return m_rpn;
}

bool RpnBuilder::opExists(const QString &op) const
{
    return m_opp.exists(op);
}

bool RpnBuilder::handleOp(const QString &op)
{
    // If it's a left unary operator:
    if (this->m_lastTokenWasOp) {
        if (m_opp.exists("L" + op)) {
            handleLeftUnary("L" + op);
            this->m_lastTokenWasUnary = true;
            this->m_lastTokenWasOp = op[0].unicode();
        } else {
            clearRPN(&(this->m_rpn));
            qWarning(cparseLog) << "Unrecognized unary operator: '" << op << "'.";
            return false;
        }

        // If its a right unary operator:
    } else if (m_opp.exists("R" + op)) {
        handleRightUnary("R" + op);

        // Set it to false, since we have already added
        // an unary token and operand to the stack:
        this->m_lastTokenWasUnary = false;
        this->m_lastTokenWasOp = false;

        // If it is a binary operator:
    } else {
        if (m_opp.exists(op)) {
            handleBinary(op);
        } else {
            clearRPN(&(m_rpn));
            qWarning(cparseLog) << "Undefined operator: `" << op << "`!";
            return false;
        }

        this->m_lastTokenWasUnary = false;
        this->m_lastTokenWasOp = op[0].unicode();
    }

    return true;
}

bool RpnBuilder::handleToken(Token *token)
{
    if (!m_lastTokenWasOp) {
        qWarning(cparseLog) << "Expected an operator or bracket but got " << PackToken::str(token);
        delete token;
        return false;
    }

    m_rpn.push(token);
    m_lastTokenWasOp = false;
    m_lastTokenWasUnary = false;
    return true;
}

bool RpnBuilder::openBracket(const QString &bracket)
{
    m_opStack.push(bracket);
    m_lastTokenWasOp = bracket[0].unicode();
    m_lastTokenWasUnary = false;
    ++m_bracketLevel;
    return true;
}

bool RpnBuilder::closeBracket(const QString &bracket)
{
    if (char(m_lastTokenWasOp) == bracket[0]) {
        m_rpn.push(new Tuple());
    }

    QString cur_op;

    while (!m_opStack.empty() && m_opStack.top() != bracket) {
        cur_op = normalizeOp(m_opStack.top());
        m_rpn.push(new TokenTyped<QString>(cur_op, OP));
        m_opStack.pop();
    }

    if (m_opStack.empty()) {
        RpnBuilder::clearRPN(&m_rpn);
        qWarning(cparseLog) << "Extra '" + bracket + "' on the expression!";
        return false;
    }

    m_opStack.pop();
    m_lastTokenWasOp = false;
    m_lastTokenWasUnary = false;
    --m_bracketLevel;
    return true;
}

bool RpnBuilder::isVariableNameChar(const QChar c)
{
    return c.isLetter() || c == '_' || c == '$' || c == '#' || c == '@';
}

QString RpnBuilder::parseVariableName(const QChar *expr, const QChar *exprEnd, const QChar **rest, bool allowDigits, bool allowDots)
{
    QString ss;
    ss += *expr;
    ++expr;

    while (expr != exprEnd && RpnBuilder::isVariableNameChar(*expr) || (allowDigits && expr->isDigit())
           || (allowDots && *expr == '.')) {
        ss += *expr;
        ++expr;
    }

    if (rest) {
        *rest = expr;
    }

    return ss;
}

void cleanStack(std::stack<Token *> st)
{
    while (!st.empty()) {
        delete resolveReferenceToken(st.top());
        st.pop();
    }
}

cparse::OpPrecedenceMap::OpPrecedenceMap()
{
    // These operations are hard-coded inside the Calculator,
    // thus their precedence should always be defined:
    m_prMap["[]"] = -1;
    m_prMap["()"] = -1;
    m_prMap["["] = 0x7FFFFFFF;
    m_prMap["("] = 0x7FFFFFFF;
    m_prMap["{"] = 0x7FFFFFFF;
    m_rtol.insert("=");
}

void cparse::OpPrecedenceMap::add(const QString &op, int precedence)
{
    if (precedence < 0) {
        m_rtol.insert(op);
        precedence = -precedence;
    }

    m_prMap[op] = precedence;
}

void cparse::OpPrecedenceMap::addUnary(const QString &op, int precedence)
{
    add("L" + op, precedence);

    // Also add a binary operator with same precedence so
    // it is possible to verify if an op exists just by checking
    // the binary set of operators:
    if (!exists(op)) {
        add(op, precedence);
    }
}

void cparse::OpPrecedenceMap::addRightUnary(const QString &op, int precedence)
{
    add("R" + op, precedence);

    // Also add a binary operator with same precedence so
    // it is possible to verify if an op exists just by checking
    // the binary set of operators:
    if (!exists(op)) {
        add(op, precedence);
    } else {
        // Note that using a unary and binary operators with
        // the same left operand is ambiguous and that the unary
        // operator will take precedence.
        //
        // So only do it if you know the expected left operands
        // have distinct types.
    }
}

int cparse::OpPrecedenceMap::prec(const QString &op) const
{
    return m_prMap.at(op);
}

bool cparse::OpPrecedenceMap::assoc(const QString &op) const
{
    return m_rtol.count(op);
}

bool cparse::OpPrecedenceMap::exists(const QString &op) const
{
    return m_prMap.count(op);
}

EvaluationData::EvaluationData(TokenQueue rpn,
                               const TokenMap &scope,
                               const OpMap &opMap,
                               const std::function<PackToken(const QString &)> &func)
    : rpn(std::move(rpn)), scope(scope), opMap(opMap), variableResolver(func)
{
}

cparse::OpSignature::OpSignature(const TokenType L, const QString &op, const TokenType R)
    : left(L), op(op), right(R)
{
}

void cparse::OpMap::add(const OpSignature &sig, Operation::OpFunc func)
{
    (*this)[sig.op].push_back(Operation(sig, func));
}

QString cparse::OpMap::str() const
{
    if (this->empty()) {
        return "{}";
    }

    QString result = "{ ";

    for (const auto &pair : (*this)) {
        result += "\"" + pair.first + "\", ";
    }

    result.chop(2);
    return result + " }";
}
