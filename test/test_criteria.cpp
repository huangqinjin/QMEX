#include "catch.hpp"

#include <qmex.hpp>

using namespace qmex;

TEST_CASE("Criteria Constructor")
{
    CHECK_THROWS(Criteria(nullptr, nullptr));
    CHECK_THROWS(Criteria("A.EQ", nullptr));
    CHECK_THROWS(Criteria(".EQ", "3"));
    CHECK_NOTHROW(Criteria("A.MH", "a|A"));
    CHECK_NOTHROW(Criteria("A_EQ", "0x3"));
    CHECK_THROWS(Criteria("A.EQ", "a"));
    CHECK_NOTHROW(Criteria("A.LT", "3.5"));
    CHECK_THROWS(Criteria("A_LT", "a"));
    CHECK_NOTHROW(Criteria("A_LE", "inf"));
    CHECK_THROWS(Criteria("A.LE", "b"));
    CHECK_NOTHROW(Criteria("A.GT", "-inf"));
    CHECK_THROWS(Criteria("A_GT", "c"));
    CHECK_NOTHROW(Criteria("A_GE", "-3.5"));
    CHECK_THROWS(Criteria("A.GE", "d"));
}

TEST_CASE("Criteria MH")
{
    char pattern[] = "a|0x5*";
    Criteria c("A.MH", pattern);
    CHECK(c.distance(KeyValue("A", 3)) == Criteria::VALUE_NOT_STRING);
    CHECK(c.distance(KeyValue("B", "a")) == Criteria::KEY_MISMATCH);
    CHECK(c.distance(KeyValue("B", 3)) == Criteria::KEY_MISMATCH);
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
    CHECK(c.distance(KeyValue("A", "ab")) == Criteria::VALUE_NOT_NUMBER);
    CHECK(c.distance(KeyValue("B", "a")) == Criteria::KEY_MISMATCH);
    CHECK(c.distance(KeyValue("B", 3)) == Criteria::KEY_MISMATCH);
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
    CHECK(cn.distance(KeyValue("A", "ab")) == Criteria::VALUE_NOT_NUMBER);
    CHECK(cn1.distance(KeyValue("B", "a")) == Criteria::KEY_MISMATCH);
    CHECK(c0.distance(KeyValue("B", 3)) == Criteria::KEY_MISMATCH);
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
    CHECK(cn.distance(KeyValue("A", "ab")) == Criteria::VALUE_NOT_NUMBER);
    CHECK(cn1.distance(KeyValue("B", "a")) == Criteria::KEY_MISMATCH);
    CHECK(c0.distance(KeyValue("B", 3)) == Criteria::KEY_MISMATCH);
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
    CHECK(cn.distance(KeyValue("A", "ab")) == Criteria::VALUE_NOT_NUMBER);
    CHECK(cn1.distance(KeyValue("B", "a")) == Criteria::KEY_MISMATCH);
    CHECK(c0.distance(KeyValue("B", 3)) == Criteria::KEY_MISMATCH);
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
    CHECK(cn.distance(KeyValue("A", "ab")) == Criteria::VALUE_NOT_NUMBER);
    CHECK(cn1.distance(KeyValue("B", "a")) == Criteria::KEY_MISMATCH);
    CHECK(c0.distance(KeyValue("B", 3)) == Criteria::KEY_MISMATCH);
    CHECK(cn.distance(KeyValue("A", -INFINITY)) == 0);
    CHECK(cn.distance(KeyValue("A", -2)) < (Criteria::max)());
    CHECK(c0.distance(KeyValue("A", 0.0)) == 0);
    CHECK(c0.distance(KeyValue("A", 0.0)) < ci.distance(KeyValue("A", 0.0)));
    CHECK(ci.distance(KeyValue("A", INFINITY)) == 0);
    CHECK(ci.distance(KeyValue("A", "inf")) == 0);
}
