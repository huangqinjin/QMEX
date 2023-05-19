
//
// Copyright (c) 2018-2023 Huang Qinjin (huangqinjin@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#include <algorithm>
#include <limits>
#include <vector>
#include <new>
#include <cmath>
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
       "AE",
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
                *size = 7;
                return "return ";
            }
            *size = t->len;
            const char* expr = t->expr;
            t->expr = nullptr;
            return expr;
        }
    };

    void LuaValue(lua_State* L, int cache, KeyValue& kv) noexcept(false)
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
            kv.type = STRING;
            kv.val.s = lua_tostring(L, -1);
            lua_pushvalue(L, -1);
            lua_seti(L, cache, (lua_Integer)(intptr_t)kv.val.s); //TODO keep pointer valid
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

    void EvalLua(lua_State* L, int local, const char* expr, KeyValue& kv, int id) noexcept(false)
    {
        LuaExpr t(expr);
        LuaStack s(L, 2);

        lua_pushfstring(L, "%s:%d", kv.key, id);
        lua_pushvalue(L, -1);
        if (lua_gettable(L, local) != LUA_TFUNCTION)
        {
            lua_pop(L, 1);
            if (lua_load(L, &LuaExpr::read, &t, kv.key, "t"))
                throw LuaError(lua_tostring(L, -1));
            lua_pushvalue(L, -1);
            lua_insert(L, -3);
            lua_settable(L, local);
            lua_pushvalue(L, -1);
        }

        if (lua_pcall(L, 0, 1, 0))
            throw LuaError(lua_tostring(L, -1));
        ++s.n;
        lua_geti(L, -1, 1);
        LuaValue(L, local, kv);
    }

    void CallLua(lua_State* L, int local, const char* expr, KeyValue& kv, LuaJIT* jit) noexcept(false) try
    {
        LuaStack s(L, 1);

        if (lua_getfield(L, local, expr) == LUA_TNIL)
        {
            lua_pop(L, 1);

            LuaExpr t(expr);
            if (lua_load(L, &LuaExpr::read, &t, kv.key, "t") || lua_pcall(L, 0, 1, 0))
                throw LuaError(lua_tostring(L, -1));

            lua_pushvalue(L, -1);
            lua_setfield(L, local, expr);
        }

        if (lua_pcall(L, 0, 1, 0))
            throw LuaError(lua_tostring(L, -1));
        jit = nullptr;
        LuaValue(L, local, kv);
    }
    catch (LuaError&)
    {
        if (jit)
        {
            jit->jit(L, expr);
            CallLua(L, local, expr, kv, nullptr);
        }
        else
        {
            throw;
        }
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
    case AE: return std::fabs((double)qn.n - (double)val.n.n);
    }
    return 0;
}

struct Table::Context
{
    std::vector<String> cells;
    int rows;
    int cols;
    int criteria;
    int cache;
    bool ownL;
    bool init;
    lua_State* L;
    LuaJIT* jit;

    Context() noexcept : cache(0), ownL(false), init(false) {}
    ~Context() noexcept { clear(); }

    void clear() noexcept
    {
        cells.clear();
        rows = 0;
        cols = 0;
        criteria = 0;

        if (cache > 0)
        {
            lua_pushnil(L);
            lua_setiuservalue(L, 1, cache);
        }
        else if (init && L && lua_getglobal(L, "QMEX_G") == LUA_TTABLE)
        {
            lua_pushnil(L);
            lua_seti(L, -2, (lua_Integer)(intptr_t)this);
            lua_pop(L, 1);
        }

        if (ownL && L)
        {
            lua_close(L);
        }

        ownL = false;
        init = false;
        L = nullptr;
        jit = nullptr;
    }

    void g() noexcept
    {
        if (lua_getglobal(L, "QMEX_G") != LUA_TTABLE)
        {
            lua_pop(L, 1);
            lua_createtable(L, 0, 2);
            lua_pushvalue(L, -1);
            lua_setglobal(L, "QMEX_G");
        }
    }

    int local() noexcept
    {
        if (cache > 0)
        {
            lua_getiuservalue(L, 1, cache);
        }
        else
        {
            g();
            lua_geti(L, -1, (lua_Integer)(intptr_t)this);
            lua_remove(L, -2);
        }
        return lua_gettop(L);
    }

    lua_State* lua() noexcept
    {
        if (L == nullptr)
        {
            ownL = true;
            L = luaL_newstate();
            luaL_openlibs(L);
        }
        if (!init)
        {
            if (cache > 0)
            {
                lua_newtable(L);
                lua_setiuservalue(L, 1, cache);
            }
            else
            {
                LuaStack s(L, 1);
                g();
                lua_newtable(L);
                lua_seti(L, -2, (lua_Integer)(intptr_t)this);
            }
            init = true;
        }
        return L;
    }
};

Table::Table() noexcept : ctx(new Context) {}
Table::~Table() noexcept { delete ctx; }
void Table::clear() noexcept { ctx->clear(); }
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

void Table::parse(char* buf, std::size_t bufsz, lua_State* L, LuaJIT* jit) noexcept(false)
{
    if (buf == nullptr || bufsz == 0 || buf[bufsz - 1] != '\0')
        throw std::invalid_argument("invalid buffer input for table parse");

    clear();
    ctx->L = L;
    ctx->jit = jit;

    int j = 0, criteria = 0;
    String cell = nullptr;
    for (std::size_t i = 0; i < bufsz; ++i)
    {
        switch (char& c = buf[i])
        {
        case '\0':
        case '\r':
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
                int cache = ctx->local();
                lua_pushvalue(ctx->lua(), -2);
                s.n += 2;
                LuaValue(ctx->lua(), cache, kv);
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
        LuaStack s(ctx->lua(), 1);
        EvalLua(ctx->lua(), ctx->local(), val, kv, i);
    }
    else if (val[0] == '[')
    {
        StringGuard _(val + std::strlen(val) - 1);
        LuaStack s(ctx->lua(), 1);
        CallLua(ctx->lua(), ctx->local(), val + 1, kv, ctx->jit);
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


extern "C" int lua_getnuvalue_hint(lua_State* L, int idx, int b)
{
    int a = 0;
    while (b - a > 1)
    {
        int n = (a + b) / 2;
        if (lua_getiuservalue(L, idx, n) == LUA_TNONE)
            b = n;
        else
            a = n;
        lua_pop(L, 1);
    }
    return a;
}

extern "C" int lua_getnuvalue(lua_State* L, int idx)
{
    return lua_getnuvalue_hint(L, idx, (std::numeric_limits<unsigned short>::max)() + 1);
}


namespace
{
    enum
    {
        UV_MIN,
        UV_BUF,
        UV_JIT,
        UV_CACHE,
        UV_MAX,
    };

    struct LuaTable : Table, LuaJIT
    {
        LuaTable(lua_State* L, int cache)
        {
            ctx->L = L;
            ctx->cache = cache;
        }

        void jit(lua_State* L, const char* name) override
        {
            LuaStack s(L, 1);
            lua_getiuservalue(L, 1, UV_JIT);
            lua_pushstring(L, name);
            if (lua_pcall(L, 1, 1, 0))
                throw LuaError(lua_tostring(L, -1));
        }
    };

    int chkidx(lua_State* L, int idx, bool error, int min = 1)
    {
        int isnum = 0;
        lua_Integer i = lua_tointegerx(L, idx, &isnum);
        if (isnum)
        {
            int max = (std::numeric_limits<unsigned short>::max)() - UV_MAX + 1;
            if (i < min || i > max)
                luaL_error(L, "index [%I] out of range [%d, %d]", i, min, max);
            return (int)i;
        }
        if (error)
        {
            luaL_error(L, "index must be integer");
        }
        return 0;
    }

    int index(lua_State* L)
    {
        int i = chkidx(L, 2, false);
        if (i == 0)
        {
            lua_getmetatable(L, 1);
            lua_pushvalue(L, 2);
            lua_rawget(L, -2);
        }
        else if (lua_getiuservalue(L, 1, i + UV_MAX - 1) == LUA_TNONE)
        {
            // Do not emit error as ipairs will stop when it gets nil value.
        }
        return 1;
    }

    int newindex(lua_State* L)
    {
        int i = chkidx(L, 2, true);
        lua_pushvalue(L, 3);
        if (lua_setiuservalue(L, 1, i + UV_MAX - 1) == 0)
        {
            luaL_error(L, "index [%I] out of range [%d, %d]", (lua_Integer)i, 1,
                       lua_getnuvalue_hint(L, 1, i + UV_MAX - 1) - UV_MAX + 1);
        }
        return 0;
    }

    int len(lua_State* L)
    {
        lua_pushinteger(L, lua_getnuvalue(L, 1) - UV_MAX + 1);
        return 1;
    }

    int newtable(lua_State* L)
    {
        int n = lua_gettop(L) >= 1 ? chkidx(L, 1, true, 0) : 0;
        new (lua_newuserdatauv(L, sizeof(LuaTable), n + UV_MAX - UV_MIN - 1)) LuaTable(L, UV_CACHE);
        luaL_setmetatable(L, "qmex::Table");
        return 1;
    }

    LuaTable* totable(lua_State* L)
    {
        return (LuaTable*)luaL_checkudata(L, 1, "qmex::Table");
    }

    bool isclosed(LuaTable* t)
    {
        for (std::size_t i = 0; i < sizeof(LuaTable); ++i)
            if (reinterpret_cast<unsigned char*>(t)[i])
                return false;
        return true;
    }

    LuaTable* checktable(lua_State* L)
    {
        LuaTable* t = totable(L);
        if (isclosed(t))
            luaL_error(L, "attempt to use a closed table");
        return t;
    }

    int deltable(lua_State* L)
    {
        LuaTable* t = totable(L);
        if (!isclosed(t))
        {
            t->~LuaTable();
            std::memset((void*)t, 0, sizeof(LuaTable));
            for (int i = UV_MIN + 1; i < UV_MAX; ++i)
            {
                lua_pushnil(L);
                lua_setiuservalue(L, 1, i);
            }
        }
        return 0;
    }

    std::vector<KeyValue> tokvs(lua_State* L, int idx)
    {
        std::vector<KeyValue> kvs;
        kvs.reserve(lua_rawlen(L, idx));
        lua_pushnil(L);
        while (lua_next(L, idx))
        {
            switch (lua_type(L, -2))
            {
            case LUA_TSTRING:
                break;
            case LUA_TNUMBER:
                if (lua_type(L, -1) == LUA_TSTRING)
                    kvs.push_back(KeyValue(lua_tostring(L, -1)));
                // [[fallthrough]];
            default:
                lua_pop(L, 1);
                continue;
            }

            switch (lua_type(L, -1))
            {
            case LUA_TNUMBER:
                kvs.push_back(KeyValue(lua_tostring(L, -2), lua_tonumber(L, -1)));
                break;
            case LUA_TSTRING:
                kvs.push_back(KeyValue(lua_tostring(L, -2), lua_tostring(L, -1)));
                break;
            case LUA_TNIL:
                kvs.push_back(KeyValue(lua_tostring(L, -2)));
                break;
            }
            lua_pop(L, 1);
        }
        return kvs;
    }

    int parse(lua_State* L) try
    {
        LuaTable* t = checktable(L);
        std::size_t len  = 0;
        const char* data = luaL_checklstring(L, 2, &len);

        const bool jit = lua_isnoneornil(L, 3) == 0;
        lua_pushvalue(L, 3);
        lua_setiuservalue(L, 1, UV_JIT);

        void* buf = lua_newuserdatauv(L, len + 1, 0);
        std::memcpy(buf, data, len + 1);
        lua_setiuservalue(L, 1, UV_BUF);
        t->parse((char*)buf, len + 1, L, jit ? t : nullptr);
        return 0;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "%s", e.what());
    }

    int query(lua_State* L) try
    {
        LuaTable* t = checktable(L);
        luaL_checktype(L, 2, LUA_TTABLE);
        unsigned options = (unsigned)luaL_optinteger(L, 3, QUERY_EXACTLY);
        std::vector<KeyValue> kvs = tokvs(L, 2);
        int row = t->query(kvs.empty() ? nullptr : &kvs[0], kvs.size(), options);
        lua_pushinteger(L, row);
        return 1;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "%s", e.what());
    }

    int verify(lua_State* L) try
    {
        LuaTable* t = checktable(L);
        int row = (int)luaL_checkinteger(L, 2);
        luaL_checktype(L, 3, LUA_TTABLE);
        unsigned options = (unsigned)luaL_optinteger(L, 4, QUERY_SUBSET);
        std::vector<KeyValue> kvs = tokvs(L, 3);
        t->verify(row, kvs.empty() ? nullptr : &kvs[0], kvs.size(), options);
        return 0;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "%s", e.what());
    }

    int retrieve(lua_State* L) try
    {
        LuaTable* t = checktable(L);
        int row = (int)luaL_checkinteger(L, 2);
        luaL_checktype(L, 3, LUA_TTABLE);
        unsigned options = (unsigned)luaL_optinteger(L, 4, QUERY_SUBSET);
        std::vector<KeyValue> kvs = tokvs(L, 3);
        t->retrieve(row, kvs.empty() ? nullptr : &kvs[0], kvs.size(), options);

        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            if (lua_type(L, -1) == LUA_TSTRING && lua_type(L, -2) == LUA_TNUMBER)
            {
                lua_pop(L, 1);
                lua_pushvalue(L, -1);
                lua_pushnil(L);
                lua_settable(L, 3);
            }
            else
            {
                lua_pop(L, 1);
            }
        }

        for (std::size_t i = 0; i < kvs.size(); ++i)
        {
            if (kvs[i].type == NUMBER)
                lua_pushnumber(L, kvs[i].val.n);
            else if (kvs[i].type == STRING)
                lua_pushstring(L, kvs[i].val.s);
            else
                lua_pushnil(L);
            lua_setfield(L, 3, kvs[i].key);
        }
        return 0;
    }
    catch (std::exception& e)
    {
        return luaL_error(L, "%s", e.what());
    }
}

extern "C" int luaopen_qmex(lua_State* L)
{
    if (luaL_newmetatable(L, "qmex::Table"))
    {
        const luaL_Reg metameth[] = {
            {"__index", index},
            {"__newindex", newindex},
            {"__len", len},
            {"__gc", deltable},
            {"__close", deltable},
            {"parse", parse},
            {"query", query},
            {"verify", verify},
            {"retrieve", retrieve},
            {nullptr, nullptr}
        };
        luaL_setfuncs(L, metameth, 0);
    }
    lua_pop(L, 1);

    lua_createtable(L, 0, 1 + TypeKinds + OpKinds + 3);

    lua_pushliteral(L, "Table");
    lua_pushcfunction(L, newtable);
    lua_settable(L, -3);

    for (int i = 0; i < TypeKinds; ++i)
    {
        lua_pushstring(L, TypeName[i]);
        lua_pushinteger(L, i);
        lua_settable(L, -3);
    }

    for (int i = 0; i < OpKinds; ++i)
    {
        lua_pushstring(L, OpName[i]);
        lua_pushinteger(L, i);
        lua_settable(L, -3);
    }

    const char* const options[] = {
        "QUERY_EXACTLY",
        "QUERY_SUBSET",
        "QUERY_SUPERSET",
    };

    for (int i = 0; i < sizeof(options) / sizeof(options[0]); ++i)
    {
        lua_pushstring(L, options[i]);
        lua_pushinteger(L, i);
        lua_settable(L, -3);
    }

    return 1;
}
