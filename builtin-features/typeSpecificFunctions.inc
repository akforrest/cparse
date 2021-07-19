
namespace builtin_typeSpecificFunctions {

/* * * * * MAP Type built-in functions * * * * */

const args_t map_pop_args = {"key", "default"};
PackToken map_pop(TokenMap scope) {
  TokenMap map = scope["this"].asMap();
  QString key = scope["key"].asString();

  // Check if the item is available and remove it:
  if (map.map().count(key)) {
    PackToken value = map[key];
    map.erase(key);
    return value;
  }

  // If not available return the default value or None
  PackToken* def = scope.find("default");
  if (def) {
    return *def;
  } else {
    return PackToken::None();
  }
}

PackToken map_len(TokenMap scope) {
  TokenMap map = scope.find("this")->asMap();
  return map.map().size();
}

PackToken default_instanceof(TokenMap scope) {
  TokenMap _super = scope["value"].asMap();
  TokenMap* _this = scope["this"].asMap().parent();

  TokenMap* parent = _this;
  while (parent) {
    if ((*parent) == _super) {
      return true;
    }

    parent = parent->parent();
  }

  return false;
}

/* * * * * LIST Type built-in functions * * * * */

const args_t push_args = {"item"};
PackToken list_push(TokenMap scope) {
  PackToken* list = scope.find("this");
  PackToken* token = scope.find("item");

  // If "this" is not a list it will throw here:
  list->asList().list().push_back(*token);

  return *list;
}

const args_t list_pop_args = {"pos"};
PackToken list_pop(TokenMap scope) {
  TokenList list = scope.find("this")->asList();
  PackToken* token = scope.find("pos");

  int64_t pos;

  if ((*token)->type & NUM) {
    pos = token->asInt();

    // So that pop(-1) is the same as pop(last_idx):
    if (pos < 0) pos = list.list().size()-pos;
  } else {
    pos = list.list().size()-1;
  }

  PackToken result = list.list()[pos];

  // Erase the item from the list:
  // Note that this operation is optimal if pos == list.size()-1
  list.list().erase(list.list().begin() + pos);

  return result;
}

PackToken list_len(TokenMap scope) {
  TokenList list = scope.find("this")->asList();
  return list.list().size();
}

PackToken list_join(TokenMap scope) {
  TokenList list = scope["this"].asList();
  QString chars = scope["chars"].asString();
  std::stringstream result;

  std::vector<PackToken>::const_iterator it = list.list().begin();
  result << it->asString();
  for (++it; it != list.list().end(); ++it) {
    result << chars << it->asString();
  }

  return QString::fromStdString(result.str());
}

/* * * * * STR Type built-in functions * * * * */

PackToken string_len(TokenMap scope) {
  QString str = scope["this"].asString();
  return str.length();
}

PackToken string_lower(TokenMap scope) {
  QString str = scope["this"].asString();
  return str.toLower();
}

PackToken string_upper(TokenMap scope) {
  QString str = scope["this"].asString();
  return str.toUpper();
}

PackToken string_strip(TokenMap scope) {
  QString str = scope["this"].asString();
  return str.trimmed();
}

PackToken string_split(TokenMap scope) {
  TokenList list;
  QString str = scope["this"].asString();
  QString split_chars = scope["chars"].asString();

  auto split = str.split(split_chars);

  for (auto ss : split) {
      list.push(ss);
  }

  return list;
}

/* * * * * Type-Specific Functions Startup: * * * * */

struct Startup {
  Startup() {
    TokenMap& base_list = Calculator::type_attribute_map()[LIST];
    base_list["push"] = CppFunction(list_push, push_args, "push");
    base_list["pop"] = CppFunction(list_pop, list_pop_args, "pop");
    base_list["len"] = CppFunction(list_len, "len");
    base_list["join"] = CppFunction(list_join, {"chars"}, "join");

    TokenMap& base_str = Calculator::type_attribute_map()[STR];
    base_str["len"] = CppFunction(&string_len, "len");
    base_str["lower"] = CppFunction(&string_lower, "lower");
    base_str["upper"] = CppFunction(&string_upper, "upper");
    base_str["strip"] = CppFunction(&string_strip, "strip");
    base_str["split"] = CppFunction(&string_split, {"chars"}, "split");

    TokenMap& base_map = TokenMap::base_map();
    base_map["pop"] = CppFunction(map_pop, map_pop_args, "pop");
    base_map["len"] = CppFunction(map_len, "len");
    base_map["instanceof"] = CppFunction(&default_instanceof,
                                         {"value"}, "instanceof");
  }
} __CPARSE_STARTUP;

}  // namespace builtin_typeSpecificFunctions
