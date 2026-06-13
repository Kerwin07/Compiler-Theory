#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

struct Production {
    std::string lhs;
    std::vector<std::string> rhs; // rhs 若为空/或仅包含 eps，表示空产生式
};

struct Grammar {
    std::string startSymbol;
    std::set<std::string> nonterminals;
    std::set<std::string> terminals;
    std::vector<Production> prods;
};

struct LLTable {
    // table[A][a] = productionIndex
    std::map<std::string, std::map<std::string, int>> table;

    std::map<std::string, std::set<std::string>> first;
    std::map<std::string, std::set<std::string>> follow;
};

constexpr const char* EPS = "eps";
constexpr const char* END = "$";

bool loadCFG(const std::string& path, Grammar& g, std::string& err);

// 计算 FIRST/FOLLOW 并构造分析表；若冲突则 isLL1=false 且 err 提供信息。
bool buildLL1Table(const Grammar& g, LLTable& out, bool& isLL1, std::string& err);

bool saveLLTable(const std::string& path, const Grammar& g, const LLTable& t);

// 读取表文件（供程序2使用）
bool loadLLTable(const std::string& path, Grammar& g, LLTable& t);
