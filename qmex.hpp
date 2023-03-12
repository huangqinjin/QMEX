//
// Copyright (c) 2018-2023 Huang Qinjin (huangqinjin@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#ifndef QMEX_HPP
#define QMEX_HPP

#if defined(_MSC_VER)
#  define QMEX_EXPORT __declspec(dllexport)
#  define QMEX_IMPORT __declspec(dllimport)
#else
#  define QMEX_EXPORT __attribute__((visibility("default")))
#  define QMEX_IMPORT __attribute__((visibility("default")))
#endif

#if defined(QMEX_EXPORTS)
#  define QMEX_API QMEX_EXPORT
#elif QMEX_SHARED_LIBRARY
#  define QMEX_API QMEX_IMPORT
#else
#  define QMEX_API
#endif

#include <stdexcept>
#include <string>
#include <cstdio>
#include <cstring>

struct lua_State;

///
/// Do not directly call this function, use
///    luaL_requiref(L, "qmex", luaopen_qmex, 1);
///    lua_pop(L, 1);
/// or
///    require "qmex"
///
extern "C" QMEX_API int luaopen_qmex(lua_State* L);
extern "C" QMEX_API int lua_getnuvalue(lua_State* L, int idx);
extern "C" QMEX_API int lua_getnuvalue_hint(lua_State* L, int idx, int hint);

namespace qmex
{
    /// Immutable, non-owning and null-terminated string
    typedef const char* String;

    /// Fixed point number
    struct QMEX_API Number
    {
        enum { precision = 3 };
        typedef int integer;
        integer n;

        Number() noexcept;
        Number(double d) noexcept(false);
        Number(String s) noexcept(false);

        static Number inf() noexcept;
        static Number neginf() noexcept;

        Number& operator=(double d) noexcept(false);
        Number& operator=(String s) noexcept(false);
        operator double() const noexcept;
        operator std::string() const noexcept;
        bool operator==(const Number& other) const noexcept;
        bool operator!=(const Number& other) const noexcept;
        bool operator<=(const Number& other) const noexcept;
        bool operator< (const Number& other) const noexcept;
        bool operator>=(const Number& other) const noexcept;
        bool operator> (const Number& other) const noexcept;
        std::size_t toString(char buf[], std::size_t bufsz) const noexcept;
    };

    enum Type
    {
        NIL,
        NUMBER,
        STRING,
    };

    enum Op
    {
        MH,
        EQ,
        LT,
        LE,
        GT,
        GE,
    };

    QMEX_API const char* toString(Type type) noexcept;
    QMEX_API const char* toString(Op op) noexcept;

    union QMEX_API Value
    {
        Number n;
        String s;

        Value() noexcept : s(nullptr) {}
        Value(String s) noexcept : s(s) {}
        Value(Number n) noexcept : n(n) {}
        Value(double d) noexcept(false) : n(d) {}
        std::string toString(Type type) const noexcept;
    };

    struct QMEX_API KeyValue
    {
        String key;
        Value val;
        union
        {
            Type type; // used by KeyValue
            Op op;     // used by Criteria
        };

        std::string toString() const noexcept;

        KeyValue() noexcept
        {
            this->key = "";
            this->val.s = nullptr;
            this->type = NIL;
        }

        explicit KeyValue(String key) noexcept
        {
            this->key = key;
            this->val.s = nullptr;
            this->type = NIL;
        }

        KeyValue(String key, String val) noexcept
        {
            this->key = key;
            this->val.s = val;
            this->type = STRING;
        }

        KeyValue(String key, Number val) noexcept
        {
            this->key = key;
            this->val.n = val;
            this->type = NUMBER;
        }

        KeyValue(String key, double val) noexcept(false)
        {
            this->key = key;
            this->val.n = val;
            this->type = NUMBER;
        }
    };

    struct CriteriaFormatError : std::invalid_argument
    {
        explicit CriteriaFormatError(std::string msg)
            : std::invalid_argument(msg) {}
    };

    struct ValueTypeError : std::logic_error
    {
        explicit ValueTypeError(std::string msg)
            : std::logic_error(msg) {}
    };

    class QMEX_API Criteria : public KeyValue
    {
    public:
        explicit Criteria(String key) noexcept(false);
        Criteria(String key, String val) noexcept(false);
        void bind(String val) noexcept(false);
        void bind(Number val) noexcept(false);

        static double (max)() noexcept;
        static double (min)() noexcept;

        double distance(const KeyValue& q) noexcept(false);
    };

    struct TableFormatError : std::logic_error
    {
        explicit TableFormatError(std::string msg)
            : std::logic_error(msg) {}
    };

    struct TableDataError : ValueTypeError
    {
        explicit TableDataError(std::string msg)
            : ValueTypeError(msg) {}
    };

    struct TooManyKeys : std::invalid_argument
    {
        explicit TooManyKeys(std::string msg)
            : std::invalid_argument(msg) {}
    };

    struct TooFewKeys : std::invalid_argument
    {
        explicit TooFewKeys(std::string msg)
            : std::invalid_argument(msg) {}
    };

    struct LuaError : std::runtime_error
    {
        explicit LuaError(std::string msg)
            : std::runtime_error(msg) {}
    };

    struct LuaJIT
    {
        virtual void jit(lua_State* L, const char* name) = 0;
    };

    enum QueryOption
    {
        QUERY_EXACTLY = 0,
        QUERY_SUBSET = 1,
        QUERY_SUPERSET = 2,
    };

    class QMEX_API Table
    {
    protected:
        struct Context;
        Context* const ctx;

    public:
        Table() noexcept;
        ~Table() noexcept;
        Table(const Table&) = delete;
        Table& operator=(const Table&) = delete;

        void clear() noexcept;
        int rows() const noexcept;
        int cols() const noexcept;
        int criteria() const noexcept;
        String cell(int i, int j) const noexcept(false);
        void print(FILE* f) const noexcept;
        void parse(char* buf, std::size_t bufsz, lua_State* L = nullptr, LuaJIT* jit = nullptr) noexcept(false);
        int query(const KeyValue kvs[], std::size_t num, unsigned options = QUERY_EXACTLY) noexcept(false);
        void verify(int row, KeyValue kvs[], std::size_t num, unsigned options = QUERY_SUBSET) noexcept(false);
        void retrieve(int row, KeyValue kvs[], std::size_t num, unsigned options = QUERY_SUBSET) noexcept(false);
        bool retrieve(int i, int j, KeyValue& kv) noexcept(false);
        void context(const KeyValue kvs[], std::size_t num) noexcept;
    };
}

#endif
