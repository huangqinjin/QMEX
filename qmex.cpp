
//
// Copyright (c) 2018-2019 Huang Qinjin (huangqinjin@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#include <algorithm>
#include <limits>
#include <vector>
#include <climits>
#include <cassert>

#include "qmex.hpp"
#include "lua.hpp"

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

    struct LuaStack
    {
        lua_State* const L;
        const int s;
        int n;
        LuaStack(lua_State* L, int n) : L(L), s(lua_gettop(L)), n(n) {}
        ~LuaStack() { lua_pop(L, n); assert(lua_gettop(L) == s); }
    };

    struct LuaExpr
    {
        const char* expr;
        std::size_t len;
        explicit LuaExpr(const char* expr) : expr(expr), len(0) {}

        static const char* read(lua_State* L, void* ud, size_t* size) noexcept
        {
            LuaExpr* t = static_cast<LuaExpr*>(ud);
            if (t->expr == nullptr) return nullptr;
            if (t->len == 0)
            {
                t->len = std::strlen(t->expr);
                *size = 6;
                return "return";
            }
            *size = t->len;
            const char* expr = t->expr;
            t->expr = nullptr;
            return expr;
        }
    };

    void LuaValue(lua_State* L, KeyValue& kv) noexcept(false)
    {
        if (kv.type == NUMBER || (kv.type == NIL && lua_type(L, -1) == LUA_TNUMBER))
        {
            int isnum = 0;
            lua_Number n = lua_tonumberx(L, -1, &isnum);
            if (isnum) { kv.val.n = n; kv.type = NUMBER; }
            else goto error;
        }
        else if (lua_isstring(L, -1))
        {
            lua_pushvalue(L, -1); // balance stack
            kv.type = STRING;
            kv.val.s = lua_tostring(L, -1);
            luaL_ref(L, LUA_REGISTRYINDEX); //TODO keep pointer valid
        }
        else error:
        {
            const char* types[] = {
                "NONE",
                "NIL",
                "BOOLEAN",
                "LIGHTUSERDATA",
                "NUMBER",
                "STRING",
                "TABLE",
                "FUNCTION",
                "USERDATA",
                "THREAD",
            };

            int t = lua_type(L, -1) + 1;
            if (t < 0 || t >= sizeof(types) / sizeof(types[0]))
            {
                char buf[200];
                snprintf(buf, sizeof(buf), "unknown lua type %d, requires NUMBER or STRING", t - 1);
                throw LuaError(buf);
            }
            else
            {
                char buf[200];
                snprintf(buf, sizeof(buf), "wrong lua type %s, requires NUMBER or STRING", types[t]);
                throw LuaError(buf);
            }
        }
    }

    void EvalLua(lua_State* L, const char* expr, KeyValue& kv, int id) noexcept(false)
    {
        LuaExpr t(expr);
        LuaStack s(L, 3);

        lua_pushglobaltable(L);
        lua_pushfstring(L, "%s%d", kv.key, id);
        lua_pushvalue(L, -1);
        if (lua_gettable(L, -3) != LUA_TFUNCTION)
        {
            lua_pop(L, 1);
            if (lua_load(L, &LuaExpr::read, &t, kv.key, "t"))
                throw LuaError(lua_tostring(L, -1));
            lua_pushvalue(L, -1);
            lua_insert(L, -3);
            lua_settable(L, -4);
            lua_pushvalue(L, -1);
        }

        if (lua_pcall(L, 0, 1, 0))
            throw LuaError(lua_tostring(L, -1));
        ++s.n;
        lua_geti(L, -1, 1);
        LuaValue(L, kv);
    }

    void CallLua(lua_State* L, const char* expr, KeyValue& kv) noexcept(false)
    {
        LuaStack s(L, 2);

        lua_pushglobaltable(L);
        for (const char* p = expr; ;)
        {
            StringGuard s = std::strchr(p, '.');
            int t = lua_getfield(L, -1, p);
            if (!s) break;
            if (t != LUA_TTABLE)
                throw LuaError(std::string(expr) + " is not TABLE");
            p = s + 1;
            lua_remove(L, -2);
        }

        if (lua_pcall(L, 0, 1, 0))
            throw LuaError(lua_tostring(L, -1));
        LuaValue(L, kv);
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

struct Table::Context
{
    std::vector<String> cells;
    int rows;
    int cols;
    int criteria;
    bool ownL;
    lua_State* L;

    lua_State* lua()
    {
        if (L == nullptr)
        {
            ownL = true;
            L = luaL_newstate();
            luaL_openlibs(L);
        }
        return L;
    }
};

Table::Table() noexcept : ctx(new Context) { ctx->ownL = false; ctx->L = nullptr; }
Table::~Table() noexcept { if (ctx->ownL && ctx->L) lua_close(ctx->L); delete ctx; }

void Table::clear() noexcept
{
    ctx->cells.clear();
    ctx->rows = 0;
    ctx->cols = 0;
    ctx->criteria = 0;
    if (ctx->ownL && ctx->L)
    {
        lua_close(ctx->L);
        ctx->ownL = false;
        ctx->L = nullptr;
    }
}

int Table::rows() const noexcept { return ctx->rows; }
int Table::cols() const noexcept { return ctx->cols; }
int Table::criteria() const noexcept { return ctx->criteria; }

String Table::cell(int i, int j) const noexcept(false)
{
    if (i < 0 || i >= ctx->rows || j < 0 || j >= ctx->cols)
    {
        char buf[200];
        snprintf(buf, sizeof(buf), "index (%d,%d) out of range %dx%d", i, j, ctx->rows, ctx->cols);
        throw std::out_of_range(buf);
    }
    return ctx->cells[i * ctx->cols + j];
}

void Table::print(FILE* f) const noexcept
{
    for (int i = 0; i < ctx->rows; ++i)
    {
        for (int j = 0; j < ctx->cols; ++j)
        {
            if (j == ctx->criteria)
                std::fprintf(f, "%s ", "=");
            std::fprintf(f, "%s ", cell(i, j));
        }
        std::fputc('\n', f);
    }
}

void Table::parse(char* buf, std::size_t bufsz, lua_State* L) noexcept(false)
{
    if (buf == nullptr || bufsz == 0 || buf[bufsz - 1] != '\0')
        throw std::invalid_argument("invalid buffer input for table parse");

    clear();
    ctx->ownL = false;
    ctx->L = L;

    int j = 0, criteria = 0;
    String cell = nullptr;
    for (std::size_t i = 0; i < bufsz; ++i)
    {
        switch (char& c = buf[i])
        {
        case '\0':
        case '\n':
            if (j != 0)
            {
                if (ctx->cols == 0)
                    ctx->cols = j;
                if (criteria == 0 || criteria == j)
                {
                    char str[200];
                    snprintf(str, sizeof(str), "Table has no data at row %d", ctx->rows);
                    throw TableFormatError(str);
                }
                if (ctx->cols != j)
                {
                    char str[200];
                    snprintf(str, sizeof(str), "Table has %d columns but %d at row %d", ctx->cols, j, ctx->rows);
                    throw TableFormatError(str);
                }
                ++ctx->rows;
                j = 0;
                criteria = 0;
            }
        case ' ':
        case '\t':
        //case ',':
        case '=':
            if (c == '=' && criteria == 0)
            {
                criteria = j;
                if (ctx->criteria == 0)
                    ctx->criteria = j;
                if (ctx->criteria != j)
                {
                    char str[200];
                    snprintf(str, sizeof(str), "Table has %d criteria but %d at row %d", ctx->criteria, j, ctx->rows);
                    throw TableFormatError(str);
                }
                else if (j == 0)
                {
                    char str[200];
                    snprintf(str, sizeof(str), "Table has no criteria at row %d", ctx->rows);
                    throw TableFormatError(str);
                }
            }
            if (cell)
            {
                ctx->cells.push_back(cell);
                cell = nullptr;
                c = '\0';
            }
            break;
        default:
            if (!cell) // start a new cell
            {
                cell = &c;
                ++j;
            }
        }
    }

    assert(ctx->cells.size() == ctx->rows * ctx->cols);
    if (ctx->cells.empty()) throw TableFormatError("Table is empty");
}

namespace
{
    struct QueryInfo : Criteria
    {
        int index; // of matched kvs[], -1 not matched yet, -2 no match found.
        int count;
        QueryInfo(Criteria c) : Criteria(c), index(-1), count(0) {}
    };
}

int Table::query(const KeyValue kvs[], std::size_t num, unsigned options) noexcept(false)
{
    int min_i = 0;
    if (ctx->rows <= 1) return min_i;

    std::vector<QueryInfo> info;
    info.reserve(ctx->criteria);
    for (int j = 0; j < ctx->criteria; ++j) try
    {
        info.push_back(Criteria(cell(0, j)));
    }
    catch (CriteriaFormatError& e)
    {
        char buf[200];
        snprintf(buf, sizeof(buf), "Table row:%d, col:%d\n", 0, j + 1);
        throw TableFormatError(std::string(buf) + e.what());
    }

    int matched = 0;
    double min_d = (Criteria::max)();
    for (int i = 1; i < ctx->rows; ++i)
    {
        double sum_d = 0;
        for (int j = 0; j < (int)info.size(); ++j)
        {
            if (info[j].index == -2) continue;
            try
            {
                info[j].bind(cell(i, j));
            }
            catch (std::exception& e)
            {
                char buf[200];
                snprintf(buf, sizeof(buf), "Table row:%d, col:%d\n", i, j + 1);
                throw TableFormatError(std::string(buf) + e.what());
            }
            if (info[j].index >= 0)
            {
                double d = info[j].distance(kvs[info[j].index]);
                if (d < 0) continue; // never happen
                sum_d += d;
                if (sum_d >= min_d) goto next;
                continue;
            }
            for (std::size_t k = 0; k < num; ++k)
            {
                double d = info[j].distance(kvs[k]);
                if (d < 0) continue; // key mismatch
                info[j].index = (int)k;
                sum_d += d;
                ++matched;
                if (sum_d >= min_d) goto next;
                break;
            }
            if (info[j].index == -1)
            {
                info[j].index = -2; // no match found
                if (!(options & QUERY_SUBSET))
                {
                    std::string msg = "Query requires Criteria [" + std::string(info[j].key);
                    msg.resize(msg.size() - 2);
                    msg[msg.size() - 1] = ']';
                    throw TooFewKeys(msg);
                }
            }
        }
        if (!(options & QUERY_SUPERSET))
        {
            options |= QUERY_SUPERSET;

            for (std::size_t j = 0; j < info.size(); ++j)
            {
                if (info[j].index < 0 || info[j].index >= (int)info.size())
                    continue;
                info[info[j].index].count += 1;
            }

            for (std::size_t j = 0; j < (std::min)(info.size(), num); ++j)
            {
                if (info[j].count == 0)
                    throw TooManyKeys('[' + std::string(kvs[j].key) + "] not Criteria");
            }
        }
        if (matched == 0)
            break;
        if (sum_d < min_d)
        {
            min_d = sum_d;
            min_i = i;
            if (min_d == 0) // current row is the best match already
                break;
        }
    next:
        ;
    }
    return min_i;
}

void Table::verify(int row, KeyValue kvs[], std::size_t num, unsigned options) noexcept(false)
{
    bool lua = false;
    for (std::size_t k = 0; k < num; ++k)
    {
        if ((options & QUERY_SUPERSET) && kvs[k].type == NIL) continue;
        bool matched = false;
        for (int j = ctx->criteria; j < ctx->cols; ++j)
        {
            if (std::strcmp(cell(0, j), kvs[k].key)) continue;
            matched = true;
            if (kvs[k].type == NIL) break;

            if (!lua)
            {
                String val = cell(row, j);
                lua = val[0] == '{' || val[0] == '[';
                if (lua)
                {
                    context(kvs, num);
                    context(nullptr, 0);
                }
            }

            KeyValue kv = kvs[k];
            retrieve(row, j, kv);
            if (kv.type == NUMBER)
            {
                if (kv.val.n != kvs[k].val.n)
                {
                    char buf[200];
                    snprintf(buf, sizeof(buf), "Table row:%d, col:%d[%s], NUMBER %s != %s",
                        row, j + 1, kv.key, std::string(kv.val.n).c_str(), std::string(kvs[k].val.n).c_str());
                    throw TableDataError(std::string(buf));
                }
            }
            else if (!kvs[k].val.s || !*kvs[k].val.s)
            {
                char buf[200];
                snprintf(buf, sizeof(buf), "Table row:%d, col:%d[%s], STRING `%s` != NIL",
                    row, j + 1, kv.key, kv.val.s);
                throw TableDataError(std::string(buf));
            }
            else if (std::strcmp(kv.val.s, kvs[k].val.s))
            {
                char buf[200];
                snprintf(buf, sizeof(buf), "Table row:%d, col:%d[%s], STRING `%s` != `%s`",
                    row, j + 1, kv.key, kv.val.s, kvs[k].val.s);
                throw TableDataError(std::string(buf));
            }
            break;
        }
        if (!matched && !(options & QUERY_SUPERSET))
            throw TooManyKeys("Table no data column [" + std::string(kvs[k].key) + ']');
    }
}

void Table::retrieve(int row, KeyValue kvs[], std::size_t num, unsigned options) noexcept(false)
{
    bool lua = false;
    for (std::size_t k = 0; k < num; ++k)
    {
        bool matched = false;
        for (int j = ctx->criteria; j < ctx->cols; ++j)
        {
            if (std::strcmp(cell(0, j), kvs[k].key)) continue;

            if (!lua)
            {
                String val = cell(row, j);
                lua = val[0] == '{' || val[0] == '[';
                if (lua)
                {
                    context(kvs, num);
                    context(nullptr, 0);
                }
            }

            retrieve(row, j, kvs[k]);
            matched = true;
            break;
        }
        if (!matched && !(options & QUERY_SUPERSET))
            throw TooManyKeys("Retrieve ["+ std::string(kvs[k].key) + "] failed");
    }
}

bool Table::retrieve(int i, int j, KeyValue& kv) noexcept(false) try
{
    String val = cell(i, j);

    bool lua = val[0] == '{' || val[0] == '[';
    if (lua)
    {
        {
            LuaStack s(ctx->lua(), 1);
            if (lua_getglobal(ctx->lua(), kv.key) != LUA_TNIL)
            {
                LuaValue(ctx->lua(), kv);
                return lua;
            }
        }
        for (int k = j - 1; k >= ctx->criteria; --k)
        {
            KeyValue ks(cell(0, k));
            if (retrieve(i, k, ks)) break;
            else context(&ks, 1);
        }
    }

    if (val[0] == '{')
    {
        EvalLua(ctx->lua(), val, kv, i);
    }
    else if (val[0] == '[')
    {
        StringGuard _(val + std::strlen(val) - 1);
        CallLua(ctx->lua(), val + 1, kv);
    }
    else if (kv.type == NUMBER)
    {
        kv.val.n = val;
    }
    else
    {
        kv.val.s = val;
        kv.type = STRING;
    }

    if (lua) context(&kv, 1);
    return lua;
}
catch (std::exception& e)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "Table row:%d, col:%d[%s]\n", i, j + 1, kv.key);
    throw TableDataError(std::string(buf) + e.what());
}

void Table::context(const KeyValue kvs[], std::size_t num) noexcept
{
    if (!kvs || !num)
    {
        for (int j = ctx->criteria; j < ctx->cols; ++j)
        {
            KeyValue kv(cell(0, j));
            context(&kv, 1);
        }
        return;
    }
    for (std::size_t i = 0; i < num; ++i)
    {
        if (kvs[i].type == NUMBER)
            lua_pushnumber(ctx->lua(), kvs[i].val.n);
        else if (kvs[i].type == STRING)
            lua_pushstring(ctx->lua(), kvs[i].val.s);
        else
            lua_pushnil(ctx->lua());
        lua_setglobal(ctx->lua(), kvs[i].key);
    }
}
