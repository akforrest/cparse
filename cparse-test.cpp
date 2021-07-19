#include <iostream>
#include <memory>
#include <string>

#include <QtTest>
#include <utility>

#include "cparse-test.h"
#include "rpnbuilder.h"
#include "calculator.h"
#include "reftoken.h"

using namespace cparse;

TokenMap vars, emap, tmap, key3;

#define REQUIRE QVERIFY

void PREPARE_ENVIRONMENT()
{
    vars["pi"] = 3.14;
    vars["b1"] = 0.0;
    vars["b2"] = 0.86;
    vars["_b"] = 0;
    vars["str1"] = "foo";
    vars["str2"] = "bar";
    vars["str3"] = "foobar";
    vars["str4"] = "foo10";
    vars["str5"] = "10bar";

    vars["map"] = tmap;
    tmap["key"] = "mapped value";
    tmap["key1"] = "second mapped value";
    tmap["key2"] = 10;
    tmap["key3"] = key3;
    tmap["key3"]["map1"] = "inception1";
    tmap["key3"]["map2"] = "inception2";

    emap["a"] = 10;
    emap["b"] = 20;
}
class Approx
{
    public:
        explicit Approx(double value)
            :   m_epsilon(std::numeric_limits<float>::epsilon() * 100),
                m_scale(1.0),
                m_value(value)
        {}

        Approx(Approx const & other)
            :   m_epsilon(other.m_epsilon),
                m_scale(other.m_scale),
                m_value(other.m_value)
        {}

        static Approx custom()
        {
            return Approx(0);
        }

        Approx operator()(double value)
        {
            Approx approx(value);
            approx.epsilon(m_epsilon);
            approx.scale(m_scale);
            return approx;
        }

        friend bool operator == (double lhs, Approx const & rhs)
        {
            // Thanks to Richard Harris for his help refining this formula
            return fabs(lhs - rhs.m_value) < rhs.m_epsilon * (rhs.m_scale + (std::max)(fabs(lhs), fabs(rhs.m_value)));
        }

        friend bool operator == (Approx const & lhs, double rhs)
        {
            return operator==(rhs, lhs);
        }

        friend bool operator != (double lhs, Approx const & rhs)
        {
            return !operator==(lhs, rhs);
        }

        friend bool operator != (Approx const & lhs, double rhs)
        {
            return !operator==(rhs, lhs);
        }

        Approx & epsilon(double newEpsilon)
        {
            m_epsilon = newEpsilon;
            return *this;
        }

        Approx & scale(double newScale)
        {
            m_scale = newScale;
            return *this;
        }

        std::string toString() const
        {
            std::ostringstream oss;
            oss << "Approx( " << QString::number(m_value).toStdString() << " )";
            return oss.str();
        }

    private:
        double m_epsilon;
        double m_scale;
        double m_value;
};

#define REQUIRE_THROWS( expr ) \
    do { \
        bool threw = false; \
        try { \
            expr; \
        } \
        catch( ... ) { \
            threw = true; \
        } \
        QVERIFY(threw); \
    } while( false )

#define REQUIRE_FALSE( expr ) QVERIFY(!(expr))
#define REQUIRE_NOTHROW( expr )  \
    do { \
        bool threw = false; \
        try { \
            expr; \
        } \
        catch( ... ) { \
            threw = true; \
        } \
        QVERIFY(!threw); \
    } while( false )

//TEST_CASE("Static calculate::calculate()", "[calculate]")
void static_calculate_calculate()
{
    REQUIRE(Calculator::calculate("-pi + 1", vars).asReal() == Approx(-2.14));
    REQUIRE(Calculator::calculate("-pi + 1 * b1", vars).asReal() == Approx(-3.14));
    REQUIRE(Calculator::calculate("(20+10)*3/2-3", vars).asReal() == Approx(42.0));
    REQUIRE(Calculator::calculate("1 << 4", vars).asReal() == Approx(16.0));
    REQUIRE(Calculator::calculate("1+(-2*3)", vars).asReal() == Approx(-5));
    REQUIRE(Calculator::calculate("1+_b+(-2*3)", vars).asReal() == Approx(-5));
    REQUIRE(Calculator::calculate("4 * -3", vars).asInt() == -12);

    REQUIRE_THROWS(Calculator("5x"));
    REQUIRE_THROWS(Calculator("v1 v2"));
}

//TEST_CASE("calculate::compile() and calculate::eval()", "[compile]")
void calculate_compile()
{
    Calculator c1;
    c1.compile("-pi+1", vars);
    REQUIRE(c1.eval().asReal() == Approx(-2.14));

    Calculator c2("pi+4", vars);
    REQUIRE(c2.eval().asReal() == Approx(7.14));
    REQUIRE(c2.eval().asReal() == Approx(7.14));

    Calculator c3("pi+b1+b2", vars);
    REQUIRE(c3.eval(vars).asReal() == Approx(4.0));
}

//TEST_CASE("Numerical expressions")
void numerical_expressions()
{
    REQUIRE(Calculator::calculate("123").asInt() == 123);
    REQUIRE(Calculator::calculate("0x1f").asInt() == 31);
    REQUIRE(Calculator::calculate("010").asInt() == 8);
    REQUIRE(Calculator::calculate("0").asInt() == 0);
    REQUIRE(Calculator::calculate("-0").asInt() == 0);

    REQUIRE(Calculator::calculate("0.5").asReal() == Approx(0.5));
    REQUIRE(Calculator::calculate("1.5").asReal() == Approx(1.50));
    REQUIRE(Calculator::calculate("2e2").asReal() == Approx(200));
    REQUIRE(Calculator::calculate("2E2").asReal() == Approx(200));

    REQUIRE(Calculator::calculate("2.5e2").asReal() == Approx(250));
    REQUIRE(Calculator::calculate("2.5E2").asReal() == Approx(250));

    REQUIRE_THROWS(Calculator::calculate("0x22.5"));
}

//TEST_CASE("Boolean expressions")
void boolean_expressions()
{
    REQUIRE_FALSE(Calculator::calculate("3 < 3").asBool());
    REQUIRE(Calculator::calculate("3 <= 3").asBool());
    REQUIRE_FALSE(Calculator::calculate("3 > 3").asBool());
    REQUIRE(Calculator::calculate("3 >= 3").asBool());
    REQUIRE(Calculator::calculate("3 == 3").asBool());
    REQUIRE_FALSE(Calculator::calculate("3 != 3").asBool());

    REQUIRE(Calculator::calculate("(3 && True) == True").asBool());
    REQUIRE_FALSE(Calculator::calculate("(3 && 0) == True").asBool());
    REQUIRE(Calculator::calculate("(3 || 0) == True").asBool());
    REQUIRE_FALSE(Calculator::calculate("(False || 0) == True").asBool());

    REQUIRE_FALSE(Calculator::calculate("10 == None").asBool());
    REQUIRE(Calculator::calculate("10 != None").asBool());
    REQUIRE_FALSE(Calculator::calculate("10 == 'str'").asBool());
    REQUIRE(Calculator::calculate("10 != 'str'").asBool());

    REQUIRE(Calculator::calculate("True")->m_type == BOOL);
    REQUIRE(Calculator::calculate("False")->m_type == BOOL);
    REQUIRE(Calculator::calculate("10 == 'str'")->m_type == BOOL);
    REQUIRE(Calculator::calculate("10 == 10")->m_type == BOOL);
}

//TEST_CASE("String expressions")
void string_expressions()
{
    REQUIRE(Calculator::calculate("str1 + str2 == str3", vars).asBool());
    REQUIRE_FALSE(Calculator::calculate("str1 + str2 != str3", vars).asBool());
    REQUIRE(Calculator::calculate("str1 + 10 == str4", vars).asBool());
    REQUIRE(Calculator::calculate("10 + str2 == str5", vars).asBool());

    REQUIRE(Calculator::calculate("'foo' + \"bar\" == str3", vars).asBool());
    REQUIRE(Calculator::calculate("'foo' + \"bar\" != 'foobar\"'", vars).asBool());

    // Test escaping characters:
    REQUIRE(Calculator::calculate("'foo\\'bar'").asString() == "foo'bar");
    REQUIRE(Calculator::calculate("\"foo\\\"bar\"").asString() == "foo\"bar");

    // Special meaning escaped characters:
    REQUIRE(Calculator::calculate("'foo\\bar'").asString() == "foo\\bar");
    REQUIRE(Calculator::calculate("'foo\\nar'").asString() == "foo\nar");
    REQUIRE(Calculator::calculate("'foo\\tar'").asString() == "foo\tar");
    REQUIRE_NOTHROW(Calculator::calculate("'foo\\t'"));
    REQUIRE(Calculator::calculate("'foo\\t'").asString() == "foo\t");

    // Scaping linefeed:
    REQUIRE_THROWS(Calculator::calculate("'foo\nar'"));
    REQUIRE(Calculator::calculate("'foo\\\nar'").asString() == "foo\nar");
}

//TEST_CASE("Testing operator parsing mechanism", "[operator]")
void operator_parsing()
{
    Calculator c1;

    // Test if it is detecting operations correctly:
    REQUIRE_NOTHROW(c1.compile("['list'] == ['list']"));
    REQUIRE(c1.eval() == true);

    REQUIRE_NOTHROW(c1.compile("['list']== ['list']"));
    REQUIRE(c1.eval() == true);

    REQUIRE_NOTHROW(c1.compile("['list'] ==['list']"));
    REQUIRE(c1.eval() == true);

    REQUIRE_NOTHROW(c1.compile("['list']==['list']"));
    REQUIRE(c1.eval() == true);

    REQUIRE_NOTHROW(c1.compile("{a:'list'} == {a:'list'}"));
    REQUIRE(c1.eval() == true);

    REQUIRE_NOTHROW(c1.compile("{a:'list'}== {a:'list'}"));
    REQUIRE(c1.eval() == true);

    REQUIRE_NOTHROW(c1.compile("{a:'list'} =={a:'list'}"));
    REQUIRE(c1.eval() == true);

    REQUIRE_NOTHROW(c1.compile("{a:'list'}=={a:'list'}"));
    REQUIRE(c1.eval() == true);
}

struct Test;
struct TestData_t
{
    Test * t = nullptr;
    TestData_t() {}
    TestData_t(const Test & t);
    ~TestData_t();
};

struct Test : public Container<TestData_t>
{
    Test() {}
    void set(Test t)
    {
        m_ref->t = new Test(std::move(t));
    }
    Test * get()
    {
        return m_ref->t;
    }

    std::weak_ptr<TestData_t> wkref()
    {
        return m_ref;
    }
    void reset()
    {
        m_ref.reset();
    }
};

TestData_t::TestData_t(const Test & t) : t(new Test(t)) {}
TestData_t::~TestData_t()
{
    delete t;
}

//TEST_CASE("Reference counting system", "[rc]")
void reference_counting()
{
    //SECTION("Testing constructors:")
    {
        Test t1;
        Test t2;
        t2.set(t1);

        REQUIRE(t1.get() == nullptr);
        REQUIRE(*(t2.get()) == t1);
    }
    // t1 and t2 should have been deleted by now.
    // If no exceptions were thrown it is working.

    //SECTION("Testing cycles")
    {
        std::weak_ptr<TestData_t> r1, r2, r3, r4;
        {
            Test t1;
            Test t2;
            t2.set(t1);

            // Build a cycle:
            REQUIRE_NOTHROW(t1.set(t2));

            // Add some non cyclic references:
            Test t3;
            Test t4;
            t4.set(t2);

            // Save some weak refs for later tests:
            r1 = t1.wkref();
            r2 = t2.wkref();
            r3 = t3.wkref();
            r4 = t4.wkref();
        }

        REQUIRE(r1.expired() == false);
        REQUIRE(r2.expired() == false);
        REQUIRE(r3.expired() == true);
        REQUIRE(r4.expired() == true);
        REQUIRE_NOTHROW(r1.lock()->t->reset());
        REQUIRE(r1.expired() == true);
        REQUIRE(r2.expired() == true);
    }
    // t1, t2, t3 and t4 should have been deleted by now.
    // If no exceptions were thrown it is working.

    // Note:
    // There should be no memory leaks and no "still reachable"
    // blocks when testing with valgrind.
}

//TEST_CASE("String operations")
void string_operations()
{
    // String formatting:
    REQUIRE(Calculator::calculate("'the test %s working' % 'is'").asString() == "the test is working");
    REQUIRE(Calculator::calculate("'the tests %s %s' % ('are', 'working')").asString() == "the tests are working");

    REQUIRE(Calculator::calculate("'works %s% %s' % (100, 'now')").asString() == "works 100% now");

    REQUIRE(Calculator::calculate("'escape \\%s works %s' % ('now')").asString() == "escape %s works now");

    REQUIRE_THROWS(Calculator::calculate("'the tests %s' % ('are', 'working')"));
    REQUIRE_THROWS(Calculator::calculate("'the tests %s %s' % ('are')"));

    // String indexing:
    REQUIRE(Calculator::calculate("'foobar'[0]").asString() == "f");
    REQUIRE(Calculator::calculate("'foobar'[3]").asString() == "b");
    REQUIRE(Calculator::calculate("'foobar'[-1]").asString() == "r");
    REQUIRE(Calculator::calculate("'foobar'[-3]").asString() == "b");
}

//TEST_CASE("Map access expressions", "[map][map-access]")
void map_expressions()
{
    REQUIRE(Calculator::calculate("map[\"key\"]", vars).asString() == "mapped value");
    REQUIRE(Calculator::calculate("map[\"key\"+1]", vars).asString() ==
            "second mapped value");
    REQUIRE(Calculator::calculate("map[\"key\"+2] + 3 == 13", vars).asBool() == true);
    REQUIRE(Calculator::calculate("map.key1", vars).asString() == "second mapped value");

    REQUIRE(Calculator::calculate("map.key3.map1", vars).asString() == "inception1");
    REQUIRE(Calculator::calculate("map.key3['map2']", vars).asString() == "inception2");
    REQUIRE(Calculator::calculate("map[\"no_key\"]", vars) == PackToken::None());
}

//TEST_CASE("Prototypical inheritance tests")
void prototypical_inheritance()
{
    TokenMap vars;
    TokenMap parent;
    TokenMap child(&parent);
    TokenMap grand_child(&child);

    vars["a"] = 0;
    vars["parent"] = parent;
    vars["child"] = child;
    vars["grand_child"] = grand_child;

    parent["a"] = 10;
    parent["b"] = 20;
    parent["c"] = 30;
    child["b"] = 21;
    child["c"] = 31;
    grand_child["c"] = 32;

    REQUIRE(Calculator::calculate("grand_child.a - 10", vars).asReal() == 0);
    REQUIRE(Calculator::calculate("grand_child.b - 20", vars).asReal() == 1);
    REQUIRE(Calculator::calculate("grand_child.c - 30", vars).asReal() == 2);

    REQUIRE_NOTHROW(Calculator::calculate("grand_child.a = 12", vars));
    REQUIRE(Calculator::calculate("parent.a", vars).asReal() == 10);
    REQUIRE(Calculator::calculate("child.a", vars).asReal() == 10);
    REQUIRE(Calculator::calculate("grand_child.a", vars).asReal() == 12);
}

//TEST_CASE("Map usage expressions", "[map][map-usage]")
void map_usage_expressions()
{
    TokenMap vars;
    vars["my_map"] = TokenMap();
    REQUIRE_NOTHROW(Calculator::calculate("my_map['a'] = 1", vars));
    REQUIRE_NOTHROW(Calculator::calculate("my_map['b'] = 2", vars));
    REQUIRE_NOTHROW(Calculator::calculate("my_map['c'] = 3", vars));

    REQUIRE(vars["my_map"].str() == "{ \"a\": 1, \"b\": 2, \"c\": 3 }");
    REQUIRE(Calculator::calculate("my_map.len()", vars).asInt() == 3);

    REQUIRE_NOTHROW(Calculator::calculate("my_map.pop('b')", vars));

    REQUIRE(vars["my_map"].str() == "{ \"a\": 1, \"c\": 3 }");
    REQUIRE(Calculator::calculate("my_map.len()", vars).asReal() == 2);

    REQUIRE_NOTHROW(Calculator::calculate("default = my_map.pop('b', 3)", vars));
    REQUIRE(vars["default"].asInt() == 3);
}

//TEST_CASE("List usage expressions", "[list]")
void list_usage_expressions()
{
    TokenMap vars;
    vars["my_list"] = TokenList();

    REQUIRE_NOTHROW(Calculator::calculate("my_list.push(1)", vars));
    REQUIRE_NOTHROW(Calculator::calculate("my_list.push(2)", vars));
    REQUIRE_NOTHROW(Calculator::calculate("my_list.push(3)", vars));

    REQUIRE(vars["my_list"].str() == "[ 1, 2, 3 ]");
    REQUIRE(Calculator::calculate("my_list.len()", vars).asInt() == 3);

    REQUIRE_NOTHROW(Calculator::calculate("my_list.pop(1)", vars));

    REQUIRE(vars["my_list"].str() == "[ 1, 3 ]");
    REQUIRE(Calculator::calculate("my_list.len()", vars).asReal() == 2);

    REQUIRE_NOTHROW(Calculator::calculate("my_list.pop()", vars));
    REQUIRE(vars["my_list"].str() == "[ 1 ]");
    REQUIRE(Calculator::calculate("my_list.len()", vars).asReal() == 1);

    vars["list"] = TokenList();
    REQUIRE_NOTHROW(Calculator::calculate("list.push(4).push(5).push(6)", vars));
    REQUIRE_NOTHROW(Calculator::calculate("my_list.push(2).push(3)", vars));
    REQUIRE(vars["my_list"].str() == "[ 1, 2, 3 ]");
    REQUIRE(vars["list"].str() == "[ 4, 5, 6 ]");

    REQUIRE_NOTHROW(Calculator::calculate("concat = my_list + list", vars));
    REQUIRE(vars["concat"].str() == "[ 1, 2, 3, 4, 5, 6 ]");
    REQUIRE(Calculator::calculate("concat.len()", vars).asReal() == 6);

    // Reverse index like python:
    REQUIRE_NOTHROW(Calculator::calculate("concat[-2] = 10", vars));
    REQUIRE_NOTHROW(Calculator::calculate("concat[2] = '3'", vars));
    REQUIRE_NOTHROW(Calculator::calculate("concat[3] = None", vars));
    REQUIRE(vars["concat"].str() == "[ 1, 2, \"3\", None, 10, 6 ]");

    // List index out of range:
    REQUIRE_THROWS(Calculator::calculate("concat[10]", vars));
    REQUIRE_THROWS(Calculator::calculate("concat[-10]", vars));
    REQUIRE_THROWS(vars["concat"].asList()[10]);
    REQUIRE_THROWS(vars["concat"].asList()[-10]);

    // Testing push and pop functions:
    TokenList L;
    REQUIRE_NOTHROW(L.push("my value"));
    REQUIRE_NOTHROW(L.push(10));
    REQUIRE_NOTHROW(L.push(TokenMap()));
    REQUIRE_NOTHROW(L.push(TokenList()));

    REQUIRE(PackToken(L).str() == "[ \"my value\", 10, {}, [] ]");
    REQUIRE(L.pop().str() == "[]");
    REQUIRE(PackToken(L).str() == "[ \"my value\", 10, {} ]");
}

//TEST_CASE("Tuple usage expressions", "[tuple]")
void tuple_usage_expressions()
{
    TokenMap vars;
    Calculator c;

    REQUIRE_NOTHROW(c.compile("'key':'value'"));
    auto * t0 = static_cast<STuple *>(c.eval()->clone());
    REQUIRE(t0->m_type == STUPLE);
    REQUIRE(t0->list().size() == 2);
    delete t0;

    REQUIRE_NOTHROW(c.compile("1, 'key':'value', 3"));
    auto * t1 = static_cast<Tuple *>(c.eval()->clone());
    REQUIRE(t1->m_type == TUPLE);
    REQUIRE(t1->list().size() == 3);

    auto * t2 = static_cast<STuple *>(t1->list()[1]->clone());
    REQUIRE(t2->m_type == STUPLE);
    REQUIRE(t2->list().size() == 2);
    delete t1;
    delete t2;

    GlobalScope global;
    REQUIRE_NOTHROW(c.compile("pow, None"));
    REQUIRE(c.eval(global).str() == "([Function: pow], None)");
}

//TEST_CASE("List and map constructors usage")
void list_map_constructor_usage()
{
    GlobalScope vars;
    REQUIRE_NOTHROW(Calculator::calculate("my_map = map()", vars));
    REQUIRE_NOTHROW(Calculator::calculate("my_list = list()", vars));

    REQUIRE(vars["my_map"]->m_type == MAP);
    REQUIRE(vars["my_list"]->m_type == LIST);
    REQUIRE(Calculator::calculate("my_list.len()", vars).asReal() == 0);

    REQUIRE_NOTHROW(Calculator::calculate("my_list = list(1,'2',None,map(),list('sub_list'))", vars));
    REQUIRE(vars["my_list"].str() == "[ 1, \"2\", None, {}, [ \"sub_list\" ] ]");

    // Test initialization by Iterator:
    REQUIRE_NOTHROW(Calculator::calculate("my_map  = map()", vars));
    REQUIRE_NOTHROW(Calculator::calculate("my_map.a = 1", vars));
    REQUIRE_NOTHROW(Calculator::calculate("my_map.b = 2", vars));
    REQUIRE_NOTHROW(Calculator::calculate("my_list  = list(my_map)", vars));
    REQUIRE(vars["my_list"].str() == "[ \"a\", \"b\" ]");
}

//TEST_CASE("Map '{}' and list '[]' constructor usage")
void map_operator_constructor_usage()
{
    Calculator c1;
    TokenMap vars;

    REQUIRE_NOTHROW(c1.compile("{ 'a': 1 }.a"));
    REQUIRE(c1.eval().asInt() == 1);

    REQUIRE_NOTHROW(c1.compile("M = {'a': 1}"));
    REQUIRE(c1.eval().str() == "{ \"a\": 1 }");

    REQUIRE_NOTHROW(c1.compile("[ 1, 2 ].len()"));
    REQUIRE(c1.eval().asInt() == 2);

    REQUIRE_NOTHROW(c1.compile("L = [1,2]"));
    REQUIRE(c1.eval().str() == "[ 1, 2 ]");
}

//TEST_CASE("Test list iterable behavior")
void test_list_iterable_behavior()
{
    GlobalScope vars;
    REQUIRE_NOTHROW(Calculator::calculate("L = list(1,2,3)", vars));
    TokenIterator * it;
    REQUIRE_NOTHROW(it = vars["L"].asList().getIterator());
    PackToken * next;
    REQUIRE_NOTHROW(next = it->next());
    REQUIRE(next != nullptr);
    REQUIRE(next->asReal() == 1);

    REQUIRE_NOTHROW(next = it->next());
    REQUIRE(next != nullptr);
    REQUIRE(next->asReal() == 2);

    REQUIRE_NOTHROW(next = it->next());
    REQUIRE(next != nullptr);
    REQUIRE(next->asReal() == 3);

    REQUIRE_NOTHROW(next = it->next());
    REQUIRE(next == nullptr);

    delete it;
}

//TEST_CASE("Test map iterable behavior")
void test_map_iterable_behavior()
{
    GlobalScope vars;
    vars["M"] = TokenMap();
    vars["M"]["a"] = 1;
    vars["M"]["b"] = 2;
    vars["M"]["c"] = 3;

    TokenIterator * it;
    REQUIRE_NOTHROW(it = vars["M"].asMap().getIterator());
    PackToken * next;
    REQUIRE_NOTHROW(next = it->next());
    REQUIRE(next != nullptr);
    REQUIRE(next->asString() == "a");

    REQUIRE_NOTHROW(next = it->next());
    REQUIRE(next != nullptr);
    REQUIRE(next->asString() == "b");

    REQUIRE_NOTHROW(next = it->next());
    REQUIRE(next != nullptr);
    REQUIRE(next->asString() == "c");

    REQUIRE_NOTHROW(next = it->next());
    REQUIRE(next == nullptr);

    delete it;
}

//TEST_CASE("Function usage expressions")
void function_usage_expression()
{
    GlobalScope vars;
    vars["pi"] = 3.141592653589793;
    vars["a"] = -4;

    REQUIRE(Calculator::calculate("sqrt(4)", vars).asReal() == 2);
    REQUIRE(Calculator::calculate("sin(pi)", vars).asReal() == Approx(0));
    REQUIRE(Calculator::calculate("cos(pi/2)", vars).asReal() == Approx(0));
    REQUIRE(Calculator::calculate("tan(pi)", vars).asReal() == Approx(0));
    Calculator c("a + sqrt(4) * 2");
    REQUIRE(c.eval(vars).asReal() == 0);
    REQUIRE(Calculator::calculate("sqrt(4-a*3) * 2", vars).asReal() == 8);
    REQUIRE(Calculator::calculate("abs(42)", vars).asReal() == 42);
    REQUIRE(Calculator::calculate("abs(-42)", vars).asReal() == 42);

    // With more than one argument:
    REQUIRE(Calculator::calculate("pow(2,2)", vars).asReal() == 4);
    REQUIRE(Calculator::calculate("pow(2,3)", vars).asReal() == 8);
    REQUIRE(Calculator::calculate("pow(2,a)", vars).asReal() == Approx(1. / 16));
    REQUIRE(Calculator::calculate("pow(2,a+4)", vars).asReal() == 1);

    REQUIRE_THROWS(Calculator::calculate("foo(10)"));
    REQUIRE_THROWS(Calculator::calculate("foo(10),"));
    REQUIRE_NOTHROW(Calculator::calculate("foo,(10)"));

    REQUIRE(TokenMap::default_global()["abs"].str() == "[Function: abs]");
    REQUIRE(Calculator::calculate("1,2,3,4,5").str() == "(1, 2, 3, 4, 5)");

    REQUIRE(Calculator::calculate(" float('0.1') ").asReal() == 0.1);
    REQUIRE(Calculator::calculate("float(10)").asReal() == 10);

    vars["a"] = 0;
    REQUIRE(Calculator::calculate(" eval('a = 3') ", vars).asReal() == 3);
    REQUIRE(vars["a"] == 3);

    vars["m"] = TokenMap();
    REQUIRE_THROWS(Calculator::calculate("1 + float(m) * 3", vars));
    REQUIRE_THROWS(Calculator::calculate("float('not a number')"));

    REQUIRE_NOTHROW(Calculator::calculate("pow(1,-10)"));
    REQUIRE_NOTHROW(Calculator::calculate("pow(1,+10)"));

    vars["base"] = 2;
    c.compile("pow(base,2)", vars);
    vars["base"] = 3;
    REQUIRE(c.eval().asReal() == 4);
    REQUIRE(c.eval(vars).asReal() == 9);
}

//TEST_CASE("Built-in extend() function")
void built_in_extend_function()
{
    GlobalScope vars;

    REQUIRE_NOTHROW(Calculator::calculate("a = map()", vars));
    REQUIRE_NOTHROW(Calculator::calculate("b = extend(a)", vars));
    REQUIRE_NOTHROW(Calculator::calculate("a.a = 10", vars));
    REQUIRE(Calculator::calculate("b.a", vars).asReal() == 10);
    REQUIRE_NOTHROW(Calculator::calculate("b.a = 20", vars));
    REQUIRE(Calculator::calculate("a.a", vars).asReal() == 10);
    REQUIRE(Calculator::calculate("b.a", vars).asReal() == 20);

    REQUIRE_NOTHROW(Calculator::calculate("c = extend(b)", vars));
    REQUIRE(Calculator::calculate("a.instanceof(b)", vars).asBool() == false);
    REQUIRE(Calculator::calculate("a.instanceof(c)", vars).asBool() == false);
    REQUIRE(Calculator::calculate("b.instanceof(a)", vars).asBool() == true);
    REQUIRE(Calculator::calculate("c.instanceof(a)", vars).asBool() == true);
    REQUIRE(Calculator::calculate("c.instanceof(b)", vars).asBool() == true);
}

// Used on the test case below:
PackToken map_str(TokenMap)
{
    return "custom map str";
}

//TEST_CASE("Built-in str() function")
void built_in_str_function()
{
    REQUIRE(Calculator::calculate(" str(None) ").asString() == "None");
    REQUIRE(Calculator::calculate(" str(10) ").asString() == "10");
    REQUIRE(Calculator::calculate(" str(10.1) ").asString() == "10.1");
    REQUIRE(Calculator::calculate(" str('texto') ").asString() == "texto");
    REQUIRE(Calculator::calculate(" str(list(1,2,3)) ").asString() == "[ 1, 2, 3 ]");
    REQUIRE(Calculator::calculate(" str(map()) ").asString() == "{}");
    REQUIRE(Calculator::calculate(" str(map) ").asString() == "[Function: map]");

    vars["iterator"] = PackToken(new TokenList());
    vars["iterator"]->m_type = IT;
    REQUIRE(Calculator::calculate("str(iterator)", vars).asString() == "[Iterator]");

    TokenMap vars;
    vars["my_map"] = TokenMap();
    vars["my_map"]["__str__"] = CppFunction(&map_str, {}, "map_str");
    // Test `packToken_str()` function declared on builtin-features/functions.h:
    REQUIRE(Calculator::calculate(" str(my_map) ", vars) == "custom map str");
}

//TEST_CASE("Multiple argument functions")
void multiple_argument_functions()
{
    GlobalScope vars;
    REQUIRE_NOTHROW(Calculator::calculate("total = sum(1,2,3,4)", vars));
    REQUIRE(vars["total"].asReal() == 10);
}

//TEST_CASE("Passing keyword arguments to functions", "[function][kwargs]")
void passing_keyword_arguments_to_functions()
{
    GlobalScope vars;
    Calculator c1;
    REQUIRE_NOTHROW(c1.compile("my_map = map('a':1,'b':2)", vars));
    REQUIRE_NOTHROW(c1.eval(vars));

    TokenMap map;
    REQUIRE_NOTHROW(map = vars["my_map"].asMap());

    REQUIRE(map["a"].asInt() == 1);
    REQUIRE(map["b"].asInt() == 2);

    qreal result;
    REQUIRE_NOTHROW(c1.compile("result = pow(2, 'exp': 3)"));
    REQUIRE_NOTHROW(c1.eval(vars));
    REQUIRE_NOTHROW(result = vars["result"].asReal());
    REQUIRE(result == 8.0);

    REQUIRE_NOTHROW(c1.compile("result = pow('exp': 3, 'number': 2)"));
    REQUIRE_NOTHROW(c1.eval(vars));
    REQUIRE_NOTHROW(result = vars["result"].asReal());
    REQUIRE(result == 8.0);
}

//TEST_CASE("Default functions")
void default_functions()
{
    REQUIRE(Calculator::calculate("type(None)").asString() == "none");
    REQUIRE(Calculator::calculate("type(10.0)").asString() == "real");
    REQUIRE(Calculator::calculate("type(10)").asString() == "integer");
    REQUIRE(Calculator::calculate("type(True)").asString() == "boolean");
    REQUIRE(Calculator::calculate("type('str')").asString() == "string");
    REQUIRE(Calculator::calculate("type(str)").asString() == "function");
    REQUIRE(Calculator::calculate("type(list())").asString() == "list");
    REQUIRE(Calculator::calculate("type(map())").asString() == "map");

    TokenMap vars;
    vars["mymap"] = TokenMap();
    vars["mymap"]["__type__"] = "my_type";
    REQUIRE(Calculator::calculate("type(mymap)", vars).asString() == "my_type");
}

//TEST_CASE("Type specific functions")
void type_specific_functions()
{
    TokenMap vars;
    vars["s1"] = "String";
    vars["s2"] = " a b ";

    REQUIRE(Calculator::calculate("s1.len()", vars).asReal() == 6);
    REQUIRE(Calculator::calculate("s1.lower()", vars).asString() == "string");
    REQUIRE(Calculator::calculate("s1.upper()", vars).asString() == "STRING");
    REQUIRE(Calculator::calculate("s2.strip()", vars).asString() == "a b");

    Calculator c1("L = 'a, b'.split(', ')", vars);
    REQUIRE(c1.eval(vars).str() == "[ \"a\", \"b\" ]");

    Calculator c2("L.join(', ')");
    REQUIRE(c2.eval(vars).asString() == "a, b");
}

//TEST_CASE("Assignment expressions")
void assignment_expressions()
{
    GlobalScope vars;
    Calculator::calculate("assignment = 10", vars);

    // Assigning to an unexistent variable works.
    REQUIRE(Calculator::calculate("assignment", vars).asReal() == 10);

    // Assigning to existent variables should work as well.
    REQUIRE_NOTHROW(Calculator::calculate("assignment = 20", vars));
    REQUIRE(Calculator::calculate("assignment", vars).asReal() == 20);

    // Chain assigning should work with a right-to-left order:
    REQUIRE_NOTHROW(Calculator::calculate("a = b = 20", vars));
    REQUIRE_NOTHROW(Calculator::calculate("a = b = c = d = 30", vars));
    REQUIRE(Calculator::calculate("a == b && b == c && b == d && d == 30", vars) == true);

    REQUIRE_NOTHROW(Calculator::calculate("teste='b'"));

    // The user should not be able to explicit overwrite variables
    // he did not declare. So by default he can't overwrite variables
    // on the global scope:
    REQUIRE_NOTHROW(Calculator::calculate("print = 'something'", vars));
    REQUIRE(vars["print"].asString() == "something");
    REQUIRE(TokenMap::default_global()["print"].str() == "[Function: print]");

    // But it should overwrite variables
    // on non-local scopes as expected:
    TokenMap child = vars.getChild();
    REQUIRE_NOTHROW(Calculator::calculate("print = 'something else'", vars));
    REQUIRE(vars["print"].asString() == "something else");
    REQUIRE(child["print"]->m_type == NONE);
}

//TEST_CASE("Assignment expressions on maps")
void assignment_expressions_on_maps()
{
    vars["m"] = TokenMap();
    Calculator::calculate("m['asn'] = 10", vars);

    // Assigning to an unexistent variable works.
    REQUIRE(Calculator::calculate("m['asn']", vars).asReal() == 10);

    // Assigning to existent variables should work as well.
    REQUIRE_NOTHROW(Calculator::calculate("m['asn'] = 20", vars));
    REQUIRE(Calculator::calculate("m['asn']", vars).asReal() == 20);

    // Chain assigning should work with a right-to-left order:
    REQUIRE_NOTHROW(Calculator::calculate("m.a = m.b = 20", vars));
    REQUIRE_NOTHROW(Calculator::calculate("m.a = m.b = m.c = m.d = 30", vars));
    REQUIRE(Calculator::calculate("m.a == m.b && m.b == m.c && m.b == m.d && m.d == 30", vars) == true);

    REQUIRE_NOTHROW(Calculator::calculate("m.m = m", vars));
    REQUIRE(Calculator::calculate("10 + (a = m.a = m.m.b)", vars) == 40);

    REQUIRE_NOTHROW(Calculator::calculate("m.m = None", vars));
    REQUIRE(Calculator::calculate("m.m", vars)->m_type == NONE);
}

//TEST_CASE("Scope management")
void scope_management()
{
    Calculator c("pi+b1+b2");
    TokenMap parent;
    parent["pi"] = 3.14;
    parent["b1"] = 0;
    parent["b2"] = 0.86;

    TokenMap child = parent.getChild();

    // Check scope extension:
    REQUIRE(c.eval(child).asReal() == Approx(4));

    child["b2"] = 1.0;
    REQUIRE(c.eval(child).asReal() == Approx(4.14));

    // Testing with 3 namespaces:
    TokenMap vmap = child.getChild();
    vmap["b1"] = -1.14;
    REQUIRE(c.eval(vmap).asReal() == Approx(3.0));

    TokenMap copy = vmap;
    Calculator c2("pi+b1+b2", copy);
    REQUIRE(c2.eval().asReal() == Approx(3.0));
    REQUIRE(Calculator::calculate("pi+b1+b2", copy).asReal() == Approx(3.0));
}

// Working as a slave parser implies it will return
// a pointer to the place it has stopped parsing
// and accept a list of delimiters that should make it stop.
//TEST_CASE("Parsing as slave parser")
void parsing_as_slave_parser()
{
    QString original_code = "a=1; b=2\n c=a+b }";
    QString code = original_code;
    int parsedTo = 0;
    TokenMap vars;
    Calculator c1, c2, c3;

    // With static function:
    REQUIRE_NOTHROW(Calculator::calculate(code, vars, ";}\n", &parsedTo));
    REQUIRE(parsedTo == 3);
    REQUIRE(vars["a"].asReal() == 1);

    code = code.mid(parsedTo);

    // With constructor:
    REQUIRE_NOTHROW((c2 = Calculator(code, vars, ";}\n", &parsedTo)));
    REQUIRE(parsedTo == 5);

    code = code.mid(parsedTo);

    // With compile method:
    REQUIRE_NOTHROW(c3.compile(code, vars, ";}\n", &parsedTo));
    REQUIRE(parsedTo == original_code.length() - 1);

    REQUIRE_NOTHROW(c2.eval(vars));
    REQUIRE(vars["b"] == 2);

    REQUIRE_NOTHROW(c3.eval(vars));
    REQUIRE(vars["c"] == 3);

    // Testing with delimiter between brackets of the expression:
    QString if_code = "if ( a+(b*c) == 3 ) { ... }";
    QString multiline = "a = (\n  1,\n  2,\n  3\n)\n print(a);";

    code = if_code;
    REQUIRE_NOTHROW(Calculator::calculate(if_code + 4, vars, ")", &parsedTo));
    REQUIRE(code.at(parsedTo) == if_code[18]);

    code = multiline;
    REQUIRE_NOTHROW(Calculator::calculate(multiline, vars, "\n;", &parsedTo));
    REQUIRE(code.at(parsedTo) == multiline[21]);

    QString error_test = "a = (;  1,;  2,; 3;)\n print(a);";
    REQUIRE_THROWS(Calculator::calculate(error_test, vars, "\n;", &parsedTo));
}

// This function is for internal use only:
//TEST_CASE("operation_id() function", "[op_id]")
void operation_id_function()
{
#define opIdTest(t1, t2) Operation::buildMask(t1, t2)
    REQUIRE((opIdTest(NONE, NONE)) == 0x0000000100000001);
    REQUIRE((opIdTest(FUNC, FUNC)) == 0x0000002000000020);
    REQUIRE((opIdTest(FUNC, ANY_TYPE)) == 0x000000200000FFFF);
    REQUIRE((opIdTest(FUNC, ANY_TYPE)) == 0x000000200000FFFF);
}

/* * * * * Declaring adhoc operations * * * * */

struct myCalc : public Calculator
{
    static Config & my_config()
    {
        static Config conf;
        return conf;
    }

    const Config & config() const override
    {
        return my_config();
    }

    using Calculator::Calculator;
};

PackToken op1(const PackToken & left, const PackToken & right,
              EvaluationData * data)
{
    return Calculator::defaultConfig().opMap["%"][0].exec(left, right, data);
}

PackToken op2(const PackToken & left, const PackToken & right,
              EvaluationData * data)
{
    return Calculator::defaultConfig().opMap[","][0].exec(left, right, data);
}

PackToken op3(const PackToken & left, const PackToken & right,
              EvaluationData *)
{
    return left.asReal() - right.asReal();
}

PackToken op4(const PackToken & left, const PackToken & right,
              EvaluationData *)
{
    return left.asReal() * right.asReal();
}

PackToken slash_op(const PackToken & left, const PackToken & right,
                   EvaluationData *)
{
    return left.asReal() / right.asReal();
}

PackToken not_unary_op(const PackToken &, const PackToken & right,
                       EvaluationData *)
{
    return ~right.asInt();
}

PackToken not_right_unary_op(const PackToken & left, const PackToken &,
                             EvaluationData *)
{
    return ~left.asInt();
}

PackToken lazy_increment(const PackToken &, const PackToken &,
                         EvaluationData * data)
{
    auto var_name = data->left->m_key.asString();

    TokenMap * map_p = data->scope.findMap(var_name);

    if (map_p == nullptr)
    {
        map_p = &data->scope;
    }

    PackToken value = (*map_p)[var_name];
    (*map_p)[var_name] = value.asInt() + 1;
    return value;
}

PackToken eager_increment(const PackToken &, const PackToken &,
                          EvaluationData * data)
{
    auto var_name = data->right->m_key.asString();

    TokenMap * map_p = data->scope.findMap(var_name);

    if (map_p == nullptr)
    {
        map_p = &data->scope;
    }

    return (*map_p)[var_name] = (*map_p)[var_name].asInt() + 1;
}

PackToken assign_right(const PackToken & left, const PackToken &,
                       EvaluationData * data)
{
    auto var_name = data->right->m_key.asString();

    TokenMap * map_p = data->scope.findMap(var_name);

    if (map_p == nullptr)
    {
        map_p = &data->scope;
    }

    return (*map_p)[var_name] = left;
}

PackToken assign_left(const PackToken &, const PackToken & right,
                      EvaluationData * data)
{
    auto var_name = data->left->m_key.asString();

    TokenMap * map_p = data->scope.findMap(var_name);

    if (map_p == nullptr)
    {
        map_p = &data->scope;
    }

    return (*map_p)[var_name] = right;
}

void slash(const char * expr, const char ** rest, RpnBuilder * data)
{
    data->handleOp("*");

    // Eat the next character:
    *rest = ++expr;
}

void slash_slash(const char *, const char **, RpnBuilder * data)
{
    data->handleOp("-");
}

struct myCalcStartup
{
    myCalcStartup()
    {
        OpPrecedenceMap & opp = myCalc::my_config().opPrecedence;
        opp.add(".", 1);
        opp.add("+", 2);
        opp.add("*", 2);
        opp.add("/", 3);
        opp.add("<=", 4);
        opp.add("=>", 4);

        // This operator will evaluate from right to left:
        opp.add("-", -3);

        // Unary operators:
        opp.addUnary("$$", 2);
        opp.addUnary("~", 4);
        opp.addRightUnary("!", 1);
        opp.addRightUnary("$$", 2);
        opp.addRightUnary("~", 4);

        OpMap & opMap = myCalc::my_config().opMap;
        opMap.add({STR, "+", TUPLE}, &op1);
        opMap.add({ANY_TYPE, ".", ANY_TYPE}, &op2);
        opMap.add({NUM, "-", NUM}, &op3);
        opMap.add({NUM, "*", NUM}, &op4);
        opMap.add({NUM, "/", NUM}, &slash_op);
        opMap.add({UNARY, "~", NUM}, &not_unary_op);
        opMap.add({NUM, "~", UNARY}, &not_right_unary_op);
        opMap.add({NUM, "!", UNARY}, &not_right_unary_op);
        opMap.add({NUM, "$$", UNARY}, &lazy_increment);
        opMap.add({UNARY, "$$", NUM}, &eager_increment);
        opMap.add({ANY_TYPE, "=>", REF}, &assign_right);
        opMap.add({REF, "<=", ANY_TYPE}, &assign_left);

        ParserMap & parser = myCalc::my_config().parserMap;
        parser.add('/', &slash);
        parser.add("//", &slash_slash);
    }
} myCalcStartup;

/* * * * * Testing adhoc operations * * * * */

//TEST_CASE("Adhoc operations", "[operation][config]")
void adhoc_operations()
{
    myCalc c1, c2;
    const char * exp = "'Lets create %s operators%s' + ('adhoc' . '!' )";
    REQUIRE_NOTHROW(c1.compile(exp));
    REQUIRE_NOTHROW(c2 = myCalc(exp, vars, nullptr, nullptr, myCalc::my_config()));

    REQUIRE(c1.eval() == "Lets create adhoc operators!");
    REQUIRE(c2.eval() == "Lets create adhoc operators!");

    // Testing opPrecedence:
    exp = "'Lets create %s operators%s' + 'adhoc' . '!'";
    REQUIRE_NOTHROW(c1.compile(exp));
    REQUIRE(c1.eval() == "Lets create adhoc operators!");

    exp = "2 - 1 * 1";  // 2 - (1 * 1)
    REQUIRE_NOTHROW(c1.compile(exp));
    REQUIRE(c1.eval() == 1);

    // Testing op associativity:
    exp = "2 - 1";
    REQUIRE_NOTHROW(c1.compile(exp));
    REQUIRE(c1.eval() == 1);

    // Associativity right to left, i.e. 2 - (1 - 1)
    exp = "2 - 1 - 1";
    REQUIRE_NOTHROW(c1.compile(exp));
    REQUIRE(c1.eval() == 2);
}

//TEST_CASE("Adhoc unary operations", "[operation][unary][config]")
void adhoc_unary_operations()
{
    //SECTION("Left Unary Operators")
    {
        myCalc c1;

        // * * Using custom unary operators: * * //

        REQUIRE_NOTHROW(c1.compile("~10"));
        REQUIRE(c1.eval().asInt() == ~10l);

        REQUIRE_NOTHROW(c1.compile("2 * ~10"));
        REQUIRE(c1.eval().asInt() == 2 * ~10l);

        REQUIRE_NOTHROW(c1.compile("2 * ~10 * 3"));
        REQUIRE(c1.eval().asInt() == 2 * ~(10l * 3));

        Calculator c2;

        // * * Using built-in unary operators: * * //

        // Testing inside brackets:
        REQUIRE_NOTHROW(c2.compile("(2 * -10) * 3"));
        REQUIRE(c2.eval() == 2 * -10 * 3);

        REQUIRE_NOTHROW(c2.compile("2 * (-10 * 3)"));
        REQUIRE(c2.eval() == 2 * (-10 * 3));

        REQUIRE_NOTHROW(c2.compile("2 * -(10 * 3)"));
        REQUIRE(c2.eval() == 2 * -(10 * 3));

        // Testing opPrecedence:
        REQUIRE_NOTHROW(c2.compile("-10 - 2"));  // (-10) - 2
        REQUIRE(c2.eval() == -12);

        TokenMap vars;
        vars["scope_map"] = TokenMap();
        vars["scope_map"]["my_var"] = 10;

        REQUIRE_NOTHROW(c2.compile("- scope_map . my_var"));  // - (map . key2)
        REQUIRE(c2.eval(vars) == -10);
    }

    //SECTION("Right unary operators")
    {
        myCalc c1;

        // Testing with lower op precedence:
        REQUIRE_NOTHROW(c1.compile("10~"));
        REQUIRE(c1.eval().asInt() == ~10l);

        REQUIRE_NOTHROW(c1.compile("2 * 10~"));
        REQUIRE(c1.eval().asInt() == ~(2 * 10l));

        REQUIRE_NOTHROW(c1.compile("2 * 10~ * 3"));
        REQUIRE(c1.eval().asInt() == ~(2 * 10l) * 3);

        // Testing with higher op precedence:
        REQUIRE_NOTHROW(c1.compile("10!"));
        REQUIRE(c1.eval().asInt() == ~10l);

        REQUIRE_NOTHROW(c1.compile("2 * 10!"));
        REQUIRE(c1.eval().asInt() == 2 * ~10l);

        REQUIRE_NOTHROW(c1.compile("2 * 10! * 3"));
        REQUIRE(c1.eval().asInt() == 2 * ~10l * 3);

        // Testing inside brackets:
        REQUIRE_NOTHROW(c1.compile("2 * (10~ * 3)"));
        REQUIRE(c1.eval().asInt() == 2 * ~10l * 3);

        REQUIRE_NOTHROW(c1.compile("(2 * 10~) * 3"));
        REQUIRE(c1.eval().asInt() == ~(2 * 10l) * 3);

        REQUIRE_NOTHROW(c1.compile("(2 * 10)~ * 3"));
        REQUIRE(c1.eval().asInt() == ~(2 * 10l) * 3);
    }
}

//TEST_CASE("Adhoc reference operations", "[operation][reference][config]")
void adhoc_reference_operations()
{
    myCalc c1;
    TokenMap scope;

    scope["a"] = 10;
    REQUIRE_NOTHROW(c1.compile("$$ a"));
    REQUIRE(c1.eval(scope) == 11);
    REQUIRE(scope["a"] == 11);

    scope["a"] = 10;
    REQUIRE_NOTHROW(c1.compile("a $$"));
    REQUIRE(c1.eval(scope) == 10);
    REQUIRE(scope["a"] == 11);

    scope["a"] = PackToken::None();
    REQUIRE_NOTHROW(c1.compile("a <= 20"));
    REQUIRE(c1.eval(scope) == 20);
    REQUIRE(scope["a"] == 20);

    scope["a"] = PackToken::None();
    REQUIRE_NOTHROW(c1.compile("30 => a"));
    REQUIRE(c1.eval(scope) == 30);
    REQUIRE(scope["a"] == 30);
}

//TEST_CASE("Adhoc reservedWord parsers", "[parser][config]")
void adhoc_reservedWord_parsers()
{
    myCalc c1;

    REQUIRE_NOTHROW(c1.compile("2 / 2"));
    REQUIRE(c1.eval().asInt() == 1);

    REQUIRE_NOTHROW(c1.compile("2 // 2"));
    REQUIRE(c1.eval().asInt() == 0);

    REQUIRE_NOTHROW(c1.compile("2 /? 2"));
    REQUIRE(c1.eval().asInt() == 4);

    REQUIRE_NOTHROW(c1.compile("2 /! 2"));
    REQUIRE(c1.eval().asInt() == 4);
}

//TEST_CASE("Custom parser for operator ':'", "[parser]")
void custom_parser_for_operator()
{
    PackToken p1;
    Calculator c2;

    REQUIRE_NOTHROW(c2.compile("{ a : 1 }"));
    REQUIRE_NOTHROW(p1 = c2.eval());
    REQUIRE(p1["a"] == 1);

    REQUIRE_NOTHROW(c2.compile("map(a : 1, b:2, c: \"c\")"));
    REQUIRE_NOTHROW(p1 = c2.eval());
    REQUIRE(p1["a"] == 1);
    REQUIRE(p1["b"] == 2);
    REQUIRE(p1["c"] == "c");
}

//TEST_CASE("Resource management")
void resource_management()
{
    Calculator C1, C2("1 + 1");

    // These are likely to cause seg fault if
    // RPN copy is not handled:

    // Copy:
    REQUIRE_NOTHROW(Calculator C3(C2));
    // Assignment:
    REQUIRE_NOTHROW(C1 = C2);
}

/* * * * * Testing adhoc operator parser * * * * */

//TEST_CASE("Adhoc operator parser", "[operator]")
void adhoc_operator_parser()
{
    // Testing comments:
    REQUIRE(Calculator::calculate("1 + 1 # And a comment!").asInt() == 2);
    REQUIRE(Calculator::calculate("1 + 1 /*And a comment!*/").asInt() == 2);
    REQUIRE(Calculator::calculate("1 /* + 1 */").asInt() == 1);
    REQUIRE(Calculator::calculate("1 /* in-between */ + 1").asInt() == 2);

    REQUIRE_THROWS(Calculator::calculate("1 + 1 /* Never ending comment"));

    TokenMap vars;
    QString expr = "#12345\n - 10";
    int parsedTo = 0;
    REQUIRE_NOTHROW(Calculator::calculate(expr, vars, "\n", &parsedTo));
    REQUIRE(parsedTo == 5);

    REQUIRE(Calculator::calculate(expr.mid(5)).asInt() == -10);
}

//TEST_CASE("Exception management")
void exception_management()
{
    Calculator ecalc1, ecalc2;
    ecalc1.compile("a+b+del", emap);
    emap["del"] = 30;

    REQUIRE_THROWS(ecalc2.compile(""));
    REQUIRE_THROWS(ecalc2.compile("      "));

    // Uninitialized calculators should eval to None:
    REQUIRE(Calculator().eval().str() == "None");

    REQUIRE_THROWS(ecalc1.eval());
    REQUIRE_NOTHROW(ecalc1.eval(emap));

    emap.erase("del");
    REQUIRE_THROWS(ecalc1.eval(emap));

    emap["del"] = 0;
    emap.erase("a");
    REQUIRE_NOTHROW(ecalc1.eval(emap));

    REQUIRE_NOTHROW(Calculator c5("10 + - - 10"));
    REQUIRE_THROWS(Calculator c5("10 + +"));
    REQUIRE_NOTHROW(Calculator c5("10 + -10"));
    REQUIRE_THROWS(Calculator c5("c.[10]"));

    TokenMap v1;
    v1["map"] = TokenMap();
    // Mismatched types, no supported operators.
    REQUIRE_THROWS(Calculator("map * 0").eval(v1));

    // This test attempts to cause a memory leak:
    // To see if it still works run with `make check`
    REQUIRE_THROWS(Calculator::calculate("a+2*no_such_variable", vars));

    REQUIRE_THROWS(ecalc2.compile("print('hello'))"));
    REQUIRE_THROWS(ecalc2.compile("map()['hello']]"));
    REQUIRE_THROWS(ecalc2.compile("map(['hello']]"));
}

void cparse::runTests()
{
    qInfo() << "running cparse tests";
    PREPARE_ENVIRONMENT();
    static_calculate_calculate();
    calculate_compile();
    numerical_expressions();
    boolean_expressions();
    string_expressions();
    operator_parsing();
    reference_counting();
    string_operations();
    map_expressions();
    prototypical_inheritance();
    map_usage_expressions();
    list_usage_expressions();
    tuple_usage_expressions();
    list_map_constructor_usage();
    map_operator_constructor_usage();
    test_list_iterable_behavior();
    test_map_iterable_behavior();
    function_usage_expression();
    built_in_extend_function();
    built_in_str_function();
    multiple_argument_functions();
    passing_keyword_arguments_to_functions();
    default_functions();
    type_specific_functions();
    assignment_expressions();
    assignment_expressions_on_maps();
    scope_management();
    parsing_as_slave_parser();
    operation_id_function();
    adhoc_operations();
    adhoc_unary_operations();
    adhoc_reference_operations();
    adhoc_reservedWord_parsers();
    custom_parser_for_operator();
    resource_management();
    adhoc_operator_parser();
    exception_management();
    qInfo() << "cparse tests finished";
}
