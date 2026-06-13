#include "dfa_minimize.h"
#include <map>
#include <queue>
#include <vector>
#include <algorithm>

// 使用 Hopcroft 算法最小化 DFA
DFA minimizeDFA(const DFA& dfa, const std::set<char>& alphabet) {
    // 1) 仅收集 start 可达状态（避免不可达状态参与划分，造成 tokenType/accept 被“稀释”或错误合并）
    std::set<int> states;
    {
        std::queue<int> q;
        states.insert(dfa.startState);
        q.push(dfa.startState);
        while (!q.empty()) {
            int s = q.front(); q.pop();
            for (char c : alphabet) {
                auto it = dfa.trans.find({s, c});
                if (it == dfa.trans.end()) continue;
                int to = it->second;
                if (!states.count(to)) {
                    states.insert(to);
                    q.push(to);
                }
            }
        }
    }

    // 2) 构建反向转移表：rev[c][to] = {from...}
    std::map<char, std::map<int, std::set<int>>> rev;
    for (const auto& kv : dfa.trans) {
        int from = kv.first.first;
        char c = kv.first.second;
        int to = kv.second;
        if (!states.count(from) || !states.count(to)) continue;
        rev[c][to].insert(from);
    }

    // 3) 初始划分：接受态按 tokenType 分组，非接受态一组
    std::map<std::string, std::set<int>> acceptGroups;
    for (int s : dfa.acceptStates) {
        if (!states.count(s)) continue;
        std::string t;
        auto it = dfa.tokenType.find(s);
        if (it != dfa.tokenType.end()) t = it->second;
        acceptGroups[t].insert(s);
    }

    std::vector<std::set<int>> P; // partition
    for (auto& kv : acceptGroups) {
        if (!kv.second.empty()) P.push_back(kv.second);
    }

    std::set<int> nonAccept;
    for (int s : states) {
        if (!dfa.acceptStates.count(s)) nonAccept.insert(s);
    }
    if (!nonAccept.empty()) P.push_back(nonAccept);

    // 4) 工作队列 W
    std::queue<std::set<int>> W;
    for (auto& block : P) W.push(block);

    while (!W.empty()) {
        auto A = W.front();
        W.pop();

        for (char c : alphabet) {
            // X = { q | delta(q, c) in A }
            std::set<int> X;
            auto itMap = rev.find(c);
            if (itMap != rev.end()) {
                for (int to : A) {
                    auto itPred = itMap->second.find(to);
                    if (itPred != itMap->second.end()) {
                        X.insert(itPred->second.begin(), itPred->second.end());
                    }
                }
            }
            if (X.empty()) continue;

            std::vector<std::set<int>> newP;
            for (auto& Y : P) {
                std::set<int> Y1, Y2;
                for (int s : Y) {
                    if (X.count(s)) Y1.insert(s);
                    else Y2.insert(s);
                }
                if (Y1.empty() || Y2.empty()) {
                    newP.push_back(Y);
                } else {
                    newP.push_back(Y1);
                    newP.push_back(Y2);
                    // 简化处理：两块都入队（正确性 OK，效率差一点）
                    W.push(Y1);
                    W.push(Y2);
                }
            }
            P.swap(newP);
        }
    }

    // 5) 构造最小化 DFA
    std::map<int, int> stateToNew;
    for (int i = 0; i < (int)P.size(); ++i) {
        for (int s : P[i]) stateToNew[s] = i;
    }

    DFA md;
    md.startState = stateToNew[dfa.startState];

    // 接受态以及 tokenType：同一块内接受态 tokenType 理论上相同（初始分组已保证）
    for (int i = 0; i < (int)P.size(); ++i) {
        for (int s : P[i]) {
            if (dfa.acceptStates.count(s)) {
                md.acceptStates.insert(i);
                auto it = dfa.tokenType.find(s);
                if (it != dfa.tokenType.end()) md.tokenType[i] = it->second;
                break;
            }
        }
    }

    // 转移：任选代表状态
    for (int i = 0; i < (int)P.size(); ++i) {
        if (P[i].empty()) continue;
        int repr = *P[i].begin();
        for (char c : alphabet) {
            auto it = dfa.trans.find({repr, c});
            if (it != dfa.trans.end() && stateToNew.count(it->second)) {
                md.trans[{i, c}] = stateToNew[it->second];
            }
        }
    }

    return md;
}
