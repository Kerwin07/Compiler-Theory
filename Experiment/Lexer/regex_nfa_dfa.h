#pragma once

#include "automata.h"

#include <map>
#include <set>
#include <string>
#include <vector>

struct NFAState {
    int id;
    std::map<char, std::vector<NFAState*>> trans;
    std::vector<NFAState*> eps;
    bool isAccept = false;
    std::string acceptToken;
    int acceptPriority = 0; // 小的优先级更高（规则越靠前越优先）
};

struct NFA {
    NFAState* start;
    NFAState* end;
};

// 正则语法（实验用简化但够用）：
// - 连接：隐式（内部会插入 '.'）
// - 选择：|
// - 闭包：*
// - 正闭包：+   （至少一次）
// - 分组：()
// - 转义：\n \t \r \\ \| \* \( \) \[ \] \' \" 等（\xNN 也支持）
// - 字符类：[abc]、[a-z]、[0-9]、[^\n]（支持 ^ 取反；支持转义）
// - 预定义类：\d(数字) \w(字母数字下划线) \s(空白)
// 注：不支持 ? {m,n}

// --- Token-based regex pipeline (B方案) ---
// 用结构化 token 替代 std::string(char流)，避免出现 "01"、"AB" 这类看似粘连/实际是两个 token 的歧义，
// 同时让 addConcat/shunting-yard 的判断基于 token 类型而非字符本身。

enum class RegexTokType {
    Literal,
    LParen,
    RParen,
    Alt,     // |
    Concat,  // . (internal)
    Star,    // *
    Plus     // +
};

struct RegexTok {
    RegexTokType type;
    char ch = 0; // only for Literal
};

// 将中缀 regex 转为后缀（Shunting-yard）；字面量/字符类会被编码成 1 字节 token
std::string infixToPostfix(std::string regex);

// 将中缀 regex 转为后缀（Shunting-yard）；字面量/字符类会被展开为 Literal token
std::vector<RegexTok> infixToPostfixTokens(const std::string& regex);

// Thompson 构造：从 token 后缀式构建 NFA
NFA buildNFAFromPostfixTokens(const std::vector<RegexTok>& postfix);

NFA buildNFAFromPostfix(const std::string& postfix);

// alphabet 缺省由正则里出现的字面量收集
std::set<char> collectAlphabetFromRegex(const std::string& regex);

// alphabet 缺省由正则里出现的字面量收集（token 版）
std::set<char> collectAlphabetFromRegexTokens(const std::string& regex);

DFA buildDFA(const NFA& nfa, const std::set<char>& alphabet);

// 创建一个新的 NFAState（与 Thompson 构造使用同一套计数/初始化逻辑）
NFAState* createNFAState();
