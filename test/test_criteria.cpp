#include "catch.hpp"

#include <qmex.hpp>

using namespace qmex;

TEST_CASE("Criteria Constructor")
{
    CHECK_THROWS_AS(Criteria(nullptr, nullptr), CriteriaFormatError);
    CHECK_THROWS_AS(Criteria("A.EQ", nullptr), ValueTypeError);
    CHECK_THROWS_AS(Criteria(".EQ", "3"), CriteriaFormatError);
    CHECK_NOTHROW(Criteria("A.MH", "a|A"));
    CHECK_NOTHROW(Criteria("A_EQ", "0x3"));
    CHECK_THROWS_AS(Criteria("A.EQ", "a"), ValueTypeError);
    CHECK_NOTHROW(Criteria("A.LT", "3.5"));
    CHECK_THROWS_AS(Criteria("A_LT", "a"), ValueTypeError);
    CHECK_NOTHROW(Criteria("A_LE", "inf"));
    CHECK_THROWS_AS(Criteria("A.LE", "b"), ValueTypeError);
    CHECK_NOTHROW(Criteria("A.GT", "-inf"));
    CHECK_THROWS_AS(Criteria("A_GT", "c"), ValueTypeError);
    CHECK_NOTHROW(Criteria("A_GE", "-3.5"));
    CHECK_THROWS_AS(Criteria("A.GE", "d"), ValueTypeError);
}

TEST_CASE("Criteria MH")
{
    char pattern[] = "a|0x5*";
    Criteria c("A.MH", pattern);
    CHECK_THROWS_AS(c.distance(KeyValue("A", 3)), ValueTypeError);
    CHECK(c.distance(KeyValue("B", "a")) < 0);
    CHECK(c.distance(KeyValue("B", 3)) < 0);
    CHECK(c.distance(KeyValue("A", "a")) == 0);
    CHECK(c.distance(KeyValue("A", "A")) == 0);
    CHECK(c.distance(KeyValue("A", "ab")) == (Criteria::max)());
    CHECK(c.distance(KeyValue("A", "0X5")) == 0);
    CHECK(c.distance(KeyValue("A", "0X54")) == 0);
    CHECK(c.distance(KeyValue("A", "0X")) == (Criteria::max)());
}

TEST_CASE("Criteria EQ")
{
    Criteria c("A.EQ", "12.50");
    CHECK_THROWS_AS(c.distance(KeyValue("A", "ab")), ValueTypeError);
    CHECK(c.distance(KeyValue("B", "a")) < 0);
    CHECK(c.distance(KeyValue("B", 3)) < 0);
    CHECK(c.distance(KeyValue("A", 12.5)) == 0);
    CHECK(c.distance(KeyValue("A", "12.5")) == 0);
    CHECK(c.distance(KeyValue("A", 12.49)) == (Criteria::max)());
    CHECK(c.distance(KeyValue("A", "12.49")) == (Criteria::max)());
}

TEST_CASE("Criteria LT")
{
    Criteria cn("A.LT", "-inf");
    Criteria cn1("A.LT", "-1");
    Criteria c0("A.LT", "0");
    Criteria ci("A.LT", "inf");
    CHECK_THROWS_AS(cn.distance(KeyValue("A", "ab")), ValueTypeError);
    CHECK(cn1.distance(KeyValue("B", "a")) < 0);
    CHECK(c0.distance(KeyValue("B", 3)) < 0);
    CHECK(cn.distance(KeyValue("A", -INFINITY)) == (Criteria::max)());
    CHECK(cn.distance(KeyValue("A", "-inf")) == (Criteria::max)());
    CHECK(cn.distance(KeyValue("A", -2)) == (Criteria::max)());
    CHECK(cn.distance(KeyValue("A", "-2")) == (Criteria::max)());
    CHECK(cn1.distance(KeyValue("A", -2)) < c0.distance(KeyValue("A", -2)));
    CHECK(cn1.distance(KeyValue("A", "-2")) < c0.distance(KeyValue("A", -2)));
    CHECK(c0.distance(KeyValue("A", 0.0)) == (Criteria::max)());
    CHECK(c0.distance(KeyValue("A", -0.1)) < ci.distance(KeyValue("A", -0.1)));
    CHECK(c0.distance(KeyValue("A", 0.0)) > ci.distance(KeyValue("A", 0.0)));
    CHECK(ci.distance(KeyValue("A", -INFINITY)) < (Criteria::max)());
}

TEST_CASE("Criteria LE")
{
    Criteria cn("A.LE", "-inf");
    Criteria cn1("A.LE", "-1");
    Criteria c0("A.LE", "0");
    Criteria ci("A.LE", "inf");
    CHECK_THROWS_AS(cn.distance(KeyValue("A", "ab")), ValueTypeError);
    CHECK(cn1.distance(KeyValue("B", "a")) < 0);
    CHECK(c0.distance(KeyValue("B", 3)) < 0);
    CHECK(cn.distance(KeyValue("A", -INFINITY)) == 0);
    CHECK(cn.distance(KeyValue("A", -2)) == (Criteria::max)());
    CHECK(c0.distance(KeyValue("A", 0.0)) == 0);
    CHECK(c0.distance(KeyValue("A", "0")) == 0);
    CHECK(c0.distance(KeyValue("A", 0.0)) < ci.distance(KeyValue("A", 0.0)));
    CHECK(ci.distance(KeyValue("A", -INFINITY)) < (Criteria::max)());
}

TEST_CASE("Criteria GT")
{
    Criteria cn("A.GT", "-inf");
    Criteria cn1("A.GT", "-1");
    Criteria c0("A.GT", "0");
    Criteria ci("A.GT", "inf");
    CHECK_THROWS_AS(cn.distance(KeyValue("A", "ab")), ValueTypeError);
    CHECK(cn1.distance(KeyValue("B", "a")) < 0);
    CHECK(c0.distance(KeyValue("B", 3)) < 0);
    CHECK(cn.distance(KeyValue("A", -INFINITY)) == (Criteria::max)());
    CHECK(cn.distance(KeyValue("A", -2)) < (Criteria::max)());
    CHECK(c0.distance(KeyValue("A", 0.0)) == (Criteria::max)());
    CHECK(c0.distance(KeyValue("A", 0.0)) == ci.distance(KeyValue("A", 0.0)));
    CHECK(ci.distance(KeyValue("A", INFINITY)) == (Criteria::max)());
}

TEST_CASE("Criteria GE")
{
    Criteria cn("A.GE", "-inf");
    Criteria cn1("A.GE", "-1");
    Criteria c0("A.GE", "0");
    Criteria ci("A.GE", "inf");
    CHECK_THROWS_AS(cn.distance(KeyValue("A", "ab")), ValueTypeError);
    CHECK(cn1.distance(KeyValue("B", "a")) < 0);
    CHECK(c0.distance(KeyValue("B", 3)) < 0);
    CHECK(cn.distance(KeyValue("A", -INFINITY)) == 0);
    CHECK(cn.distance(KeyValue("A", -2)) < (Criteria::max)());
    CHECK(c0.distance(KeyValue("A", 0.0)) == 0);
    CHECK(c0.distance(KeyValue("A", 0.0)) < ci.distance(KeyValue("A", 0.0)));
    CHECK(ci.distance(KeyValue("A", INFINITY)) == 0);
    CHECK(ci.distance(KeyValue("A", "inf")) == 0);
}
