// D:\msys64\ucrt64\bin\g++.exe -std=c++17 -O2 -g automata.cpp lexer_runner.cpp -o lexer_runner.exe

#include "automata.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Token {
    std::string type;
    std::string value;
};

static bool readAll(const std::string& path, std::string& out) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open()) return false;
    std::ostringstream ss;
    ss << fin.rdbuf();
    out = ss.str();
    // Windows 文本可能含 \r\n，这里保留 \r 也能走 DFA；更常见需求是把 \r 忽略
    return true;
}

static bool writeTokens(const std::string& path, const std::vector<Token>& tokens) {
    std::ofstream fout(path);
    if (!fout.is_open()) return false;
    for (auto& t : tokens) {
        fout << t.type << "\t" << t.value << "\n";
    }
    return true;
}

static bool isSkipToken(const std::string& tokenType) {
    return tokenType == "WS" || tokenType == "COMMENT";
}

static int stepDFA(const DFA& dfa, int state, char c) {
    auto it = dfa.trans.find({state, c});
    if (it == dfa.trans.end()) return -1;
    return it->second;
}

int main() {
    DFA dfa;
    if (!loadMindfa("mindfa", dfa)) {
        std::cerr << "failed to read mindfa" << std::endl;
        return 1;
    }

    std::string input;
    if (!readAll("source_char.txt", input)) {
        std::cerr << "failed to read source_char.txt" << std::endl;
        return 1;
    }

    std::vector<Token> out;

    size_t i = 0;
    while (i < input.size()) {
        // 可选：忽略 \r（Windows 换行）
        if (input[i] == '\r') { i++; continue; }

        int state = dfa.startState;
        size_t j = i;

        // 记录“最后一次到达接受态的位置与token类型”
        bool hasAccept = false;
        size_t lastAcceptPos = i;
        std::string lastAcceptType;

        while (j < input.size()) {
            char c = input[j];
            if (c == '\r') break; // 让 \r 单独处理

            int ns = stepDFA(dfa, state, c);
            if (ns < 0) break;
            state = ns;
            j++;

            if (dfa.acceptStates.count(state)) {
                hasAccept = true;
                lastAcceptPos = j;
                auto it = dfa.tokenType.find(state);
                lastAcceptType = (it == dfa.tokenType.end()) ? "TOKEN" : it->second;
            }
        }

        if (!hasAccept) {
            // 错误：当前位置无法匹配任何 token
            std::ostringstream oss;
            char bad = input[i];
            if (bad == '\n') oss << "\\n";
            else if (bad == '\t') oss << "\\t";
            else if (bad == '\0') oss << "\\0";
            else oss << bad;

            out.push_back({"ERROR", "unexpected char: " + oss.str()});
            i++; // 错误恢复：跳过一个字符
            continue;
        }

        std::string lexeme = input.substr(i, lastAcceptPos - i);
        if (!isSkipToken(lastAcceptType)) {
            out.push_back({lastAcceptType, lexeme});
        }

        i = lastAcceptPos;
    }

    if (!writeTokens("source_token.txt", out)) {
        std::cerr << "failed to write source_token.txt" << std::endl;
        return 1;
    }

    for (auto& t : out) {
        std::cout << t.type << ": " << t.value << std::endl;
    }

    return 0;
}
