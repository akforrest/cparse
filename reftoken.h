#ifndef CPARSE_REFTOKEN_H
#define CPARSE_REFTOKEN_H

#include "token.h"
#include "packtoken.h"

namespace cparse
{
    // The RefToken keeps information about the context in which a variable was
    // originally evaluated and allow a final value to be correctly resolved afterwards.
    class RefToken : public Token
    {
        public:

            RefToken(PackToken k, Token * v, PackToken m = PackToken::None());
            RefToken(PackToken k = PackToken::None(), PackToken v = PackToken::None(), PackToken m = PackToken::None());

            Token * resolve(TokenMap * localScope = nullptr) const;
            Token * clone() const override;

            PackToken m_key;
            PackToken m_origin;

        private:

            PackToken m_originalValue;
    };

}

#endif // CPARSE_REFTOKEN_H
