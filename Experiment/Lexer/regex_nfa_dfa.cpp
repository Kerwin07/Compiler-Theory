#include "regex_nfa_dfa.h"

#include <cctype>
#include <iomanip>
#include <iostream>
#include <queue>
#include <stack>
#include <stdexcept>

// 为 tokenizeAndExpand([]) 使用
#include <set>

// IntelliSense 可能会在函数定义位置变化/解析失败时误报未定义；加前置声明以消除该类报错。
static char parseEscape(const std::string& s, size_t& i);
static int priority(char op);

static int stateCnt = 0;

// 统一管理所有 NFAState，避免生命周期问题（本实验程序不回收，进程结束交给 OS）
static std::vector<NFAState*> g_allStates;

static NFAState* newState() {
    auto s = new NFAState();
    s->id = stateCnt++;
    g_allStates.push_back(s);
    return s;
}

NFAState* createNFAState() {
    return newState();
}

// 用于 postfix 里表示“连接运算符”
static constexpr char OP_CONCAT = '.';
static constexpr char OP_ALT = '|';
static constexpr char OP_STAR = '*';

// 解析 \ 转义序列。
// 约定：调用方传入 i 指向 '\\' 字符，函数返回转义后的实际字符，并将 i 前进到转义序列的最后一个字符。
static char parseEscape(const std::string& s, size_t& i) {
    if (i >= s.size() || s[i] != '\\') throw std::runtime_error("escape: expect \\\\ ");
    if (i + 1 >= s.size()) throw std::runtime_error("dangling escape");
    char n = s[i + 1];
    i += 1; // move to escaped char

    switch (n) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '\\': return '\\';
        // 注意：运算符类字符（如 '*', '+', '(', ')' 等）直接按字面量返回；
        // tokenizeAndExpand 会对其 encodeLiteral，避免与内部运算符冲突。
        default: return n;
    }
}

static int priority(char op) {
    switch (op) {
        case OP_STAR: return 3;
        case OP_CONCAT: return 2;
        case OP_ALT: return 1;
        default: return 0;
    }
}

// 将可能与运算符冲突的字面量字符编码（保证 1-byte）
static unsigned char encodeLiteral(unsigned char c) {
    // 对会参与正则控制语义的字符统一做偏移编码，避免与内部运算符/控制符混淆。
    // 当前实现的正则运算符只用到 . | * ( )，但规则里可能出现 \{ \} 等“需要被当作字面量”的字符。
    // 一旦这些字符以未编码形式进入 token 流，容易在 addConcat / infixToPostfix 阶段造成歧义。
    if (c=='.' || c=='|' || c=='*' || c=='(' || c==')' || c=='{' || c=='}') {
        return (unsigned char)(c + 64); // 保证仍在 ASCII 范围
    }
    return c;
}
static char decodeLiteral(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc == (unsigned char)('.'+64)) return '.';
    if (uc == (unsigned char)('|'+64)) return '|';
    if (uc == (unsigned char)('*'+64)) return '*';
    if (uc == (unsigned char)('('+64)) return '(';
    if (uc == (unsigned char)(')'+64)) return ')';
    if (uc == (unsigned char)('{'+64)) return '{';
    if (uc == (unsigned char)('}'+64)) return '}';
    return c;
}

// 简化：运算符集合固定为 | * ( )，连接符(.)仅作为内部插入的运算符，不应被当作字面量
static bool isOp(char c) {
    return c == OP_ALT || c == OP_STAR || c == '(' || c == ')' || c == OP_CONCAT;
}

static bool isLiteralToken(char c) {
    // postfix 中，非运算符都视为字面量 token
    return !isOp(c);
}

static bool isLeftConcatTarget(char t) {
    return isLiteralToken(t) || t == ')' || t == OP_STAR;
}

static bool isRightConcatTarget(char t) {
    return isLiteralToken(t) || t == '(';
}

// 把 regex 解析成 token 序列（每个 token 是单字节 char）：
// - 普通字符：自身
// - \n 等：替换为实际字符
// - 字符类 [a-z]：展开为 (a|b|...|z)
static std::string tokenizeAndExpand(const std::string& regex) {
    std::string t;
    for (size_t i = 0; i < regex.size(); ++i) {
        char c = regex[i];
        if (std::isspace((unsigned char)c)) continue;

        if (c == '\\') {
            if (i + 1 >= regex.size()) throw std::runtime_error("dangling escape");
            char n = regex[i+1];
            if (n == 'd' || n == 'w' || n == 's') {
                // 预定义类展开
                if (n == 'd') {
                    t += '(';
                    for (char x='0'; x<='9'; ++x) { t += encodeLiteral((unsigned char)x); if (x!='9') t += '|'; }
                    t += ')';
                } else if (n == 'w') {
                    t += '(';
                    bool first=true;
                    auto add=[&](char x){ if(!first) t+='|'; t+=encodeLiteral((unsigned char)x); first=false; };
                    for (char x='a'; x<='z'; ++x) add(x);
                    for (char x='A'; x<='Z'; ++x) add(x);
                    for (char x='0'; x<='9'; ++x) add(x);
                    add('_');
                    t += ')';
                } else if (n == 's') {
                    t += '(';
                    t += encodeLiteral((unsigned char)' ');
                    t += '|'; t += encodeLiteral((unsigned char)'\t');
                    t += '|'; t += encodeLiteral((unsigned char)'\n');
                    t += '|'; t += encodeLiteral((unsigned char)'\r');
                    t += ')';
                }
                i++; // consume n
                continue;
            }

            char esc = parseEscape(regex, i);
            t += encodeLiteral((unsigned char)esc);
            continue;
        }

        if (c == '[') {
            // 字符类：三段式解析（elem + optional range），避免索引跳跃和把 '-' 误当字面量
            // i 指向 '[', 所以从 i+1 开始读内容
            size_t k = i + 1;
            bool negate = false;
            if (k < regex.size() && regex[k] == '^') { negate = true; k++; }

            std::set<unsigned char> cls;

            auto readElem = [&](size_t& p, unsigned char& outChar, bool& producedChar) {
                producedChar = false;
                if (p >= regex.size()) return;
                if (regex[p] == ']') return;

                if (regex[p] == '\\') {
                    if (p + 1 >= regex.size()) throw std::runtime_error("dangling escape in []");
                    char n = regex[p + 1];
                    if (n == 'd' || n == 'w' || n == 's') {
                        if (n == 'd') for (unsigned char x = '0'; x <= '9'; ++x) cls.insert(x);
                        else if (n == 'w') {
                            for (unsigned char x = 'a'; x <= 'z'; ++x) cls.insert(x);
                            for (unsigned char x = 'A'; x <= 'Z'; ++x) cls.insert(x);
                            for (unsigned char x = '0'; x <= '9'; ++x) cls.insert(x);
                            cls.insert('_');
                        } else if (n == 's') {
                            cls.insert(' '); cls.insert('\t'); cls.insert('\n'); cls.insert('\r');
                        }
                        p += 2;
                        return;
                    }
                    outChar = (unsigned char)parseEscape(regex, p);
                    // parseEscape 返回后 p 指向转义的那个字符位置
                    p += 1;
                    producedChar = true;
                    return;
                }

                outChar = (unsigned char)regex[p];
                p += 1;
                producedChar = true;
            };

            bool closed = false;
            while (k < regex.size()) {
                if (regex[k] == ']') { closed = true; break; }

                size_t p = k;
                unsigned char a = 0;
                bool okA = false;
                readElem(p, a, okA);
                if (!okA) { k = p; continue; }

                // optional range: a-b
                // 此时 p 指向读取完 a 后的下一个位置；若紧跟 '-' 且后面不是 ']'，则解析范围
                if (p < regex.size() && regex[p] == '-' && (p + 1) < regex.size() && regex[p + 1] != ']') {
                    // 特殊："[-]" / "[a-]" 的 '-' 作为字面量
                    // 这里条件保证 p+1 不是 ']'，所以是形如 a-b 的范围
                    p++; // skip '-'
                    unsigned char b = 0;
                    bool okB = false;
                    readElem(p, b, okB);
                    if (!okB) throw std::runtime_error("bad range in []");
                    if (a > b) std::swap(a, b);
                    for (unsigned char x = a; x <= b; ++x) cls.insert(x);
                    k = p;
                    continue;
                }

                cls.insert(a);
                k = p;
            }

            if (!closed) throw std::runtime_error("unclosed []");

            // debug: 打印字符类内容（只在 INT/ID 规则中常见的 [0-9] / [A-Za-z_] 时触发）
            // 这里根据原始 regex 片段启发式判断（避免输出太多）。
            if (regex.find("[0-9]") != std::string::npos || regex.find("[A-Za-z_") != std::string::npos) {
                std::cerr << "[debug][cls] regex=" << regex << " size=" << cls.size() << " chars=";
                int cnt = 0;
                for (auto ch : cls) {
                    if (cnt++ > 80) { std::cerr << "..."; break; }
                    if (std::isprint(ch)) std::cerr << (char)ch;
                    else std::cerr << "\\x" << std::hex << (int)ch << std::dec;
                }
                std::cerr << "\n";
            }

            if (negate) {
                std::set<unsigned char> all;
                for (int x = 1; x <= 127; ++x) all.insert((unsigned char)x);
                std::set<unsigned char> inv;
                for (auto x : all) if (!cls.count(x)) inv.insert(x);
                cls.swap(inv);
            }

            // 展开成 ((a)|(b)|(c))：
            // 关键：给每个备选分支单独加括号，避免 addConcat 把相邻字面量误判为需要连接（出现 01|2|...）。
            t += '(';
            bool first = true;
            for (auto x : cls) {
                if (!first) t += '|';
                t += '(';
                t += encodeLiteral((unsigned char)x);
                t += ')';
                first = false;
            }
            t += ')';

            // 将外层 i 跳到 ']' 位置，外层 for 的 ++i 会跳到下一字符
            i = k;
            continue;
        }

        t += encodeLiteral((unsigned char)c);
    }

    return t;
}

static std::string addConcat(const std::string& s) {
    std::string res;
    for (int i = 0; i < (int)s.size(); i++) {
        char a = s[i];
        res += a;
        if (i + 1 < (int)s.size()) {
            char b = s[i+1];
            // 不要在已经显式存在连接符 '.' 的附近重复插入，也不要在 '|' 两侧插。
            if (a == OP_CONCAT || b == OP_CONCAT) continue;
            if (isLeftConcatTarget(a) && isRightConcatTarget(b) && a != OP_ALT && b != OP_ALT) {
                res += OP_CONCAT;
            }
        }
    }
    return res;
}

// =========================
// B方案：Token 流实现
// =========================

// Thompson helpers (token版专用，避免受下方旧实现 static 定义顺序影响)
static NFA tok_single(char c) {
    auto s = newState();
    auto e = newState();
    s->trans[c].push_back(e);
    return {s, e};
}

static NFA tok_concat(NFA a, NFA b) {
    a.end->eps.push_back(b.start);
    return {a.start, b.end};
}

static NFA tok_unite(NFA a, NFA b) {
    auto s = newState();
    auto e = newState();
    s->eps.push_back(a.start);
    s->eps.push_back(b.start);
    a.end->eps.push_back(e);
    b.end->eps.push_back(e);
    return {s, e};
}

static NFA tok_star(NFA a) {
    auto s = newState();
    auto e = newState();
    s->eps.push_back(a.start);
    s->eps.push_back(e);
    a.end->eps.push_back(a.start);
    a.end->eps.push_back(e);
    return {s, e};
}

static bool isRegexOpTok(RegexTokType t) {
    return t == RegexTokType::Alt || t == RegexTokType::Concat || t == RegexTokType::Star || t == RegexTokType::Plus;
}

static int tokPriority(RegexTokType t) {
    switch (t) {
        case RegexTokType::Star: return 3;
        case RegexTokType::Plus: return 3;
        case RegexTokType::Concat: return 2;
        case RegexTokType::Alt: return 1;
        default: return 0;
    }
}

static bool isLeftConcatTargetTok(const RegexTok& t) {
    return t.type == RegexTokType::Literal || t.type == RegexTokType::RParen || t.type == RegexTokType::Star || t.type == RegexTokType::Plus;
}

static bool isRightConcatTargetTok(const RegexTok& t) {
    return t.type == RegexTokType::Literal || t.type == RegexTokType::LParen;
}

static void pushLiteralTok(std::vector<RegexTok>& out, char ch) {
    out.push_back(RegexTok{RegexTokType::Literal, ch});
}

static std::vector<RegexTok> tokenizeAndExpandTokens(const std::string& regex) {
    std::vector<RegexTok> t;

    for (size_t i = 0; i < regex.size(); ++i) {
        char c = regex[i];
        // 仅在正则顶层跳过空白。注意：字符类 [] 内的空格是合法字面量（例如 WS: [ \t\n]+）
        if (std::isspace((unsigned char)c)) continue;

        if (c == '\\') {
            if (i + 1 >= regex.size()) throw std::runtime_error("dangling escape");
            char n = regex[i + 1];

            // 预定义类
            if (n == 'd' || n == 'w' || n == 's') {
                std::set<unsigned char> cls;
                if (n == 'd') {
                    for (unsigned char x = '0'; x <= '9'; ++x) cls.insert(x);
                } else if (n == 'w') {
                    for (unsigned char x = 'a'; x <= 'z'; ++x) cls.insert(x);
                    for (unsigned char x = 'A'; x <= 'Z'; ++x) cls.insert(x);
                    for (unsigned char x = '0'; x <= '9'; ++x) cls.insert(x);
                    cls.insert('_');
                } else if (n == 's') {
                    cls.insert(' '); cls.insert('\t'); cls.insert('\n'); cls.insert('\r');
                }

                t.push_back({RegexTokType::LParen});
                bool first = true;
                for (auto x : cls) {
                    if (!first) t.push_back({RegexTokType::Alt});
                    t.push_back({RegexTokType::LParen});
                    pushLiteralTok(t, (char)x);
                    t.push_back({RegexTokType::RParen});
                    first = false;
                }
                t.push_back({RegexTokType::RParen});

                i++; // consume n
                continue;
            }

            char esc = parseEscape(regex, i);
            pushLiteralTok(t, esc);
            continue;
        }

        if (c == '[') {
            size_t k = i + 1;
            bool negate = false;
            if (k < regex.size() && regex[k] == '^') { negate = true; k++; }

            std::set<unsigned char> cls;

            auto readElem = [&](size_t& p, unsigned char& outChar, bool& producedChar) {
                producedChar = false;
                if (p >= regex.size()) return;
                if (regex[p] == ']') return;

                if (regex[p] == '\\') {
                    if (p + 1 >= regex.size()) throw std::runtime_error("dangling escape in []");
                    char n = regex[p + 1];
                    if (n == 'd' || n == 'w' || n == 's') {
                        if (n == 'd') for (unsigned char x = '0'; x <= '9'; ++x) cls.insert(x);
                        else if (n == 'w') {
                            for (unsigned char x = 'a'; x <= 'z'; ++x) cls.insert(x);
                            for (unsigned char x = 'A'; x <= 'Z'; ++x) cls.insert(x);
                            for (unsigned char x = '0'; x <= '9'; ++x) cls.insert(x);
                            cls.insert('_');
                        } else {
                            cls.insert(' '); cls.insert('\t'); cls.insert('\n'); cls.insert('\r');
                        }
                        p += 2;
                        return;
                    }
                    outChar = (unsigned char)parseEscape(regex, p);
                    p += 1;
                    producedChar = true;
                    return;
                }

                // 注意：这里不能跳过空白；[] 内的空格本身就是一个字符
                outChar = (unsigned char)regex[p];
                p += 1;
                producedChar = true;
            };

            bool closed = false;
            while (k < regex.size()) {
                if (regex[k] == ']') { closed = true; break; }

                size_t p = k;
                unsigned char a = 0;
                bool okA = false;
                readElem(p, a, okA);
                if (!okA) { k = p; continue; }

                if (p < regex.size() && regex[p] == '-' && (p + 1) < regex.size() && regex[p + 1] != ']') {
                    p++;
                    unsigned char b = 0;
                    bool okB = false;
                    readElem(p, b, okB);
                    if (!okB) throw std::runtime_error("bad range in []");
                    if (a > b) std::swap(a, b);
                    for (unsigned char x = a; x <= b; ++x) cls.insert(x);
                    k = p;
                    continue;
                }

                cls.insert(a);
                k = p;
            }

            if (!closed) throw std::runtime_error("unclosed []");

            if (negate) {
                std::set<unsigned char> all;
                for (int x = 1; x <= 127; ++x) all.insert((unsigned char)x);
                std::set<unsigned char> inv;
                for (auto x : all) if (!cls.count(x)) inv.insert(x);
                cls.swap(inv);
            }

            // 用 token 展开：((a)|(b)|...)，彻底避免“字面量紧邻”的误判
            t.push_back({RegexTokType::LParen});
            bool first = true;
            for (auto x : cls) {
                if (!first) t.push_back({RegexTokType::Alt});
                t.push_back({RegexTokType::LParen});
                pushLiteralTok(t, (char)x);
                t.push_back({RegexTokType::RParen});
                first = false;
            }
            t.push_back({RegexTokType::RParen});

            i = k; // jump to ']'
            continue;
        }

        // 普通字符/运算符
        if (c == '(') t.push_back({RegexTokType::LParen});
        else if (c == ')') t.push_back({RegexTokType::RParen});
        else if (c == '|') t.push_back({RegexTokType::Alt});
        else if (c == '*') t.push_back({RegexTokType::Star});
        else if (c == '+') t.push_back({RegexTokType::Plus});
        else pushLiteralTok(t, c);
    }

    return t;
}

static std::vector<RegexTok> addConcatTokens(const std::vector<RegexTok>& in) {
    std::vector<RegexTok> out;
    out.reserve(in.size() * 2);

    for (size_t i = 0; i < in.size(); ++i) {
        const auto& a = in[i];
        out.push_back(a);
        if (i + 1 < in.size()) {
            const auto& b = in[i + 1];
            if (isLeftConcatTargetTok(a) && isRightConcatTargetTok(b)) {
                out.push_back({RegexTokType::Concat});
            }
        }
    }

    return out;
}

std::vector<RegexTok> infixToPostfixTokens(const std::string& regex) {
    auto s = tokenizeAndExpandTokens(regex);
    s = addConcatTokens(s);

    std::vector<RegexTok> out;
    std::stack<RegexTok> st;

    for (auto& tok : s) {
        if (tok.type == RegexTokType::Literal) {
            out.push_back(tok);
        } else if (tok.type == RegexTokType::LParen) {
            st.push(tok);
        } else if (tok.type == RegexTokType::RParen) {
            while (!st.empty() && st.top().type != RegexTokType::LParen) {
                out.push_back(st.top());
                st.pop();
            }
            if (st.empty()) throw std::runtime_error("mismatched parentheses");
            st.pop();
        } else if (isRegexOpTok(tok.type)) {
            while (!st.empty() && isRegexOpTok(st.top().type) && tokPriority(st.top().type) >= tokPriority(tok.type)) {
                out.push_back(st.top());
                st.pop();
            }
            st.push(tok);
        } else {
            throw std::runtime_error("unknown token");
        }
    }

    while (!st.empty()) {
        if (st.top().type == RegexTokType::LParen) throw std::runtime_error("mismatched parentheses");
        out.push_back(st.top());
        st.pop();
    }

    return out;
}

// 复制一个 NFA（复制状态与边），用于实现 + 的 "至少一次"：a+ = a · a*
static NFA cloneNFA(const NFA& a) {
    std::map<NFAState*, NFAState*> mp;
    std::queue<NFAState*> q;
    mp[a.start] = newState();
    q.push(a.start);

    while (!q.empty()) {
        auto cur = q.front();
        q.pop();
        auto cur2 = mp[cur];

        // trans
        for (const auto& kv : cur->trans) {
            char ch = kv.first;
            for (auto* to : kv.second) {
                if (!mp.count(to)) {
                    mp[to] = newState();
                    q.push(to);
                }
                cur2->trans[ch].push_back(mp[to]);
            }
        }
        // eps
        for (auto* to : cur->eps) {
            if (!mp.count(to)) {
                mp[to] = newState();
                q.push(to);
            }
            cur2->eps.push_back(mp[to]);
        }
    }

    return {mp[a.start], mp[a.end]};
}

NFA buildNFAFromPostfixTokens(const std::vector<RegexTok>& postfix) {
    std::stack<NFA> st;

    for (auto& tok : postfix) {
        if (tok.type == RegexTokType::Literal) {
            st.push(tok_single(tok.ch));
        } else if (tok.type == RegexTokType::Concat) {
            if (st.size() < 2) throw std::runtime_error("concat: bad postfix");
            auto b = st.top(); st.pop();
            auto a = st.top(); st.pop();
            st.push(tok_concat(a, b));
        } else if (tok.type == RegexTokType::Alt) {
            if (st.size() < 2) throw std::runtime_error("alt: bad postfix");
            auto b = st.top(); st.pop();
            auto a = st.top(); st.pop();
            st.push(tok_unite(a, b));
        } else if (tok.type == RegexTokType::Star) {
            if (st.empty()) throw std::runtime_error("star: bad postfix");
            auto a = st.top(); st.pop();
            st.push(tok_star(a));
        } else if (tok.type == RegexTokType::Plus) {
            if (st.empty()) throw std::runtime_error("plus: bad postfix");
            auto a = st.top(); st.pop();
            // a+ = a · (a)* ；用 clone 保证结构独立
            auto a2 = cloneNFA(a);
            st.push(tok_concat(a, tok_star(a2)));
        } else {
            throw std::runtime_error("unknown postfix token");
        }
    }

    if (st.size() != 1) throw std::runtime_error("bad postfix");
    return st.top();
}

std::set<char> collectAlphabetFromRegexTokens(const std::string& regex) {
    auto toks = infixToPostfixTokens(regex);
    std::set<char> alpha;
    for (auto& t : toks) {
        if (t.type == RegexTokType::Literal) alpha.insert(t.ch);
    }
    return alpha;
}

// --- 兼容旧接口：走 token 版本 ---
std::string infixToPostfix(std::string regex) {
    auto toks = infixToPostfixTokens(regex);
    std::string out;
    out.reserve(toks.size());
    for (auto& t : toks) {
        switch (t.type) {
            case RegexTokType::Literal: out.push_back(encodeLiteral((unsigned char)t.ch)); break;
            case RegexTokType::Alt: out.push_back(OP_ALT); break;
            case RegexTokType::Concat: out.push_back(OP_CONCAT); break;
            case RegexTokType::Star: out.push_back(OP_STAR); break;
            default: break; // parentheses should not appear in postfix
        }
    }
    return out;
}

NFA buildNFAFromPostfix(const std::string& postfix) {
    // 统一走 token 版本，避免单字符 postfix 的歧义与 helper 顺序问题
    std::vector<RegexTok> toks;
    toks.reserve(postfix.size());

    for (char c : postfix) {
        if (isLiteralToken(c)) {
            toks.push_back({RegexTokType::Literal, decodeLiteral(c)});
        } else if (c == OP_CONCAT) {
            toks.push_back({RegexTokType::Concat});
        } else if (c == OP_ALT) {
            toks.push_back({RegexTokType::Alt});
        } else if (c == OP_STAR) {
            toks.push_back({RegexTokType::Star});
        } else {
            throw std::runtime_error("bad regex: unknown operator");
        }
    }

    return buildNFAFromPostfixTokens(toks);
}

std::set<char> collectAlphabetFromRegex(const std::string& regex) {
    // 用 token 版本收集 alphabet（避免 string 展开阶段的歧义）
    return collectAlphabetFromRegexTokens(regex);
}

// -------------------------
// Thompson helpers (string/postfix版)
// -------------------------
static NFA single(char c) {
    auto s = newState();
    auto e = newState();
    char real = decodeLiteral(c);
    s->trans[real].push_back(e);
    return {s,e};
}

static NFA concat(NFA a, NFA b) {
    a.end->eps.push_back(b.start);
    return {a.start, b.end};
}

static NFA unite(NFA a, NFA b) {
    auto s = newState();
    auto e = newState();
    s->eps.push_back(a.start);
    s->eps.push_back(b.start);
    a.end->eps.push_back(e);
    b.end->eps.push_back(e);
    return {s,e};
}

static NFA star(NFA a) {
    auto s = newState();
    auto e = newState();
    s->eps.push_back(a.start);
    s->eps.push_back(e);
    a.end->eps.push_back(a.start);
    a.end->eps.push_back(e);
    return {s,e};
}

// -------------------------
// DFA subset construction
// -------------------------
static std::set<NFAState*> epsClosure(std::set<NFAState*> s) {
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

static std::set<NFAState*> moveSet(const std::set<NFAState*>& s, char c) {
    std::set<NFAState*> res;
    for (auto x : s) {
        auto it = x->trans.find(c);
        if (it == x->trans.end()) continue;
        for (auto y : it->second) res.insert(y);
    }
    return res;
}

DFA buildDFA(const NFA& nfa, const std::set<char>& alphabet) {
    DFA dfa;
    std::map<std::set<NFAState*>, int> mp;
    std::vector<std::set<NFAState*>> states;
    std::queue<std::set<NFAState*>> q;

    std::set<NFAState*> start = epsClosure({nfa.start});
    mp[start] = 0;
    states.push_back(start);
    q.push(start);
    dfa.startState = 0;

    int id = 1;

    while (!q.empty()) {
        auto cur = q.front(); q.pop();
        int cid = mp[cur];

        for (char c : alphabet) {
            auto nxt = epsClosure(moveSet(cur, c));
            if (nxt.empty()) continue;

            if (!mp.count(nxt)) {
                mp[nxt] = id++;
                states.push_back(nxt);
                q.push(nxt);
            }

            dfa.trans[{cid, c}] = mp[nxt];
        }
    }

    // 多规则时：一个 DFA 状态集合里可能含多个接受态，选择优先级更高（数值更小）的 token
    for (int sid = 0; sid < (int)states.size(); sid++) {
        bool ok = false;
        int bestPri = 1e9;
        std::string bestTok;
        for (auto ns : states[sid]) {
            if (!ns->isAccept) continue;
            ok = true;
            if (ns->acceptPriority < bestPri) {
                bestPri = ns->acceptPriority;
                bestTok = ns->acceptToken;
            }
        }
        if (ok) {
            dfa.acceptStates.insert(sid);
            dfa.tokenType[sid] = bestTok;
        }
    }

    return dfa;
}
