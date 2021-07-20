#ifndef CPARSE_CONTAINERS_H_
#define CPARSE_CONTAINERS_H_

#include <map>
#include <list>
#include <vector>
#include <string>
#include <memory>

#include <QString>

#include "token.h"
#include "packtoken.h"

namespace cparse
{
    class TokenIterator;

    class IterableToken : public Token
    {
        public:
            IterableToken() {}
            IterableToken(TokenType type) : Token(type) {}
            ~IterableToken() override {}

            virtual TokenIterator * getIterator() const = 0;
    };

    // Iterator super class.
    class TokenIterator : public IterableToken
    {
        public:
            TokenIterator() : IterableToken(IT) {}
            ~TokenIterator() override {}

            // Return the next position of the iterator.
            // When it reaches the end it should return NULL
            // and reset the iterator automatically.
            virtual PackToken * next() = 0;
            virtual void reset() = 0;

            TokenIterator * getIterator() const override;
    };

    class TokenMap;
    struct TokenMapData
    {
        using MapType = std::map<QString, PackToken>;

        TokenMapData();
        TokenMapData(TokenMap * p);
        TokenMapData(const TokenMapData & other);
        ~TokenMapData();

        TokenMapData & operator=(const TokenMapData & other);

        MapType m_map;
        std::unique_ptr<TokenMap> m_tokenMap;
    };

    class TokenMap : public IterableToken
    {
        public:

            using MapType = TokenMapData::MapType;

            // Static factories:
            static TokenMap empty;
            static TokenMap & base_map();
            static TokenMap & default_global();

            TokenMap(TokenMap * parent = &TokenMap::base_map());
            TokenMap(const TokenMap & other);
            ~TokenMap() override {}

            bool operator==(const TokenMap & other) const;

            // Attribute getters for the `MapData_t` content:
            MapType & map() const;
            TokenMap * parent() const;

            // Implement the Iterable Interface:
            struct MapIterator : public TokenIterator
            {
                const TokenMapData::MapType & map;
                TokenMapData::MapType::const_iterator it = map.begin();
                PackToken last;

                MapIterator(const TokenMapData::MapType & map) : map(map) {}

                PackToken * next() override;
                void reset() override;

                Token * clone() const override
                {
                    return new MapIterator(*this);
                }
            };

            TokenIterator * getIterator() const override;

            Token * clone() const override;

            PackToken * find(const QString & key);
            const PackToken * find(const QString & key) const;
            TokenMap * findMap(const QString & key);
            const TokenMap * findMap(const QString & key) const;

            void assign(const QString & key, Token * value);
            void insert(const QString & key, Token * value);

            TokenMap getChild();

            PackToken & operator[](const QString & str);
            const PackToken & operator[](const QString & str) const;

            void erase(const QString & key);

        private:

            std::shared_ptr<TokenMapData> m_ref;
    };

    inline TokenMap::TokenMap(TokenMap * parent)
        : IterableToken(MAP),
          m_ref(std::make_shared<TokenMapData>(parent))
    {
        // For the Token super class
        this->m_type = MAP;
    }

    inline TokenMap::TokenMap(const TokenMap & other)
        : IterableToken(other),
          m_ref(other.m_ref)
    {
        this->m_type = MAP;
    }

    inline TokenMapData::MapType & TokenMap::map() const
    {
        return m_ref->m_map;
    }

    inline TokenMap * TokenMap::parent() const
    {
        return m_ref->m_tokenMap.get();
    }

    inline TokenIterator * TokenMap::getIterator() const
    {
        return new MapIterator(map());
    }

    inline Token * TokenMap::clone() const
    {
        return new TokenMap(*this);
    }

    class TokenList : public IterableToken
    {
        public:

            using ListType = std::vector<PackToken>;

            TokenList()
                : m_ref(std::make_shared<std::vector<PackToken>>())
            {
                this->m_type = LIST;
            }
            ~TokenList() override {}

            // Attribute getter for the `TokenList_t` content:
            ListType & list() const
            {
                return *m_ref;
            }

            struct ListIterator : public TokenIterator
            {
                ListType * list;
                quint64 i = 0;

                ListIterator(ListType * list) : list(list) {}

                PackToken * next() override;
                void reset() override;

                Token * clone() const override
                {
                    return new ListIterator(*this);
                }
            };

            TokenIterator * getIterator() const override
            {
                return new ListIterator(&list());
            }

            PackToken & operator[](quint64 idx) const;

            void push(const PackToken & val) const
            {
                list().push_back(val);
            }
            PackToken pop() const
            {
                auto back = list().back();
                list().pop_back();
                return back;
            }

            // Implement the Token abstract class
            Token * clone() const override
            {
                return new TokenList(*this);
            }

        private:

            std::shared_ptr<std::vector<PackToken> > m_ref;
    };

    class Tuple : public TokenList
    {
        public:
            Tuple()
            {
                this->m_type = TUPLE;
            }
            Tuple(const Token * first)
            {
                this->m_type = TUPLE;
                list().push_back(PackToken(first->clone()));
            }
            Tuple(const PackToken & first) : Tuple(first.token()) {}

            Tuple(const Token * first, const Token * second)
            {
                this->m_type = TUPLE;
                list().push_back(PackToken(first->clone()));
                list().push_back(PackToken(second->clone()));
            }
            Tuple(const PackToken & first, const PackToken & second)
                : Tuple(first.token(), second.token()) {}

            // Implement the Token abstract class
            Token * clone() const override
            {
                return new Tuple(*this);
            }
    };

    // This Special Tuple is to be used only as syntactic sugar, and
    // constructed only with the operator `:`, i.e.:
    // - passing key-word arguments: func(1, 2, optional_arg:10)
    // - slicing lists or strings: my_list[2:10:2] (not implemented)
    //
    // STuple means one of:
    // - Special Tuple, Syntactic Tuple or System Tuple
    //
    // I haven't decided yet. Suggestions accepted.
    class STuple : public Tuple
    {
        public:
            STuple()
            {
                this->m_type = STUPLE;
            }
            STuple(const Token * first)
            {
                this->m_type = STUPLE;
                list().push_back(PackToken(first->clone()));
            }
            STuple(const PackToken & first) : STuple(first.token()) {}

            STuple(const Token * first, const Token * second)
            {
                this->m_type = STUPLE;
                list().push_back(PackToken(first->clone()));
                list().push_back(PackToken(second->clone()));
            }
            STuple(const PackToken & first, const PackToken & second)
                : STuple(first.token(), second.token()) {}

            // Implement the Token abstract class
            Token * clone() const override
            {
                return new STuple(*this);
            }
    };

}  // namespace cparse

#endif  // CPARSE_CONTAINERS_H_
