// D:\msys64\ucrt64\bin\g++.exe -std=c++17 -O2 -g automata.cpp dfa_minimize.cpp dfa_minimizer.cpp -o dfa_minimizer.exe

#include "automata.h"
#include "dfa_minimize.h"

#include <iostream>
#include <set>

static std::set<char> inferAlphabetFromDFA(const DFA& dfa) {
    std::set<char> alpha;
    for (const auto& kv : dfa.trans) {
        alpha.insert(kv.first.second);
    }
    return alpha;
}

int main() {
    DFA dfa;
    if (!loadMindfa("dfa", dfa)) {
        std::cerr << "failed to read dfa" << std::endl;
        return 1;
    }

    auto alphabet = inferAlphabetFromDFA(dfa);
    if (alphabet.empty()) {
        std::cerr << "alphabet is empty; cannot minimize" << std::endl;
        return 1;
    }

    auto mdfa = minimizeDFA(dfa, alphabet);

    if (!saveMindfa("mindfa", mdfa)) {
        std::cerr << "failed to write mindfa" << std::endl;
        return 1;
    }

    std::cout << "generated mindfa (minimized) from dfa" << std::endl;
    return 0;
}
