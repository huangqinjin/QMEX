//
// Copyright (c) 2018-2019 Huang Qinjin (huangqinjin@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#include "qmex.hpp"

#include <typeinfo>
#include <iostream>
#include <fstream>

using namespace std;
using namespace qmex;

int main(int argc, char* argv[]) try
{
    if (argc < 2)
    {
        printf("Usage: %s </path/to/file>\n", argv[0]);
        return 0;
    }

    string content;
    {
        ifstream in(argv[1]);
        if (!in.is_open())
        {
            printf("Failed to open file [%s].\n", argv[1]);
            return 65534;
        }
        in.seekg(0, ios_base::end);
        content.resize(in.tellg());
        in.seekg(0);
        in.read(&content[0], (std::streamsize)content.size());
    }

    Table table;
    table.parse(&content[0], content.size() + 1);
    // table.print(stdout);

    string line;
    while (getline(cin, line))
    {

    }
}
catch (exception& e)
{
    printf("%s: %s\n", typeid(e).name(), e.what());
    return 65535;
}
