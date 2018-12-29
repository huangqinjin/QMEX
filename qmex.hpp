//
// Copyright (c) 2018-2019 Huang Qinjin (huangqinjin@gmail.com)
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

#include <string>
#include <cstring>

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

    class QMEX_API Criteria : public KeyValue
    {
    public:
        explicit Criteria(String key) noexcept(false);
        Criteria(String key, String val) noexcept(false);
        void bind(String val) noexcept(false);
        void bind(Number val) noexcept(false);

        static double (max)() noexcept;
        static double (min)() noexcept;

        enum Error
        {
            KEY_MISMATCH = -1,
            VALUE_NOT_STRING = -2,
            VALUE_NOT_NUMBER = -3,
        };

        double distance(const KeyValue& q) noexcept;
    };
}

#endif
