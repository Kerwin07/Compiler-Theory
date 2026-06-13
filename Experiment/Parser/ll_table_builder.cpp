// D:\msys64\ucrt64\bin\g++.exe -std=c++17 -O2 -g ll1.cpp ll_table_builder.cpp -o ll_table_builder.exe

#include "ll1.h"

#include <iostream>

int main() {
    Grammar g;
    std::string err;
    if (!loadCFG("grammar_cfg.txt", g, err)) {
        std::cerr << err << std::endl;
        return 1;
    }

    LLTable t;
    bool isLL1 = false;
    if (!buildLL1Table(g, t, isLL1, err)) {
        std::cerr << "failed to build table: " << err << std::endl;
        return 1;
    }

    if (!err.empty()) {
        std::cerr << "warning: " << err << std::endl;
    }

    if (!saveLLTable("ll_table.txt", g, t)) {
        std::cerr << "failed to write ll_table.txt" << std::endl;
        return 1;
    }

    std::cout << "generated LL(1) table -> ll_table.txt" << std::endl;
    std::cout << "isLL1=" << (isLL1 ? "true" : "false") << std::endl;
    return 0;
}
