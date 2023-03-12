//
// Copyright (c) 2018-2023 Huang Qinjin (huangqinjin@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
#include "qmex.hpp"
#include "lua.hpp"

#include <typeinfo>
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;
using namespace qmex;

int main(int argc, char* argv[]) try
{
    if (argc < 2)
    {
        printf("Usage: %s </path/to/file>\n", argv[0]);
        return 0;
    }

    const char* const ext = strrchr(argv[1], '.');

    if (ext && strcmp(ext, ".lua") == 0)
    {
        lua_State* L = luaL_newstate();
        luaL_openlibs(L);

        luaL_requiref(L, "qmex", luaopen_qmex, 1);
        lua_pop(L, 1);

        int r = luaL_loadfile(L, argv[1]);
        if (r == LUA_OK)
        {
            for (int i = 2; i < argc; ++i)
                lua_pushstring(L, argv[i]);
            r = lua_pcall(L, argc - 2, LUA_MULTRET, 0);
        }
        if (r != LUA_OK) fprintf(stderr, "ERROR: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return r;
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

    int num_queries = 0;
    int first_error_query_id = 0;
    string first_error_query;

    std::vector<KeyValue> kvs;
    string line;
    while (getline(cin, line))
    {
        if (first_error_query_id == 0)
            first_error_query = line;

        kvs.clear();
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            i = line.find_first_not_of(" \t", i);
            if (i == std::string::npos) break;
            kvs.push_back(KeyValue(&line[i]));
            i = line.find_first_of(" \t", i + 1);
            if (i == std::string::npos) break;
            line[i] = '\0';
        }
        for (std::size_t i = 0; i < kvs.size(); ++i)
        {
            if (const char* p = std::strchr(kvs[i].key, ':'))
            {
                kvs[i] = KeyValue(kvs[i].key, p + 1);
                const_cast<char&>(*p) = '\0';
                try
                {
                    kvs[i].val.n = kvs[i].val.s;
                    kvs[i].type = NUMBER;
                }
                catch (std::exception&)
                {

                }
            }
        }
        if (!kvs.empty()) try
        {
            ++num_queries;
            int row = table.query(&kvs[0], kvs.size(), QUERY_SUBSET | QUERY_SUPERSET);
            if (row == 0 && first_error_query_id == 0)
                first_error_query_id = num_queries;
            if (row > 0)
            {
                table.verify(row, &kvs[0], kvs.size(), QUERY_SUPERSET);
                table.retrieve(row, &kvs[0], kvs.size(), QUERY_SUPERSET);
                printf("[%d] row:%d", num_queries, row);
                for (std::size_t i = 0; i < kvs.size(); ++i)
                    printf(" %s", kvs[i].toString().c_str());
                printf("\n");
            }
            else
            {
                printf("[%d] no matched row\n", num_queries);
            }
        }
        catch (exception& e)
        {
            if (first_error_query_id == 0)
                first_error_query_id = num_queries;
            printf("[%d] %s: %s\n", num_queries, typeid(e).name(), e.what());
        }
    }

    if (first_error_query_id)
        printf("Error[%d]: %s\n", first_error_query_id, first_error_query.c_str());
    return first_error_query_id;
}
catch (exception& e)
{
    printf("%s: %s\n", typeid(e).name(), e.what());
    return 65535;
}
