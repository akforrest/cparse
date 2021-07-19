
#ifndef SHUNTING_YARD_EXCEPTIONS_H_
#define SHUNTING_YARD_EXCEPTIONS_H_

#include "cparse.h"
#include "rpnbuilder.h"

#include <string>
#include <stdexcept>

namespace cparse
{
    [[maybe_unused]] static void log_undefined_operation(const QString & op, const PackToken & left, const PackToken & right)
    {
        qWarning(cparseLog) << "Unexpected operation with operator '" << op << "' and operands: " << left.str() << " and " << right.str();
    }
}  // namespace cparse

#endif  // SHUNTING_YARD_EXCEPTIONS_H_
