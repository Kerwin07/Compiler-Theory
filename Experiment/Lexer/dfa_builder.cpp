// D:\msys64\ucrt64\bin\g++.exe -std=c++17 -O2 -g automata.cpp regex_nfa_dfa.cpp dfa_builder.cpp -o dfa_builder.exe

#include "automata.h"
#include "regex_nfa_dfa.h"

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

// debug helpers
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <stack>

struct Rule {
    std::string tokenName;
    std::string regex;
};

static std::vector<Rule> readRules(const std::string& path) {
    std::ifstream fin(path);
    if (!fin.is_open()) return {};

    std::vector<Rule> rules;
    std::string line;
    while (std::getline(fin, line)) {
        auto p = line.find('#');
        if (p != std::string::npos) line = line.substr(0, p);

        auto isSpace = [](unsigned char ch){ return std::isspace(ch); };
        while (!line.empty() && isSpace((unsigned char)line.back())) line.pop_back();
        size_t i = 0;
        while (i < line.size() && isSpace((unsigned char)line[i])) i++;
        line = line.substr(i);
        if (line.empty()) continue;

        std::stringstream ss(line);
        Rule r;
        ss >> r.tokenName;
        if (r.tokenName.empty()) continue;

        // regex 可能包含空格（例如 [ \t\n]+ 之类），因此读取行剩余部分
        std::string rest;
        std::getline(ss, rest);
        while (!rest.empty() && isSpace((unsigned char)rest.front())) rest.erase(rest.begin());
        r.regex = rest;

        if (!r.regex.empty()) rules.push_back(r);
    }
    return rules;
}

static std::string toPrintable(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 4);
    for (unsigned char uc : s) {
        char c = (char)uc;
        if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (std::isprint(uc)) out += c;
        else {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\x%02X", (unsigned)uc);
            out += buf;
        }
    }
    return out;
}

static std::string toHexBytes(const std::string& s) {
    std::string out;
    char buf[8];
    for (unsigned char uc : s) {
        std::snprintf(buf, sizeof(buf), "%02X ", (unsigned)uc);
        out += buf;
    }
    return out;
}

// 复制一份 epsClosure（dfa_builder 内部调试用）
static std::set<NFAState*> epsClosureLocal(std::set<NFAState*> s) {
    std::stack<NFAState*> st;
    for (auto x : s) st.push(x);

    while (!st.empty()) {
        auto cur = st.top(); st.pop();
        for (auto nxt : cur->eps) {
            if (!s.count(nxt)) {
                s.insert(nxt);
                st.push(nxt);
            }
        }
    }
    return s;
}

int main() {
    auto rules = readRules("grammer.txt");
    if (rules.empty()) {
        std::cerr << "failed to read rules from grammer.txt" << std::endl;
        return 1;
    }

    std::cout << "[dfa_builder] loaded rules: " << rules.size() << "\n";
    for (size_t i = 0; i < rules.size(); ++i) {
        std::cout << "  [" << i << "] " << rules[i].tokenName << " => " << rules[i].regex << "\n";
    }

    // 构造总 NFA：globalStart ε 指向每条规则的“ruleStart”；ruleStart 再 ε 指向该规则的 NFA start。
    // 关键修复：combined.end 不能为 nullptr。
    // 否则 buildDFA 在挑选 accept state 时永远看不到 isAccept=true 的 NFAState。
    auto globalStart = createNFAState();
    auto globalAccept = createNFAState();
    globalAccept->isAccept = true;
    globalAccept->acceptToken = "__ACCEPT__";
    globalAccept->acceptPriority = 1e9;

    std::vector<NFA> nfas;
    nfas.reserve(rules.size());

    // 临时：只取前 N 条规则做定位（定位崩溃用）
    int N = (int)rules.size();
    const char* envN = std::getenv("LEX_RULE_LIMIT");
    if (envN) {
        try { N = std::max(1, std::min((int)rules.size(), std::stoi(envN))); } catch (...) {}
    }
    std::cout << "[dfa_builder] rule limit: " << N << "\n";

    // 为定位问题：保存每条规则的 NFA 起点/终点指针
    std::vector<NFAState*> ruleStartStates;
    std::vector<NFAState*> ruleAccept;
    ruleStartStates.reserve(rules.size());
    ruleAccept.reserve(rules.size());

    for (int idx = 0; idx < N; ++idx) {
        const auto& r = rules[idx];
        std::cout << "[dfa_builder] building rule #" << idx << " token=" << r.tokenName << "\n";
        try {
            // B方案：使用 token 版正则流水线
            auto postfixToks = infixToPostfixTokens(r.regex);

            // --- debug (disabled by default) ---
            // if (r.tokenName == "WS" || r.tokenName == "ID" || r.tokenName == "INT" || r.tokenName == "IF" || r.tokenName == "WHILE" ||
            //     r.tokenName == "INT_KW" || r.tokenName == "RETURN") {
            //     // 仅做最小 debug：打印 token 数量（避免把 token 打印成字符串产生歧义）
            //     std::cout << "  [debug] postfixTokens(" << r.tokenName << ") count=" << postfixToks.size() << "\n";
            // }

            auto nfa = buildNFAFromPostfixTokens(postfixToks);

            // 规则级接受态信息（用于在 DFA state 中挑选 token）
            nfa.end->isAccept = true;
            nfa.end->acceptToken = r.tokenName;
            nfa.end->acceptPriority = idx;

            ruleStartStates.push_back(nfa.start);
            ruleAccept.push_back(nfa.end);

            // 同时把所有规则的结束点连到一个全局终点，保证整个 combined NFA 的结构完整
            nfa.end->eps.push_back(globalAccept);

            auto ruleStart = createNFAState();
            ruleStart->eps.push_back(nfa.start);
            globalStart->eps.push_back(ruleStart);

            nfas.push_back(nfa);
        } catch (const std::exception& ex) {
            std::cerr << "[dfa_builder] regex parse/build failed at rule #" << idx
                      << " token=" << r.tokenName
                      << " regex=" << r.regex
                      << " error=" << ex.what() << std::endl;
            return 2;
        }
    }

    // 合并链路粗检：规则的 start 必须在 globalStart 的 ε-闭包中
    // --- debug (disabled by default) ---
    // {
    //     auto startSet = epsClosureLocal({globalStart});
    //     std::cout << "[debug] globalStart eps-closure size=" << startSet.size() << "\n";
    //     for (int idx = 0; idx < (int)ruleStartStates.size(); ++idx) {
    //         const auto& r = rules[idx];
    //         if (r.tokenName == "ID" || r.tokenName == "INT" || r.tokenName == "IF" || r.tokenName == "WHILE" ||
    //             r.tokenName == "INT_KW" || r.tokenName == "RETURN") {
    //             bool ok = startSet.count(ruleStartStates[idx]) != 0;
    //             std::cout << "  [debug] ruleStart in globalStart-closure? token=" << r.tokenName
    //                       << " => " << (ok ? "YES" : "NO") << " (id=" << ruleStartStates[idx]->id << ")\n";
    //         }
    //     }
    // }

    std::set<char> alphabet;
    for (int idx = 0; idx < N; ++idx) {
        auto& r = rules[idx];
        try {
            auto a = collectAlphabetFromRegexTokens(r.regex);
            alphabet.insert(a.begin(), a.end());
        } catch (const std::exception& ex) {
            std::cerr << "[dfa_builder] alphabet collect failed token=" << r.tokenName
                      << " regex=" << r.regex
                      << " error=" << ex.what() << std::endl;
            return 3;
        }
    }

    std::cout << "[dfa_builder] alphabet size: " << alphabet.size() << "\n";

    NFA combined{globalStart, globalAccept};
    auto dfa = buildDFA(combined, alphabet);

    // --- debug summary: accept/token coverage (disabled by default) ---
    // {
    //     std::cout << "accept count=" << dfa.acceptStates.size() << "\n";
    //     std::map<std::string, int> tokCnt;
    //     for (int sid : dfa.acceptStates) {
    //         auto it = dfa.tokenType.find(sid);
    //         if (it != dfa.tokenType.end()) tokCnt[it->second]++;
    //     }
    //     std::cout << "[dfa_builder] accept token coverage:";
    //     for (auto& kv : tokCnt) std::cout << " " << kv.first << "=" << kv.second;
    //     std::cout << "\n";
    // }

    // 写出：标准流水线：dfa_builder 只生成 dfa；mindfa 由 dfa_minimizer 从 dfa 最小化得到。
    if (!saveMindfa("dfa", dfa)) {
        std::cerr << "failed to write dfa" << std::endl;
        return 1;
    }

    std::cout << "generated dfa (not minimized) -> dfa" << std::endl;
    return 0;
}
