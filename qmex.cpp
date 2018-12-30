
//
// Copyright (c) 2018-2019 Huang Qinjin (huangqinjin@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#include <algorithm>
#include <limits>
#include <cstdio>
#include <climits>

#include "qmex.hpp"

using namespace qmex;

#ifdef _WIN32
#pragma comment(lib, "Shlwapi.lib")
extern "C" int __stdcall PathMatchSpecA(const char* pszFile, const char* pszSpec);
#else
#include <fnmatch.h>
#endif

namespace
{
    bool MatchString(const char* pattern, const char* s) noexcept
    {
#ifdef _WIN32
        return PathMatchSpecA(s, pattern) != 0;
#else
        return fnmatch(pattern, s, FNM_NOESCAPE | FNM_CASEFOLD) == 0;
#endif
    }

    int factor(int precision = Number::precision) noexcept
    {
        int f = 1;
        for (int i = 0; i < precision; ++i)
            f *= 10;
        return f;
    }

    const char* const TypeName[] = {
       "NIL",
       "NUMBER",
       "STRING",
    };

    const char* const OpName[] = {
       "MH",
       "EQ",
       "LT",
       "LE",
       "GT",
       "GE",
    };

    const int TypeKinds = sizeof(TypeName) / sizeof(TypeName[0]);
    const int OpKinds = sizeof(OpName) / sizeof(OpName[0]);

    struct StringGuard
    {
        char* const q;
        char const b;
        operator char*() const { return q; }
        ~StringGuard() { if (q) *q = b; }
        StringGuard(const char* p, char c = '\0')
            : q(const_cast<char*>(p)), b(p ? *p : '\0')
        {
            if (q) *q = c;
        }
    };
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
    if (d != d) throw std::invalid_argument("NaN not NUMBER");
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
    if (!s || !*s) throw std::invalid_argument("NIL not NUMBER");

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

    errno = 0;
    char* end;
    long l, m = 0;

    if (*s != '-' && (*s < '0' || *s > '9'))
        goto error;

    l = std::strtol(s, &end, 0);
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
    throw std::invalid_argument('`' + std::string(s) + "` not NUMBER");
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

const char* qmex::toString(Type type) noexcept
{
    int ordinal = (int)type;
    if (ordinal < 0 || ordinal >= TypeKinds)
        return nullptr;
    return TypeName[ordinal];
}

const char* qmex::toString(Op op) noexcept
{
    int ordinal = (int)op;
    if (ordinal < 0 || ordinal >= OpKinds)
        return nullptr;
    return OpName[ordinal];
}

std::string Value::toString(Type type) const noexcept
{
    if (type == STRING) return s ? s : "";
    if (type == NUMBER) return n;
    return "";
}

std::string KeyValue::toString() const noexcept
{
    if (type == NIL || (type == STRING && !val.s)) return key;

    std::size_t keylen = std::strlen(key), vallen;
    if (type == STRING) vallen = std::strlen(val.s);
    else vallen = val.n.toString(nullptr, 0);

    std::string s(keylen + 1 + vallen, '\0');
    std::strcpy(&s[0], key);
    s[keylen] = ':';
    if (type == STRING) std::strcpy(&s[keylen + 1], val.s);
    else val.n.toString(&s[keylen + 1], vallen + 1);
    return s;
}

Criteria::Criteria(String key) noexcept(false)
{
    if (!key || !*key) throw CriteriaFormatError("NIL invalid Criteria");

    int ordinal = 0;
    std::size_t len = std::strlen(key);
    if (len < 4) goto error;
    while (ordinal < OpKinds)
    {
        if (std::strcmp(&key[len - 2], OpName[ordinal]))
            ++ordinal;
        else
            break;
    }
    if (ordinal >= OpKinds) goto error;
    this->key = key;
    this->op = (Op)ordinal;
    if (ordinal == MH) this->val.s = "";

    return;
error:
    throw CriteriaFormatError('`' + std::string(key) + "` invalid Criteria format");
}

Criteria::Criteria(String key, String val) noexcept(false)
{
    *this = Criteria(key);
    bind(val);
}

void Criteria::bind(String val) noexcept(false)
{
    if (op == MH)
    {
        if (!val || !*val) throw ValueTypeError("Criteria [" + std::string(key) + "] requires non-NIL");
        this->val.s = val;
    }
    else try
    {
        this->val.n = val;
    }
    catch (std::exception& e)
    {
        throw ValueTypeError("Criteria [" + std::string(key) + "] requires NUMBER\n" + e.what());
    }
}

void Criteria::bind(Number val) noexcept(false)
{
    if (op == MH)
        throw ValueTypeError("Criteria [" + std::string(key) + "] requires STRING");
    this->val.n = val;
}

double (Criteria::max)() noexcept
{
    return std::numeric_limits<double>::infinity();
}

double (Criteria::min)() noexcept
{
    return 0;
}

double Criteria::distance(const KeyValue& q) noexcept(false)
{
    if (const char* s = q.key)
    {
        const char* p = key;
        while (*p && *s && *p == *s) ++p, ++s;
        if (p[0] == '\0' || p[1] == '\0' || p[2] == '\0' || p[3] != '\0')
            return -1; // key mismatch
    }
    if (q.key == nullptr) return -1; // key mismatch

    Number qn;
    String qs;
    Criteria t(*this);
    if (q.type == NUMBER) t.bind(q.val.n);
    else if (q.type == STRING) t.bind(q.val.s);
    else t.bind((String)nullptr);
    if (op == MH) qs = t.val.s;
    else qn = t.val.n;

    switch (op)
    {
    case MH:
        for (String p = val.s; ;)
        {
            if (StringGuard s = std::strchr(p, '|'))
            {
                if (MatchString(p, qs)) return 0;
                p = s + 1;
            }
            else
            {
                return MatchString(p, qs) ? 0 : (max)();
            }
        }
    case EQ: return qn.n == val.n.n ? 0 : (max)();
    case LT: return qn.n <  val.n.n ? (double)val.n.n - (double)qn.n : (max)();
    case LE: return qn.n <= val.n.n ? (double)val.n.n - (double)qn.n : (max)();
    case GT: return qn.n >  val.n.n ? (double)qn.n - (double)val.n.n : (max)();
    case GE: return qn.n >= val.n.n ? (double)qn.n - (double)val.n.n : (max)();
    }
    return 0;
}
