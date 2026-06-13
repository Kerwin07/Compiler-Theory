#include "ll1.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

static inline void trim_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
}

static inline std::string trim(const std::string& s) {
    size_t i = 0, j = s.size();
    while (i < j && std::isspace((unsigned char)s[i])) i++;
    while (j > i && std::isspace((unsigned char)s[j - 1])) j--;
    return s.substr(i, j - i);
}

static inline bool isNonTerminalName(const std::string& sym) {
    // 只将形如单个大写字母（如 "E"）或 大写字母后跟撇号（如 "E'"）识别为非终结符。
    // 这样像 "ID"、"INT" 这样的多字符大写符号将被视为终结符（token）。
    if (sym.empty()) return false;
    if (sym.size() == 1) return std::isupper((unsigned char)sym[0]);
    if (sym.size() == 2 && sym[1] == '\'') return std::isupper((unsigned char)sym[0]);
    return false;
}

static std::vector<std::string> splitBySpace(const std::string& s) {
    std::stringstream ss(s);
    std::vector<std::string> out;
    std::string w;
    while (ss >> w) out.push_back(w);
    return out;
}

static std::vector<std::string> splitAlternatives(const std::string& rhsRaw) {
    // 按 '|' 分割（假设使用空格分隔符号，或符号本身不含 | ）
    std::vector<std::string> alts;
    std::string cur;
    for (char ch : rhsRaw) {
        if (ch == '|') {
            alts.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    alts.push_back(trim(cur));
    return alts;
}

bool loadCFG(const std::string& path, Grammar& g, std::string& err) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        err = "failed to open " + path;
        return false;
    }

    g = Grammar{};

    std::vector<std::pair<std::string, std::string>> rawRules; // lhs, rhsRaw

    std::string line;
    while (std::getline(fin, line)) {
        trim_cr(line);

        auto p = line.find('#');
        if (p != std::string::npos) line = line.substr(0, p);
        line = trim(line);
        if (line.empty()) continue;

        // 支持：A -> ... 或 A->...
        auto arrow = line.find("->");
        if (arrow == std::string::npos) {
            err = "bad rule (missing ->): " + line;
            return false;
        }

        std::string lhs = trim(line.substr(0, arrow));
        std::string rhs = trim(line.substr(arrow + 2));
        if (lhs.empty() || rhs.empty()) {
            err = "bad rule: " + line;
            return false;
        }

        rawRules.push_back({lhs, rhs});
        g.nonterminals.insert(lhs);
        if (g.startSymbol.empty()) g.startSymbol = lhs;
    }

    if (g.startSymbol.empty()) {
        err = "empty grammar";
        return false;
    }

    // 展开产生式（处理 | ）
    for (auto& rr : rawRules) {
        const std::string& lhs = rr.first;
        for (auto& alt : splitAlternatives(rr.second)) {
            Production pr;
            pr.lhs = lhs;
            if (alt == EPS) {
                pr.rhs = {EPS};
            } else {
                pr.rhs = splitBySpace(alt);
                if (pr.rhs.empty()) pr.rhs = {EPS};
            }
            g.prods.push_back(std::move(pr));
        }
    }

    // 先收集所有非终结符（包括出现在 RHS 里的），避免把 E' / T' 误判成终结符
    // g.nonterminals 已在读取 LHS 时收集完毕（LHS 即为非终结符），
    // 不再通过名称启发式检测 RHS 来添加非终结符，避免把终结符误判为非终结符。
    // （如果需要从 RHS 补充非终结符，请把它们显式写在 LHS。）

    // 再统计终结符：不在 nonterminals 且不是 eps
    g.terminals.clear();
    for (auto& pr : g.prods) {
        for (auto& sym : pr.rhs) {
            if (sym == EPS) continue;
            if (!g.nonterminals.count(sym)) g.terminals.insert(sym);
        }
    }

    return true;
}

static std::set<std::string> firstOfSequence(
    const std::vector<std::string>& seq,
    const std::map<std::string, std::set<std::string>>& first
) {
    std::set<std::string> result;
    bool allCanEps = true;

    if (seq.empty()) {
        result.insert(EPS);
        return result;
    }

    for (const auto& X : seq) {
        allCanEps = false;
        if (X == EPS) {
            result.insert(EPS);
            allCanEps = true;
            break;
        }

        auto it = first.find(X);
        if (it == first.end()) {
            // 视为终结符
            result.insert(X);
            return result;
        }

        bool hasEps = false;
        for (auto& a : it->second) {
            if (a == EPS) hasEps = true;
            else result.insert(a);
        }

        if (hasEps) {
            allCanEps = true;
            continue;
        }
        return result;
    }

    if (allCanEps) result.insert(EPS);
    return result;
}

static void computeFirst(const Grammar& g, std::map<std::string, std::set<std::string>>& first) {
    first.clear();

    // 初始化：终结符 FIRST(a)={a}
    for (auto& t : g.terminals) {
        first[t].insert(t);
    }
    // 非终结符初始化空集
    for (auto& nt : g.nonterminals) {
        (void)first[nt];
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < g.prods.size(); i++) {
            const auto& pr = g.prods[i];

            auto addSet = firstOfSequence(pr.rhs, first);
            for (auto& a : addSet) {
                if (!first[pr.lhs].count(a)) {
                    first[pr.lhs].insert(a);
                    changed = true;
                }
            }
        }
    }
}

static void computeFollow(const Grammar& g,
                          const std::map<std::string, std::set<std::string>>& first,
                          std::map<std::string, std::set<std::string>>& follow) {
    follow.clear();
    for (auto& nt : g.nonterminals) (void)follow[nt];
    follow[g.startSymbol].insert(END);

    bool changed = true;
    while (changed) {
        changed = false;

        for (const auto& pr : g.prods) {
            for (size_t i = 0; i < pr.rhs.size(); i++) {
                const std::string& B = pr.rhs[i];
                if (!g.nonterminals.count(B)) continue;

                std::vector<std::string> beta;
                for (size_t j = i + 1; j < pr.rhs.size(); j++) beta.push_back(pr.rhs[j]);
                auto firstBeta = firstOfSequence(beta, first);

                // FIRST(beta) - eps 加入 FOLLOW(B)
                for (auto& a : firstBeta) {
                    if (a == EPS) continue;
                    if (!follow[B].count(a)) {
                        follow[B].insert(a);
                        changed = true;
                    }
                }

                // 如果 beta =>* eps，则 FOLLOW(A) 加入 FOLLOW(B)
                if (beta.empty() || firstBeta.count(EPS)) {
                    for (auto& b : follow.at(pr.lhs)) {
                        if (!follow[B].count(b)) {
                            follow[B].insert(b);
                            changed = true;
                        }
                    }
                }
            }
        }
    }
}

bool buildLL1Table(const Grammar& g, LLTable& out, bool& isLL1, std::string& err) {
    err.clear();
    out = LLTable{};
    isLL1 = true;

    computeFirst(g, out.first);
    computeFollow(g, out.first, out.follow);

    // 确定哪些产生式可推eps（用于冲突消解：非eps优先于eps）
    std::set<size_t> epsProds;
    for (size_t pi = 0; pi < g.prods.size(); pi++) {
        auto firstAlpha = firstOfSequence(g.prods[pi].rhs, out.first);
        if (firstAlpha.count(EPS)) epsProds.insert(pi);
    }

    // 构造预测分析表
    // 两趟：第一趟填非eps产生式，第二趟填eps产生式（冲突时保留已有的非eps条目）
    for (size_t pi = 0; pi < g.prods.size(); pi++) {
        const auto& pr = g.prods[pi];
        auto firstAlpha = firstOfSequence(pr.rhs, out.first);

        for (auto& a : firstAlpha) {
            if (a == EPS) continue;
            if (out.table[pr.lhs].count(a)) {
                isLL1 = false;
                std::ostringstream oss;
                oss << "LL(1) conflict at M[" << pr.lhs << "," << a << "]";
                if (err.empty()) err = oss.str();
                continue; // 保留先填入的条目，不覆盖
            }
            out.table[pr.lhs][a] = (int)pi;
        }
    }

    for (size_t pi = 0; pi < g.prods.size(); pi++) {
        const auto& pr = g.prods[pi];
        if (!epsProds.count(pi)) continue;

        auto fit = out.follow.find(pr.lhs);
        if (fit == out.follow.end()) continue;

        for (auto& b : fit->second) {
            if (out.table[pr.lhs].count(b)) {
                // 冲突但保留已有的非eps条目（如dangling-else：ELSE优先选非eps）
                isLL1 = false;
                std::ostringstream oss;
                oss << "LL(1) conflict at M[" << pr.lhs << "," << b << "] (eps rule, keeping existing)";
                if (err.empty()) err = oss.str();
                continue;
            }
            out.table[pr.lhs][b] = (int)pi;
        }
    }

    return true;
}

static void writeSet(std::ostream& os, const std::set<std::string>& s) {
    bool first = true;
    for (auto& x : s) {
        if (!first) os << " ";
        os << x;
        first = false;
    }
}

bool saveLLTable(const std::string& path, const Grammar& g, const LLTable& t) {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "START " << g.startSymbol << "\n";

    out << "NONTERMINALS";
    for (auto& nt : g.nonterminals) out << " " << nt;
    out << "\n";

    out << "TERMINALS";
    for (auto& tm : g.terminals) out << " " << tm;
    out << " " << END;
    out << "\n";

    out << "PRODUCTIONS " << g.prods.size() << "\n";
    for (size_t i = 0; i < g.prods.size(); i++) {
        out << i << ": " << g.prods[i].lhs << " ->";
        for (auto& sym : g.prods[i].rhs) out << " " << sym;
        out << "\n";
    }

    out << "FIRST\n";
    for (auto& kv : t.first) {
        if (!isNonTerminalName(kv.first)) continue;
        out << kv.first << ": ";
        writeSet(out, kv.second);
        out << "\n";
    }

    out << "FOLLOW\n";
    for (auto& kv : t.follow) {
        out << kv.first << ": ";
        writeSet(out, kv.second);
        out << "\n";
    }

    out << "TABLE\n";
    for (auto& row : t.table) {
        for (auto& cell : row.second) {
            out << row.first << " " << cell.first << " " << cell.second << "\n";
        }
    }

    return true;
}

bool loadLLTable(const std::string& path, Grammar& g, LLTable& t) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    g = Grammar{};
    t = LLTable{};

    std::string line;

    enum Section { NONE, PROD, FIRSTS, FOLLOWS, TABLES } sec = NONE;

    size_t prodCount = 0;
    while (std::getline(in, line)) {
        trim_cr(line);
        line = trim(line);
        if (line.empty()) continue;

        if (line.rfind("START ", 0) == 0) {
            g.startSymbol = trim(line.substr(6));
            continue;
        }
        if (line.rfind("NONTERMINALS", 0) == 0) {
            auto items = splitBySpace(line.substr(std::string("NONTERMINALS").size()));
            for (auto& x : items) if (!x.empty()) g.nonterminals.insert(x);
            continue;
        }
        if (line.rfind("TERMINALS", 0) == 0) {
            auto items = splitBySpace(line.substr(std::string("TERMINALS").size()));
            for (auto& x : items) if (!x.empty() && x != END) g.terminals.insert(x);
            continue;
        }
        if (line.rfind("PRODUCTIONS ", 0) == 0) {
            prodCount = (size_t)std::stoul(trim(line.substr(12)));
            g.prods.clear();
            g.prods.reserve(prodCount);
            sec = PROD;
            continue;
        }
        if (line == "FIRST") { sec = FIRSTS; continue; }
        if (line == "FOLLOW") { sec = FOLLOWS; continue; }
        if (line == "TABLE") { sec = TABLES; continue; }

        if (sec == PROD) {
            // i: A -> ...
            auto colon = line.find(':');
            if (colon == std::string::npos) return false;
            std::string rest = trim(line.substr(colon + 1));
            auto arrow = rest.find("->");
            if (arrow == std::string::npos) return false;
            Production pr;
            pr.lhs = trim(rest.substr(0, arrow));
            auto rhsRaw = trim(rest.substr(arrow + 2));
            pr.rhs = splitBySpace(rhsRaw);
            if (pr.rhs.empty()) pr.rhs = {EPS};
            g.prods.push_back(std::move(pr));
            continue;
        }

        if (sec == FIRSTS || sec == FOLLOWS) {
            auto colon = line.find(':');
            if (colon == std::string::npos) return false;
            std::string sym = trim(line.substr(0, colon));
            auto items = splitBySpace(line.substr(colon + 1));
            std::set<std::string> st;
            for (auto& x : items) if (!x.empty()) st.insert(x);
            if (sec == FIRSTS) t.first[sym] = std::move(st);
            else t.follow[sym] = std::move(st);
            continue;
        }

        if (sec == TABLES) {
            auto parts = splitBySpace(line);
            if (parts.size() != 3) return false;
            std::string A = parts[0];
            std::string a = parts[1];
            int idx = std::stoi(parts[2]);
            t.table[A][a] = idx;
            continue;
        }
    }

    return !g.startSymbol.empty() && g.prods.size() == prodCount;
}
