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
}

#endif
