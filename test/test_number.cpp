#include "catch.hpp"

#include <qmex.hpp>

using namespace qmex;

TEST_CASE("Number <=> String")
{
    char buf[Number::precision + 5] = { '-', '0', '.' };

    int f = 1;
    for (int i = 0; i < Number::precision + 1; ++i)
        f *= 10;

    for (int i = 0; i < f; ++i)
    {
        sprintf(&buf[3], "%0*d", Number::precision + 1, i);

        for (int j = 0; j < 2; ++j)
        {
            buf[1] = '0';
            for (int k = 0; k < 2; ++k)
            {
                buf[1] += k;
                Number n(&buf[j]);

                char expected[sizeof(buf) - 1];
                std::memcpy(expected, buf, sizeof(expected));
                char* p = &expected[sizeof(expected) - 1];
                while (*--p == '0') continue;
                p[*p != '.'] = '\0';
                p = &expected[j];

                if (p[0] == '-' && p[1] == '0' && p[2] == '\0')
                    ++p;

                char actual[sizeof(buf)];
                std::size_t actual_len = n.toString(actual, sizeof(actual));
                REQUIRE(actual[actual_len] == '\0');
                CHECK_THAT(actual, Catch::Matchers::Equals(p));
            }
        }
    }
}

TEST_CASE("Number <=> Double")
{
    CHECK(0 == (double)Number("0.00"));
    CHECK(0 == (double)Number("-0.00"));
    CHECK(12.5 == (double)Number("12.50"));
    CHECK(-12.5 == (double)Number("-12.50"));
    CHECK(0.05 == (double)Number("0.050"));
    CHECK(-0.05 == (double)Number("-0.050"));
    CHECK("0" == (std::string)Number(0.0));
    CHECK("0" == (std::string)Number(-0.0));
    CHECK("12.5" == (std::string)Number(12.50));
    CHECK("-12.5" == (std::string)Number(-12.50));
    CHECK("0.05" == (std::string)Number(0.05));
    CHECK("-0.05" == (std::string)Number(-0.05));
}

TEST_CASE("Infinity Number")
{
    CHECK(Number(1000 * 1000) < Number::inf());
    CHECK(Number(-1000 * 1000) > Number::neginf());
    CHECK(Number(std::numeric_limits<Number::integer>::max()) == Number::inf());
    CHECK(Number(std::numeric_limits<Number::integer>::min()) == Number::neginf());
    CHECK(Number(INFINITY) == Number::inf());
    CHECK(Number(-INFINITY) == Number::neginf());
    CHECK(INFINITY == (double)Number("Inf"));
    CHECK(-INFINITY == (double)Number("-infiniTY"));
    CHECK("inf" == (std::string)Number("InFiNiTy"));
    CHECK("-inf" == (std::string)Number("-iNF"));
}
