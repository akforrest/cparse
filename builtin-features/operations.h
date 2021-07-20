#ifndef CPARSE_BUILTIN_OPERATIONS_H
#define CPARSE_BUILTIN_OPERATIONS_H

#include <cmath>

#include "../cparse.h"
#include "../calculator.h"
#include "../containers.h"
#include "../functions.h"
#include "../rpnbuilder.h"
#include "../reftoken.h"

namespace cparse::builtin_operations
{
    [[maybe_unused]] static void log_undefined_operation(const QString & op, const PackToken & left, const PackToken & right)
    {
        qWarning(cparseLog) << "Unexpected operation with operator '" << op << "' and operands: " << left.str() << " and " << right.str();
    }

    using namespace cparse;
    // Assignment operator "="
    PackToken Assign(const PackToken &, const PackToken & right, EvaluationData * data)
    {
        const PackToken & key = data->left->m_key;
        const PackToken & origin = data->left->m_origin;

        // If the left operand has a name:
        if (key->m_type == TokenType::STR)
        {
            auto var_name = key.asString();

            // If it is an attribute of a TokenMap:
            if (origin->m_type == TokenType::MAP)
            {
                TokenMap & map = origin.asMap();
                map[var_name] = right;
            }
            else
            {
                // store the override of this in the local scope map
                // this will override vars passed in for the evaluation but
                // not the config level scope
                data->scope[var_name] = right;
            }

            // If the left operand has an index number:
        }
        else if (key->m_type & TokenType::NUM)
        {
            if (origin->m_type == TokenType::LIST)
            {
                TokenList & list = origin.asList();
                size_t index = key.asInt();
                list[index] = right;
            }
            else
            {
                qWarning(cparseLog) << "left operand of assignment is not a list";
                return PackToken::Error();
            }
        }
        else
        {
            log_undefined_operation(data->op, key, right);
            return PackToken::Error();
        }

        return right;
    }

    PackToken Comma(const PackToken & left, const PackToken & right, EvaluationData *)
    {
        if (left->m_type == TokenType::TUPLE)
        {
            left.asTuple().list().push_back(right);
            return left;
        }

        return Tuple(left, right);
    }

    PackToken Colon(const PackToken & left, const PackToken & right, EvaluationData *)
    {
        if (left->m_type == TokenType::STUPLE)
        {
            left.asSTuple().list().push_back(right);
            return left;
        }

        return STuple(left, right);
    }

    PackToken Equal(const PackToken & left, const PackToken & right, EvaluationData * data)
    {
        if (left->m_type == TokenType::VAR || right->m_type == TokenType::VAR)
        {
            log_undefined_operation(data->op, left, right);
            return PackToken::Error();
        }

        return PackToken(left == right);
    }

    PackToken Different(const PackToken & left, const PackToken & right, EvaluationData * data)
    {
        if (left->m_type == TokenType::VAR || right->m_type == TokenType::VAR)
        {
            log_undefined_operation(data->op, left, right);
            return PackToken::Error();
        }

        return PackToken(left != right);
    }

    PackToken MapIndex(const PackToken & p_left, const PackToken & p_right, EvaluationData * data)
    {
        if (!p_left.canConvertToMap() ||
            !p_right.canConvertToString())
        {
            return PackToken::Reject();
        }

        TokenMap & left = p_left.asMap();
        auto right = p_right.asString();
        const auto & op = data->op;

        if (op == "[]" || op == ".")
        {
            PackToken * p_value = left.find(right);

            if (p_value)
            {
                return RefToken(right, *p_value, left);
            }

            return RefToken(right, PackToken::None(), left);
        }

        log_undefined_operation(op, left, right);
        return PackToken::Error();
    }

    // Resolve build-in operations for non-map types, e.g.: 'str'.len()
    PackToken TypeSpecificFunction(const PackToken & p_left, const PackToken & p_right, EvaluationData * data)
    {
        if (p_left->m_type == TokenType::MAP ||
            !p_right.canConvertToString())
        {
            return PackToken::Reject();
        }

        TokenMap & attr_map = ObjectTypeRegistry::typeMap(p_left->m_type);
        QString key = p_right.asString();

        PackToken * attr = attr_map.find(key);

        if (attr)
        {
            // Note: If attr is a function, it will receive have
            // scope["this"] == source, so it can make changes on this object.
            // Or just read some information for example: its length.
            return RefToken(key, (*attr), p_left);
        }

        log_undefined_operation(data->op, p_left, p_right);
        return PackToken::Error();
    }

    PackToken UnaryNumeralOperation(const PackToken & left, const PackToken & right, EvaluationData * data)
    {
        const QString & op = data->op;

        if (op == "+")
        {
            return right;
        }

        if (op == "-")
        {
            if (!right.canConvertToReal())
            {
                return false;
            }

            return -right.asReal();
        }

        log_undefined_operation(data->op, left, right);
        return PackToken::Error();
    }

    PackToken NumeralOperation(const PackToken & left, const PackToken & right, EvaluationData * data)
    {
        if (!left.canConvertToReal() ||
            !right.canConvertToReal())
        {
            return PackToken::Error();
        }

        qreal left_d, right_d;
        qint64 left_i, right_i;

        // Extract integer and real values of the operators:
        left_d = left.asReal();
        left_i = left.asInt();

        right_d = right.asReal();
        right_i = right.asInt();

        const QString & op = data->op;

        if (op == "+")
        {
            return left_d + right_d;
        }

        if (op == "*")
        {
            return left_d * right_d;
        }

        if (op == "-")
        {
            return left_d - right_d;
        }

        if (op == "/")
        {
            return left_d / right_d;
        }

        if (op == "<<")
        {
            return left_i << right_i;
        }

        if (op == "**")
        {
            return pow(left_d, right_d);
        }

        if (op == ">>")
        {
            return left_i >> right_i;
        }

        if (op == "%")
        {
            return left_i % right_i;
        }

        if (op == "<")
        {
            return left_d < right_d;
        }

        if (op == ">")
        {
            return left_d > right_d;
        }

        if (op == "<=")
        {
            return left_d <= right_d;
        }

        if (op == ">=")
        {
            return left_d >= right_d;
        }

        if (op == "&&")
        {
            return left_i && right_i;
        }

        if (op == "||")
        {
            return left_i || right_i;
        }

        log_undefined_operation(op, left, right);
        return PackToken::Error();
    }

    PackToken FormatOperation(const PackToken & p_left, const PackToken & p_right, EvaluationData *)
    {
        if (!p_left.canConvertToString())
        {
            return PackToken::Error();
        }

        QString s_left = p_left.asString();
        auto stdstring = s_left.toStdString();
        const char * left = stdstring.c_str();

        Tuple right;

        if (p_right->m_type == TokenType::TUPLE)
        {
            right = p_right.asTuple();
        }
        else
        {
            right = Tuple(p_right);
        }

        QString result;

        for (const PackToken & token : right.list())
        {
            // Find the next occurrence of "%s"
            while (*left && (*left != '%' || left[1] != 's'))
            {
                if (*left == '\\' && left[1] == '%')
                {
                    ++left;
                }

                result.push_back(*left);
                ++left;
            }

            if (*left == '\0')
            {
                qWarning(cparseLog) << "Not all arguments converted during string formatting";
                return PackToken::Error();
            }

            left += 2;

            // Replace it by the token string representation:
            if (token->m_type == TokenType::STR)
            {
                // Avoid using PackToken::str for strings
                // or it will enclose it quotes `"str"`
                result += token.asString();
            }
            else
            {
                result += token.str();
            }
        }

        // Find the next occurrence of "%s" if exists:
        while (*left && (*left != '%' || left[1] != 's'))
        {
            if (*left == '\\' && left[1] == '%')
            {
                ++left;
            }

            result.push_back(*left);
            ++left;
        }

        if (*left != '\0')
        {
            qWarning(cparseLog) << "Not enough arguments for format string";
            return PackToken::Error();
        }

        return result;
    }

    PackToken StringOnStringOperation(const PackToken & p_left, const PackToken & p_right, EvaluationData * data)
    {
        if (!p_left.canConvertToString() ||
            !p_right.canConvertToString())
        {
            return PackToken::Error();
        }

        const QString & left = p_left.asString();
        const QString & right = p_right.asString();
        const QString & op = data->op;

        if (op == "+")
        {
            return left + right;
        }

        if (op == "==")
        {
            return (left == right);
        }

        if (op == "!=")
        {
            return (left != right);
        }

        log_undefined_operation(op, p_left, p_right);
        return PackToken::Error();
    }

    PackToken StringOnNumberOperation(const PackToken & p_left, const PackToken & p_right, EvaluationData * data)
    {
        if (!p_left.canConvertToString())
        {
            return PackToken::Error();
        }

        const QString & left = p_left.asString();
        const QString & op = data->op;

        if (op == "+")
        {
            if (!p_right.canConvertToReal())
            {
                return PackToken::Error();
            }

            return left + QString::number(p_right.asReal());
        }

        if (op == "[]")
        {
            if (!p_right.canConvertToInt())
            {
                return PackToken::Error();
            }

            auto index = p_right.asInt();

            if (index < 0)
            {
                // Reverse index, i.e. list[-1] = list[list.size()-1]
                index += left.size();
            }

            if (index < 0 || static_cast<size_t>(index) >= left.size())
            {
                qWarning(cparseLog) << "String index out of range";
                return PackToken::Error();
            }

            return QString(left.at(index));
        }

        log_undefined_operation(op, p_left, p_right);
        return PackToken::Error();
    }

    PackToken NumberOnStringOperation(const PackToken & p_left, const PackToken & p_right, EvaluationData * data)
    {
        if (!p_left.canConvertToReal() || !p_right.canConvertToString())
        {
            return PackToken::Error();
        }

        auto left = p_left.asReal();
        auto right = p_right.asString();

        if (data->op == "+")
        {
            return QString::number(left) + right;
        }

        log_undefined_operation(data->op, p_left, p_right);
        return PackToken::Error();
    }

    PackToken ListOnNumberOperation(const PackToken & p_left, const PackToken & p_right, EvaluationData * data)
    {
        if (!p_left.canConvertToList())
        {
            return PackToken::Error();
        }

        TokenList left = p_left.asList();

        if (data->op == "[]" && p_right.canConvertToInt())
        {
            auto index = p_right.asInt();

            if (index < 0)
            {
                // Reverse index, i.e. list[-1] = list[list.size()-1]
                index += left.list().size();
            }

            if (index < 0 || static_cast<size_t>(index) >= left.list().size())
            {
                qWarning(cparseLog) << "List index out of range!";
                return PackToken::Error();
            }

            PackToken & value = left.list()[index];

            return RefToken(index, value, p_left);
        }

        log_undefined_operation(data->op, p_left, p_right);
        return PackToken::Error();
    }

    PackToken ListOnListOperation(const PackToken & p_left, const PackToken & p_right, EvaluationData * data)
    {
        if (!p_left.canConvertToList() || !p_right.canConvertToList())
        {
            return PackToken::Error();
        }

        TokenList & left = p_left.asList();
        TokenList & right = p_right.asList();

        if (data->op == "+")
        {
            // Deep copy the first list:
            TokenList result;
            result.list() = left.list();

            // Insert items from the right list into the left:
            for (PackToken & p : right.list())
            {
                result.list().push_back(p);
            }

            return result;
        }

        log_undefined_operation(data->op, left, right);
        return PackToken::Error();
    }

    struct Register
    {
        Register(Config & config, Config::BuiltInDefinition def)
        {
            // Create the operator precedence map based on C++ default
            // precedence order as described on cppreference website:
            // http://en.cppreference.com/w/cpp/language/operator_precedence
            OpPrecedenceMap & opp = config.opPrecedence;

            using BiType = Config::BuiltInDefinition;

            if (def & BiType::ObjectOperators)
            {
                opp.add("[]", 2);
                opp.add("()", 2);
                opp.add(".", 2);
            }

            if (def & BiType::NumberOperators)
            {
                opp.add("**", 3);
                opp.add("*",  5);
                opp.add("/", 5);
                opp.add("%", 5);
                opp.add("+",  6);
                opp.add("-", 6);
                opp.add("<<", 7);
                opp.add(">>", 7);
            }

            if (def & BiType::LogicalOperators)
            {
                opp.add("<",  8);
                opp.add("<=", 8);
                opp.add(">=", 8);
                opp.add(">", 8);
                opp.add("==", 9);
                opp.add("!=", 9);
                opp.add("&&", 13);
                opp.add("||", 14);
            }

            if (def & BiType::ObjectOperators)
            {
                opp.add("=", 15);
                opp.add(":", 15);
                opp.add(",", 16);
            }

            if (def & BiType::NumberOperators)
            {
                // Add unary operators:
                opp.addUnary("+",  3);
                opp.addUnary("-", 3);
            }

            // Link operations to respective operators:
            OpMap & opMap = config.opMap;

            if (def & BiType::ObjectOperators)
            {
                opMap.add({ANY_TYPE, "=", ANY_TYPE}, &Assign);
                opMap.add({ANY_TYPE, ",", ANY_TYPE}, &Comma);
                opMap.add({ANY_TYPE, ":", ANY_TYPE}, &Colon);
            }

            if (def & BiType::LogicalOperators)
            {
                opMap.add({ANY_TYPE, "==", ANY_TYPE}, &Equal);
                opMap.add({ANY_TYPE, "!=", ANY_TYPE}, &Different);
            }

            if (def & BiType::ObjectOperators)
            {
                opMap.add({MAP, "[]", STR}, &MapIndex);
                opMap.add({ANY_TYPE, ".", STR}, &TypeSpecificFunction);
                opMap.add({MAP, ".", STR}, &MapIndex);
            }

            if (def & BiType::SystemFunctions)
            {
                opMap.add({STR, "%", ANY_TYPE}, &FormatOperation);
            }

            auto ANY_OP = "";

            // Note: The order is important:

            if (def & BiType::NumberOperators)
            {
                opMap.add({NUM, ANY_OP, NUM}, &NumeralOperation);
                opMap.add({UNARY, ANY_OP, NUM}, &UnaryNumeralOperation);
            }

            if (def & BiType::NumberConstants ||
                def & BiType::SystemFunctions ||
                def & BiType::ObjectOperators)
            {
                opMap.add({STR, ANY_OP, NUM}, &StringOnNumberOperation);
                opMap.add({NUM, ANY_OP, STR}, &NumberOnStringOperation);
                opMap.add({STR, ANY_OP, STR}, &StringOnStringOperation);
            }

            if (def & BiType::SystemFunctions || def & BiType::ObjectOperators)
            {
                opMap.add({LIST, ANY_OP, NUM}, &ListOnNumberOperation);
                opMap.add({LIST, ANY_OP, LIST}, &ListOnListOperation);
            }
        }
    };

}  // namespace builtin_operations

#endif // CPARSE_BUILTIN_OPERATIONS_H
