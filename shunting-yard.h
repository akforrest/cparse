#ifndef SHUNTING_YARD_H_
#define SHUNTING_YARD_H_
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

namespace cparse
{

    /*
     * About tokType enum:
     *
     * The 3 left most bits (0x80, 0x40 and 0x20) of the Token Type
     * are reserved for denoting Numerals, Iterators and References.
     * If you want to define your own type please mind this bits.
     */
    using tokType_t = uint8_t;
    using opID_t = uint64_t;
    enum tokType
    {
        // Internal types:
        NONE, OP, UNARY, VAR,

        // Base types:
        // Note: The mask system accepts at most 29 (32-3) different base types.
        STR, FUNC,

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

    struct TokenBase
    {
        tokType_t type{};

        virtual ~TokenBase() {}
        TokenBase() {}
        TokenBase(tokType_t type) : type(type) {}

        virtual TokenBase * clone() const = 0;
    };

    template<class T> class Token : public TokenBase
    {
        public:
            T val;
            Token(T t, tokType_t type) : TokenBase(type), val(t) {}
            TokenBase * clone() const override;
    };

    struct TokenNone : public TokenBase
    {
        TokenNone() : TokenBase(NONE) {}
        TokenBase * clone() const override
        {
            return new TokenNone(*this);
        }
    };

    struct TokenUnary : public TokenBase
    {
        TokenUnary() : TokenBase(UNARY) {}
        TokenBase * clone() const override
        {
            return new TokenUnary(*this);
        }
    };

    class PackToken;
    using TokenQueue_t = std::queue<TokenBase *>;
    class OppMap_t
    {
            // Set of operators that should be evaluated from right to left:
            std::set<QString> RtoL;
            // Map of operators precedence:
            std::map<QString, int> pr_map;

        public:
            OppMap_t();

            void add(const QString & op, int precedence);

            void addUnary(const QString & op, int precedence);

            void addRightUnary(const QString & op, int precedence);

            int prec(const QString & op) const;
            bool assoc(const QString & op) const;
            bool exists(const QString & op) const;
    };

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
    struct rpnBuilder
    {
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

        public:
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
    struct evaluationData
    {
        TokenQueue_t rpn;
        TokenMap scope;
        const opMap_t & opMap;

        std::unique_ptr<RefToken> left;
        std::unique_ptr<RefToken> right;

        QString op;
        opID_t opID{};

        evaluationData(TokenQueue_t rpn, const TokenMap & scope, const opMap_t & opMap);
    };

    // The reservedWordParser_t is the function type called when
    // a reserved word or character is found at parsing time.
    using rWordParser_t = void (const char *, const char **, rpnBuilder *);
    using rWordMap_t = std::map<QString, rWordParser_t *>;
    using rCharMap_t = std::map<char, rWordParser_t *>;

    struct parserMap_t
    {
        rWordMap_t wmap;
        rCharMap_t cmap;

        // Add reserved word:
        void add(const QString & word,  rWordParser_t * parser);

        // Add reserved character:
        void add(char c,  rWordParser_t * parser);

        rWordParser_t * find(const QString & text);

        rWordParser_t * find(char c);
    };

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

    struct opSignature_t
    {
        tokType_t left;
        QString op;
        tokType_t right;
        opSignature_t(tokType_t L, const QString & op, tokType_t R);
    };

    class Operation
    {
        public:
            using opFunc_t = PackToken(*)(const PackToken &, const PackToken &, evaluationData *);

        public:
            // Use this exception to reject an operation.
            // Without stoping the operation matching process.
            struct Reject : public std::exception {};

        public:
            static inline uint32_t mask(tokType_t type);
            static opID_t build_mask(tokType_t left, tokType_t right);

        private:
            opID_t _mask;
            opFunc_t _exec;

        public:
            Operation(const opSignature_t & sig, opFunc_t func);

        public:
            opID_t getMask() const;
            PackToken exec(const PackToken & left, const PackToken & right,
                           evaluationData * data) const;
    };

    using typeMap_t = std::map<tokType_t, TokenMap>;
    using opList_t = std::vector<Operation>;
    class opMap_t : public std::map<QString, opList_t>
    {
        public:
            void add(const opSignature_t & sig, Operation::opFunc_t func);

            QString str() const;
    };

    struct Config_t
    {
        parserMap_t parserMap;
        OppMap_t opPrecedence;
        opMap_t opMap;

        Config_t() {}
        Config_t(parserMap_t p, OppMap_t opp, opMap_t opMap);
    };

    class Calculator
    {
        public:
            static Config_t & Default();

            static typeMap_t & type_attribute_map();

            static PackToken calculate(const char * expr, const TokenMap & vars = &TokenMap::empty,
                                       const char * delim = 0, const char ** rest = nullptr);

            static TokenBase * calculate(const TokenQueue_t & RPN, const TokenMap & scope,
                                         const Config_t & config = Default());
            static TokenQueue_t toRPN(const char * expr, TokenMap vars,
                                      const char * delim = 0, const char ** rest = nullptr,
                                      Config_t config = Default());

            // Used to dealloc a TokenQueue_t safely.
            struct RAII_TokenQueue_t;

        protected:
            virtual const Config_t Config() const
            {
                return Default();
            }

        private:
            TokenQueue_t RPN;

        public:
            virtual ~Calculator();
            Calculator();
            Calculator(const Calculator & calc);
            Calculator(const char * expr, const TokenMap & vars = &TokenMap::empty,
                       const char * delim = 0, const char ** rest = nullptr,
                       const Config_t & config = Default());
            void compile(const char * expr, const TokenMap & vars = &TokenMap::empty,
                         const char * delim = 0, const char ** rest = nullptr);
            PackToken eval(const TokenMap & vars = &TokenMap::empty, bool keep_refs = false) const;

            // Serialization:
            QString str() const;
            static QString str(TokenQueue_t rpn);

            // Operators:
            Calculator & operator=(const Calculator & calc);
    };

    template<class T>
    TokenBase * Token<T>::clone() const
    {
        return new Token(*this);
    }

}  // namespace cparse

#endif  // SHUNTING_YARD_H_
