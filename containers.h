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
    template <typename T>
    class Container
    {
        public:

            Container(): m_ref(std::make_shared<T>()) {}
            Container(const T & t): m_ref(std::make_shared<T>(t)) {}

        public:

            operator T * () const
            {
                return m_ref.get();
            }

            friend bool operator==(Container<T> first, Container<T> second)
            {
                return first.m_ref == second.m_ref;
            }

        protected:

            std::shared_ptr<T> m_ref;
    };

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

    struct TokenMap;
    struct TokenMapData
    {
        using MapType = std::map<QString, PackToken>;
        MapType map;
        TokenMap * parent = nullptr;
        TokenMapData();
        TokenMapData(TokenMap * p);
        TokenMapData(const TokenMapData & other);
        ~TokenMapData();

        TokenMapData & operator=(const TokenMapData & other);
    };

    struct TokenMap : public Container<TokenMapData>, public IterableToken
    {
            // Static factories:
            static TokenMap empty;
            static TokenMap & base_map();
            static TokenMap & default_global();
            static PackToken default_constructor(TokenMap scope);

        public:

            TokenMap(TokenMap * parent = &TokenMap::base_map())
                : Container(parent), IterableToken(MAP)
            {
                // For the Token super class
                this->m_type = MAP;
            }

            TokenMap(const TokenMap & other) : Container(other), IterableToken(other)
            {
                this->m_type = MAP;
            }

            ~TokenMap() override {}

            // Attribute getters for the `MapData_t` content:
            TokenMapData::MapType & map() const
            {
                return m_ref->map;
            }
            TokenMap * parent() const
            {
                return m_ref->parent;
            }

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

            TokenIterator * getIterator() const override
            {
                return new MapIterator(map());
            }

            // Implement the Token abstract class
            Token * clone() const override
            {
                return new TokenMap(*this);
            }

            PackToken * find(const QString & key);
            const PackToken * find(const QString & key) const;
            TokenMap * findMap(const QString & key);
            void assign(const QString & key, Token * value);
            void insert(const QString & key, Token * value);

            TokenMap getChild();

            PackToken & operator[](const QString & str);

            void erase(const QString & key);
    };

    // Build a TokenMap which is a child of default_global()
    struct GlobalScope : public TokenMap
    {
        GlobalScope() : TokenMap(&TokenMap::default_global()) {}
    };

    class TokenList : public Container<std::vector<PackToken>>, public IterableToken
    {

        public:

            using ListType = std::vector<PackToken>;

            TokenList()
            {
                this->m_type = LIST;
            }
            ~TokenList() override {}

            static PackToken default_constructor(TokenMap scope);
            // Attribute getter for the `TokenList_t` content:
            ListType & list() const
            {
                return *m_ref;
            }

            struct ListIterator : public TokenIterator
            {
                ListType * list;
                uint64_t i = 0;

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

            PackToken & operator[](const uint64_t idx) const
            {
                if (list().size() <= idx)
                {
                    throw std::out_of_range("List index out of range!");
                }

                return list()[idx];
            }

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
