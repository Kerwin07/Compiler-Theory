// D:\msys64\ucrt64\bin\g++.exe -std=c++17 -O2 -g codegen.cpp codegen_runner.cpp -o codegen_runner.exe

#include "codegen.h"

#include <iostream>

int main() {
    ASTNode* root = loadAST("..\\Parser\\ast.json");
    if (!root) {
        std::cerr << "failed to read ..\\Parser\\ast.json" << std::endl;
        return 1;
    }

    int ret = executeProgram(root);

    delete root;
    return (ret != 0) ? 1 : 0;
}
