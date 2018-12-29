
//
// Copyright (c) 2018-2019 Huang Qinjin (huangqinjin@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cstdio>

#include "qmex.hpp"

using namespace qmex;

namespace
{
    int factor(int precision = Number::precision) noexcept
    {
        int f = 1;
        for (int i = 0; i < precision; ++i)
            f *= 10;
        return f;
    }
}


Number Number::inf() noexcept
{
    Number n;
    n.n = (std::numeric_limits<integer>::max)();
    return n;
}

Number Number::neginf() noexcept
{
    Number n;
    n.n = (std::numeric_limits<integer>::min)();
    return n;
}

bool Number::operator==(const Number& other) const noexcept { return n == other.n; }
bool Number::operator!=(const Number& other) const noexcept { return n != other.n; }
bool Number::operator<=(const Number& other) const noexcept { return n <= other.n; }
bool Number::operator< (const Number& other) const noexcept { return n <  other.n; }
bool Number::operator>=(const Number& other) const noexcept { return n >= other.n; }
bool Number::operator> (const Number& other) const noexcept { return n >  other.n; }

Number::Number() noexcept : n(0) {}
Number::Number(double d) noexcept(false) { *this = d; }
Number::Number(String s) noexcept(false) { *this = s; }

Number& Number::operator=(double d) noexcept(false)
{
    if (d != d) throw std::invalid_argument("Can not convert NaN to fixed point number");
    d *= factor();
    if (d >= 0) d += 0.5; // rounding
    else d -= 0.5;
    if (d >= inf().n) n = inf().n;
    else if (d <= neginf().n) n = neginf().n;
    else n = static_cast<integer>(d);
    return *this;
}

Number& Number::operator=(String s) noexcept(false)
{
    if (!s || !*s)
    {
        s = "nil";
        goto error;
    }
    else
    {
        const char* infs[] = {
           "inf",
           "infinity",
        };

        for (int i = 0; i < sizeof(infs) / sizeof(infs[0]); ++i)
        {
            const char* p = infs[i];
            const char* q = s;
            if (*q == '-') ++q;
            while (*p && *q && (*p == *q || (*p - 'i' + 'I' == *q)))
                ++p, ++q;
            if (*p == *q)
            {
                if (*s == '-') n = neginf().n;
                else n = inf().n;
                return *this;
            }
        }
    }

    if (*s != '-' && (*s < '0' || *s > '9'))
        goto error;

    errno = 0;
    char* end;
    long l = std::strtol(s, &end, 0);
    long m = 0;
    if (*end != '.' && *end != '\0') goto error;

    if (end[0] == '.' && end[1] != '\0') // fraction part, only supports base-10
    {
        const char* p = end;
        while (*++p) if (*p < '0' || *p > '9') goto error;

        if (errno != ERANGE)
        {
            char buf[precision + 1] = { 0 };
            int i = 0;
            while (i < precision && *++end) buf[i++] = *end;
            while (i < precision) buf[i++] = '0';

            m = std::strtol(buf, nullptr, 10);
        }
    }

    if (*s == '-')
    {
        if (l <= (LONG_MIN + m) / factor()) l = LONG_MIN;
        else l = l * factor() - m;
    }
    else
    {
        if (l >= (LONG_MAX - m) / factor()) l = LONG_MAX;
        else l = l * factor() + m;
    }

    if (l == LONG_MAX || l >= inf().n) n = inf().n;
    else if (l == LONG_MIN || l <= neginf().n) n = neginf().n;
    else n = static_cast<integer>(l);

    return *this;
error:
    throw std::invalid_argument(std::string("Can not convert [") + s + "] to fixed point number");
}

Number::operator double() const noexcept
{
    if (n == inf().n) return std::numeric_limits<double>::infinity();
    if (n == neginf().n) return -std::numeric_limits<double>::infinity();
    return n * 1.0 / factor();
}

Number::operator std::string() const noexcept
{
    std::string s(toString(nullptr, 0), '\0');
    toString(&s[0], s.size() + 1);
    return s;
}

std::size_t Number::toString(char buf[], std::size_t bufsz) const noexcept
{
    if (n == 0)
        return snprintf(buf, bufsz, "0");
    if (n == inf().n)
        return snprintf(buf, bufsz, "inf");
    if (n == neginf().n)
        return snprintf(buf, bufsz, "-inf");

    integer m = n;
    int f = factor(1);
    int p = precision;
    while (p > 0 && (m % f) == 0) --p, m /= f;

    if (p == 0)
        return snprintf(buf, bufsz, "%d", m);

    f = factor(p);
    bool normal = false;
    if (m > 0 && m < f) m += f;
    else if (m < 0 && -m < f) m -= f;
    else normal = true;

    std::size_t len = snprintf(buf, bufsz, "%d", m) + 1;
    if (buf == nullptr || bufsz <= 1) return len;

    if (!normal && buf[0] != '-') buf[0] = '0';
    if (!normal && buf[0] == '-' && buf[1] != '\0') buf[1] = '0';

    if (bufsz + p <= len) return len;

    char* end = &buf[(std::min)(len, bufsz)];
    char* s = end;
    while (s != &buf[len - p - 1])
    {
        s[0] = s[-1];
        --s;
    }

    *s = '.';
    *end = '\0';
    return len;
}
