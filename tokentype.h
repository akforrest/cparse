#ifndef CPARSE_CPARSETYPES_H
#define CPARSE_CPARSETYPES_H

namespace cparse
{
    /*
     * About TokenType enum:
     *
     * The 3 left most bits (0x80, 0x40 and 0x20) of the Token Type
     * are reserved for denoting Numerals, Iterators and References.
     * If you want to define your own type please mind this bits.
     */
    enum TokenType
    {
        // Internal types:
        NONE, OP, UNARY, VAR, ERROR, REJECT,

        // Base types:
        // Note: The mask system accepts at most 26 (32-6) different base types.
        STR, FUNC,

        USER     = 0x0A,
        USER_END = 0x1F,

        // Numerals:
        NUM = 0x20,   // Everything with the bit 0x20 set is a number.
        REAL = 0x21,  // == 0x20 + 0x1 => Real numbers.
        INT = 0x22,   // == 0x20 + 0x2 => Integral numbers.
        BOOL = 0x23,  // == 0x20 + 0x3 => Boolean Type.

        // Complex types:
        IT = 0x40,      // Everything with the bit 0x40 set are iterators.
        LIST = 0x41,    // == 0x40 + 0x01 => Lists are iterators.
        TUPLE = 0x42,   // == 0x40 + 0x02 => Tuples are iterators.
        STUPLE = 0x43,  // == 0x40 + 0x03 => ArgTuples are iterators.
        MAP = 0x44,     // == 0x40 + 0x04 => Maps are Iterators

        // References are internal tokens used by the calculator:
        REF = 0x80,

        // Mask used when defining operations:
        ANY_TYPE = 0xFF
    };
}

#endif // CPARSE_CPARSETYPES_H
