#include <cmath>

namespace builtin_operations {

// Assignment operator "="
PackToken Assign(const PackToken& , const PackToken& right, evaluationData* data) {
  PackToken& key = data->left->key;
  PackToken& origin = data->left->origin;

  // If the left operand has a name:
  if (key->type == STR) {
    auto var_name = key.asString();

    // If it is an attribute of a TokenMap:
    if (origin->type == MAP) {
      TokenMap& map = origin.asMap();
      map[var_name] = right;

    // If it is a local variable:
    } else {
      // Find the parent map where this variable is stored:
      TokenMap* map = data->scope.findMap(var_name);

      // Note:
      // It is not possible to assign directly to
      // the global scope. It would be easy for the user
      // to do it by accident, thus:
      if (!map || *map == TokenMap::default_global()) {
        data->scope[var_name] = right;
      } else {
        (*map)[var_name] = right;
      }
    }
  // If the left operand has an index number:
  } else if (key->type & NUM) {
    if (origin->type == LIST) {
      TokenList& list = origin.asList();
      size_t index = key.asInt();
      list[index] = right;
    } else {
      throw std::domain_error("Left operand of assignment is not a list!");
    }
  } else {
    throw undefined_operation(data->op, key, right);
  }
  return right;
}

PackToken Comma(const PackToken& left, const PackToken& right, evaluationData* ) {
  if (left->type == TUPLE) {
    left.asTuple().list().push_back(right);
    return left;
  } else {
    return Tuple(left, right);
  }
}

PackToken Colon(const PackToken& left, const PackToken& right, evaluationData* ) {
  if (left->type == STUPLE) {
    left.asSTuple().list().push_back(right);
    return left;
  } else {
    return STuple(left, right);
  }
}

PackToken Equal(const PackToken& left, const PackToken& right, evaluationData* data) {
  if (left->type == VAR || right->type == VAR) {
    throw undefined_operation(data->op, left, right);
  }

  return PackToken(left == right);
}

PackToken Different(const PackToken& left, const PackToken& right, evaluationData* data) {
  if (left->type == VAR || right->type == VAR) {
    throw undefined_operation(data->op, left, right);
  }

  return PackToken(left != right);
}

PackToken MapIndex(const PackToken& p_left, const PackToken& p_right, evaluationData* data) {
  TokenMap& left = p_left.asMap();
  auto right = p_right.asString();
  const auto & op = data->op;

  if (op == "[]" || op == ".") {
    PackToken* p_value = left.find(right);

    if (p_value) {
      return RefToken(right, *p_value, left);
    } else {
      return RefToken(right, PackToken::None(), left);
    }
  } else {
    throw undefined_operation(op, left, right);
  }
}

// Resolve build-in operations for non-map types, e.g.: 'str'.len()
PackToken TypeSpecificFunction(const PackToken& p_left, const PackToken& p_right, evaluationData* data) {
  if (p_left->type == MAP) throw Operation::Reject();

  TokenMap& attr_map = Calculator::type_attribute_map()[p_left->type];
  QString key = p_right.asString();

  PackToken* attr = attr_map.find(key);
  if (attr) {
    // Note: If attr is a function, it will receive have
    // scope["this"] == source, so it can make changes on this object.
    // Or just read some information for example: its length.
    return RefToken(key, (*attr), p_left);
  } else {
    throw undefined_operation(data->op, p_left, p_right);
  }
}

PackToken UnaryNumeralOperation(const PackToken& left, const PackToken& right, evaluationData* data) {
  const QString& op = data->op;

  if (op == "+") {
    return right;
  } else if (op == "-") {
    return -right.asDouble();
  } else {
    throw undefined_operation(data->op, left, right);
  }
}

PackToken NumeralOperation(const PackToken& left, const PackToken& right, evaluationData* data) {
  double left_d, right_d;
  int64_t left_i, right_i;

  // Extract integer and real values of the operators:
  left_d = left.asDouble();
  left_i = left.asInt();

  right_d = right.asDouble();
  right_i = right.asInt();

  const QString& op = data->op;

  if (op == "+") {
    return left_d + right_d;
  } else if (op == "*") {
    return left_d * right_d;
  } else if (op == "-") {
    return left_d - right_d;
  } else if (op == "/") {
    return left_d / right_d;
  } else if (op == "<<") {
    return left_i << right_i;
  } else if (op == "**") {
    return pow(left_d, right_d);
  } else if (op == ">>") {
    return left_i >> right_i;
  } else if (op == "%") {
    return left_i % right_i;
  } else if (op == "<") {
    return left_d < right_d;
  } else if (op == ">") {
    return left_d > right_d;
  } else if (op == "<=") {
    return left_d <= right_d;
  } else if (op == ">=") {
    return left_d >= right_d;
  } else if (op == "&&") {
    return left_i && right_i;
  } else if (op == "||") {
    return left_i || right_i;
  } else {
    throw undefined_operation(op, left, right);
  }
}

PackToken FormatOperation(const PackToken& p_left, const PackToken& p_right, evaluationData* ) {
  QString s_left = p_left.asString();
  const char* left = s_left.toStdString().c_str();

  Tuple right;

  if (p_right->type == TUPLE) {
    right = p_right.asTuple();
  } else {
    right = Tuple(p_right);
  }

  QString result;
  for (const PackToken& token : right.list()) {
    // Find the next occurrence of "%s"
    while (*left && (*left != '%' || left[1] != 's')) {
      if (*left == '\\' && left[1] == '%') ++left;
      result.push_back(*left);
      ++left;
    }

    if (*left == '\0') {
      throw type_error("Not all arguments converted during string formatting");
    } else {
      left += 2;
    }

    // Replace it by the token string representation:
    if (token->type == STR) {
      // Avoid using PackToken::str for strings
      // or it will enclose it quotes `"str"`
      result += token.asString();
    } else {
      result += token.str();
    }
  }

  // Find the next occurrence of "%s" if exists:
  while (*left && (*left != '%' || left[1] != 's')) {
    if (*left == '\\' && left[1] == '%') ++left;
    result.push_back(*left);
    ++left;
  }

  if (*left != '\0') {
    throw type_error("Not enough arguments for format string");
  } else {
    return result;
  }
}

PackToken StringOnStringOperation(const PackToken& p_left, const PackToken& p_right, evaluationData* data) {
  const QString& left = p_left.asString();
  const QString& right = p_right.asString();
  const QString& op = data->op;

  if (op == "+") {
    return left + right;
  } else if (op == "==") {
    return (left == right);
  } else if (op == "!=") {
    return (left != right);
  } else {
    throw undefined_operation(op, p_left, p_right);
  }
}

PackToken StringOnNumberOperation(const PackToken& p_left, const PackToken& p_right, evaluationData* data) {
  const QString& left = p_left.asString();
  const QString& op = data->op;

  std::stringstream ss;
  if (op == "+") {
    ss << left.toStdString() << p_right.asDouble();
    return QString::fromStdString(ss.str());
  } else if (op == "[]") {
    int64_t index = p_right.asInt();

    if (index < 0) {
      // Reverse index, i.e. list[-1] = list[list.size()-1]
      index += left.size();
    }
    if (index < 0 || static_cast<size_t>(index) >= left.size()) {
      throw std::domain_error("String index out of range!");
    }

    ss << QString(left.at(index)).toStdString();
    return QString::fromStdString(ss.str());
  } else {
    throw undefined_operation(op, p_left, p_right);
  }
}

PackToken NumberOnStringOperation(const PackToken& p_left, const PackToken& p_right, evaluationData* data) {
  double left = p_left.asDouble();
  const QString& right = p_right.asString();

  std::stringstream ss;
  if (data->op == "+") {
    ss << left << right;
    return QString::fromStdString(ss.str());
  } else {
    throw undefined_operation(data->op, p_left, p_right);
  }
}

PackToken ListOnNumberOperation(const PackToken& p_left, const PackToken& p_right, evaluationData* data) {
  TokenList left = p_left.asList();

  if (data->op == "[]") {
    int64_t index = p_right.asInt();

    if (index < 0) {
      // Reverse index, i.e. list[-1] = list[list.size()-1]
      index += left.list().size();
    }

    if (index < 0 || static_cast<size_t>(index) >= left.list().size()) {
      throw std::domain_error("List index out of range!");
    }

    PackToken& value = left.list()[index];

    return RefToken(index, value, p_left);
  } else {
    throw undefined_operation(data->op, p_left, p_right);
  }
}

PackToken ListOnListOperation(const PackToken& p_left, const PackToken& p_right, evaluationData* data) {
  TokenList& left = p_left.asList();
  TokenList& right = p_right.asList();

  if (data->op == "+") {
    // Deep copy the first list:
    TokenList result;
    result.list() = left.list();

    // Insert items from the right list into the left:
    for (PackToken& p : right.list()) {
      result.list().push_back(p);
    }

    return result;
  } else {
    throw undefined_operation(data->op, left, right);
  }
}

struct Startup {
  Startup() {
    // Create the operator precedence map based on C++ default
    // precedence order as described on cppreference website:
    // http://en.cppreference.com/w/cpp/language/operator_precedence
    OppMap_t& opp = Calculator::Default().opPrecedence;
    opp.add("[]", 2); opp.add("()", 2); opp.add(".", 2);
    opp.add("**", 3);
    opp.add("*",  5); opp.add("/", 5); opp.add("%", 5);
    opp.add("+",  6); opp.add("-", 6);
    opp.add("<<", 7); opp.add(">>", 7);
    opp.add("<",  8); opp.add("<=", 8); opp.add(">=", 8); opp.add(">", 8);
    opp.add("==", 9); opp.add("!=", 9);
    opp.add("&&", 13);
    opp.add("||", 14);
    opp.add("=", 15); opp.add(":", 15);
    opp.add(",", 16);

    // Add unary operators:
    opp.addUnary("+",  3); opp.addUnary("-", 3);

    // Link operations to respective operators:
    opMap_t& opMap = Calculator::Default().opMap;
    opMap.add({ANY_TYPE, "=", ANY_TYPE}, &Assign);
    opMap.add({ANY_TYPE, ",", ANY_TYPE}, &Comma);
    opMap.add({ANY_TYPE, ":", ANY_TYPE}, &Colon);
    opMap.add({ANY_TYPE, "==", ANY_TYPE}, &Equal);
    opMap.add({ANY_TYPE, "!=", ANY_TYPE}, &Different);
    opMap.add({MAP, "[]", STR}, &MapIndex);
    opMap.add({ANY_TYPE, ".", STR}, &TypeSpecificFunction);
    opMap.add({MAP, ".", STR}, &MapIndex);
    opMap.add({STR, "%", ANY_TYPE}, &FormatOperation);

    auto ANY_OP = "";

    // Note: The order is important:
    opMap.add({NUM, ANY_OP, NUM}, &NumeralOperation);
    opMap.add({UNARY, ANY_OP, NUM}, &UnaryNumeralOperation);
    opMap.add({STR, ANY_OP, STR}, &StringOnStringOperation);
    opMap.add({STR, ANY_OP, NUM}, &StringOnNumberOperation);
    opMap.add({NUM, ANY_OP, STR}, &NumberOnStringOperation);
    opMap.add({LIST, ANY_OP, NUM}, &ListOnNumberOperation);
    opMap.add({LIST, ANY_OP, LIST}, &ListOnListOperation);
  }
} __CPARSE_STARTUP;

}  // namespace builtin_operations
