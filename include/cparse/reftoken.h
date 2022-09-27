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

            RefToken(PackToken key, Token * value, PackToken origin = PackToken::None());
            RefToken(PackToken key = PackToken::None(), PackToken value = PackToken::None(), PackToken origin = PackToken::None());

            Token * resolve(const TokenMap * localScope, const TokenMap * configScope) const;
            Token * clone() const override;

            const PackToken m_key;
            const PackToken m_origin;

        private:

            const PackToken m_originalValue;
    };

}

#endif // CPARSE_REFTOKEN_H
