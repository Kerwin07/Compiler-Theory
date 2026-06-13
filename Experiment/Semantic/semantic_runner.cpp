// D:\msys64\ucrt64\bin\g++.exe -std=c++17 -O2 -g semantic.cpp semantic_runner.cpp -o semantic_runner.exe

#include "semantic.h"

#include <fstream>
#include <iostream>

int main() {
    ASTNode* root = loadAST("..\\Parser\\ast.json");
    if (!root) {
        std::cerr << "failed to read ..\\Parser\\ast.json" << std::endl;
        return 1;
    }

    SemanticResult result;
    if (!analyzeSemantics(root, result)) {
        std::cerr << "semantic analysis failed" << std::endl;
        delete root;
        return 1;
    }

    std::cout << "===== Semantic Analysis Report =====" << std::endl;

    if (!result.infos.empty()) {
        std::cout << "\n-- Info (" << result.infos.size() << ") --" << std::endl;
        for (auto& s : result.infos) std::cout << "  " << s << std::endl;
    }

    if (!result.warnings.empty()) {
        std::cout << "\n-- Warnings (" << result.warnings.size() << ") --" << std::endl;
        for (auto& s : result.warnings) std::cout << "  " << s << std::endl;
    }

    if (!result.errors.empty()) {
        std::cout << "\n-- Errors (" << result.errors.size() << ") --" << std::endl;
        for (auto& s : result.errors) std::cout << "  " << s << std::endl;
    }

    std::cout << "\n-- Symbol Table --" << std::endl;
    for (auto& kv : result.globalScope.symbols) {
        auto& e = kv.second;
        std::cout << "  " << e.name
                  << "  type=" << typeName(e.type)
                  << "  value=" << e.value
                  << "  defined=" << (e.defined ? "Y" : "N")
                  << "  used=" << (e.used ? "Y" : "N");
        if (e.defined) std::cout << "  definedAt=#" << e.definedLine;
        if (e.used) std::cout << "  usedAt=#" << e.usedLine;
        std::cout << std::endl;
    }

    std::ofstream report("semantic_report.txt");
    if (report.is_open()) {
        report << "SEMANTIC ANALYSIS REPORT\n";
        report << "========================\n\n";
        report << "== Info ==\n";
        for (auto& s : result.infos)    report << "[INFO]    " << s << "\n";
        report << "\n== Warnings ==\n";
        for (auto& s : result.warnings) report << "[WARNING] " << s << "\n";
        report << "\n== Errors ==\n";
        for (auto& s : result.errors)   report << "[ERROR]   " << s << "\n";
        report << "\nSYMBOL TABLE\n";
        report << "============\n";
        for (auto& kv : result.globalScope.symbols) {
            auto& e = kv.second;
            report << e.name
                   << "  type=" << typeName(e.type)
                   << "  value=" << e.value
                   << "  defined=" << (e.defined ? "true " : "false")
                   << "  used=" << (e.used ? "true " : "false")
                   << "  definedAt=#" << e.definedLine
                   << "  usedAt=#" << e.usedLine << "\n";
        }
        report.close();
        std::cout << "\nreport written to semantic_report.txt" << std::endl;
    }

    delete root;
    return result.hasErrors() ? 1 : 0;
}
