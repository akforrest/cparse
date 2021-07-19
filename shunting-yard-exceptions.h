
#ifndef SHUNTING_YARD_EXCEPTIONS_H_
#define SHUNTING_YARD_EXCEPTIONS_H_

#include "./shunting-yard.h"

#include <string>
#include <stdexcept>

namespace cparse {

class msg_exception : public std::exception {
 protected:
  const QString msg;
 public:
  msg_exception(const QString& msg) : msg(msg) {}
  ~msg_exception() throw() {}
  const char* what() const throw() {
    return msg.toStdString().c_str();
  }
};

struct bad_cast : public msg_exception {
  bad_cast(const QString& msg) : msg_exception(msg) {}
};

struct syntax_error : public msg_exception {
  syntax_error(const QString& msg) : msg_exception(msg) {}
};

struct type_error : public msg_exception {
  type_error(const QString& msg) : msg_exception(msg) {}
};

struct undefined_operation : public msg_exception {
  undefined_operation(const QString& op, const TokenBase* left, const TokenBase* right)
                      : undefined_operation(op, PackToken(left->clone()), PackToken(right->clone())) {}
  undefined_operation(const QString& op, const PackToken& left, const PackToken& right)
    : msg_exception("Unexpected operation with operator '" + op + "' and operands: " + left.str() + " and " + right.str() + ".") {}
};

}  // namespace cparse

#endif  // SHUNTING_YARD_EXCEPTIONS_H_
