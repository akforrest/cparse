#ifndef CPARSE_CONTAINERS_H_
#define CPARSE_CONTAINERS_H_

#include <map>
#include <list>
#include <vector>
#include <string>
#include <memory>

#include <QString>

#include "tokenbase.h"
#include "packtoken.h"

namespace cparse
{
    template <typename T>
    class Container
    {
        protected:
            std::shared_ptr<T> ref;

        public:
            Container() : ref(std::make_shared<T>()) {}
            Container(const T & t) : ref(std::make_shared<T>(t)) {}

        public:
            operator T * () const
            {
                return ref.get();
            }
            friend bool operator==(Container<T> first, Container<T> second)
            {
                return first.ref == second.ref;
            }
    };

    class Iterator;

    class Iterable : public TokenBase
    {
        public:
            virtual ~Iterable() {}
            Iterable() {}
            Iterable(tokType_t type) : TokenBase(type) {}

            virtual Iterator * getIterator() const = 0;
    };

    // Iterator super class.
    class Iterator : public Iterable
    {
        public:
            Iterator() : Iterable(IT) {}
            virtual ~Iterator() {}
            // Return the next position of the iterator.
            // When it reaches the end it should return NULL
            // and reset the iterator automatically.
            virtual PackToken * next() = 0;
            virtual void reset() = 0;

            Iterator * getIterator() const;
    };

    struct TokenMap;
    typedef std::map<QString, PackToken> TokenMap_t;

    struct MapData_t
    {
        TokenMap_t map;
        TokenMap * parent = nullptr;
        MapData_t();
        MapData_t(TokenMap * p);
        MapData_t(const MapData_t & other);
        ~MapData_t();

        MapData_t & operator=(const MapData_t & other);
    };

    struct TokenMap : public Container<MapData_t>, public Iterable
    {
            // Static factories:
            static TokenMap empty;
            static TokenMap & base_map();
            static TokenMap & default_global();
            static PackToken default_constructor(TokenMap scope);

        public:
            // Attribute getters for the `MapData_t` content:
            TokenMap_t & map() const
            {
                return ref->map;
            }
            TokenMap * parent() const
            {
                return ref->parent;
            }

        public:
            // Implement the Iterable Interface:
            struct MapIterator : public Iterator
            {
                const TokenMap_t & map;
                TokenMap_t::const_iterator it = map.begin();
                PackToken last;

                MapIterator(const TokenMap_t & map) : map(map) {}

                PackToken * next();
                void reset();

                TokenBase * clone() const
                {
                    return new MapIterator(*this);
                }
            };

            Iterator * getIterator() const
            {
                return new MapIterator(map());
            }

        public:
            TokenMap(TokenMap * parent = &TokenMap::base_map())
                : Container(parent), Iterable(MAP)
            {
                // For the TokenBase super class
                this->type = MAP;
            }
            TokenMap(const TokenMap & other) : Container(other)
            {
                this->type = MAP;
            }

            virtual ~TokenMap() {}

        public:
            // Implement the TokenBase abstract class
            TokenBase * clone() const
            {
                return new TokenMap(*this);
            }

        public:
            PackToken * find(const QString & key);
            const PackToken * find(const QString & key) const;
            TokenMap * findMap(const QString & key);
            void assign(const QString & key, TokenBase * value);
            void insert(const QString & key, TokenBase * value);

            TokenMap getChild();

            PackToken & operator[](const QString & str);

            void erase(const QString & key);
    };

    // Build a TokenMap which is a child of default_global()
    struct GlobalScope : public TokenMap
    {
        GlobalScope() : TokenMap(&TokenMap::default_global()) {}
    };

    typedef std::vector<PackToken> TokenList_t;

    class TokenList : public Container<TokenList_t>, public Iterable
    {

        public:
            static PackToken default_constructor(TokenMap scope);
            // Attribute getter for the `TokenList_t` content:
            TokenList_t & list() const
            {
                return *ref;
            }

        public:
            struct ListIterator : public Iterator
            {
                TokenList_t * list;
                uint64_t i = 0;

                ListIterator(TokenList_t * list) : list(list) {}

                PackToken * next();
                void reset();

                TokenBase * clone() const
                {
                    return new ListIterator(*this);
                }
            };

            Iterator * getIterator() const
            {
                return new ListIterator(&list());
            }

        public:
            TokenList()
            {
                this->type = LIST;
            }
            virtual ~TokenList() {}

            PackToken & operator[](const uint64_t idx) const
            {
                if (list().size() <= idx)
                {
                    throw std::out_of_range("List index out of range!");
                }

                return list()[idx];
            }

            void push(PackToken val) const
            {
                list().push_back(val);
            }
            PackToken pop() const
            {
                auto back = list().back();
                list().pop_back();
                return back;
            }

        public:
            // Implement the TokenBase abstract class
            TokenBase * clone() const
            {
                return new TokenList(*this);
            }
    };

    class Tuple : public TokenList
    {
        public:
            Tuple()
            {
                this->type = TUPLE;
            }
            Tuple(const TokenBase * first)
            {
                this->type = TUPLE;
                list().push_back(PackToken(first->clone()));
            }
            Tuple(const PackToken first) : Tuple(first.token()) {}

            Tuple(const TokenBase * first, const TokenBase * second)
            {
                this->type = TUPLE;
                list().push_back(PackToken(first->clone()));
                list().push_back(PackToken(second->clone()));
            }
            Tuple(const PackToken first, const PackToken second)
                : Tuple(first.token(), second.token()) {}

        public:
            // Implement the TokenBase abstract class
            TokenBase * clone() const
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
                this->type = STUPLE;
            }
            STuple(const TokenBase * first)
            {
                this->type = STUPLE;
                list().push_back(PackToken(first->clone()));
            }
            STuple(const PackToken first) : STuple(first.token()) {}

            STuple(const TokenBase * first, const TokenBase * second)
            {
                this->type = STUPLE;
                list().push_back(PackToken(first->clone()));
                list().push_back(PackToken(second->clone()));
            }
            STuple(const PackToken first, const PackToken second)
                : STuple(first.token(), second.token()) {}

        public:
            // Implement the TokenBase abstract class
            TokenBase * clone() const
            {
                return new STuple(*this);
            }
    };

}  // namespace cparse

#endif  // CPARSE_CONTAINERS_H_
