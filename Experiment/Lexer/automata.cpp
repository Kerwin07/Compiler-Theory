#include "automata.h"

#include <fstream>
#include <sstream>

// 将 DFA trans 的字符序列化为可见、安全的单词 token，避免空白/换行破坏 mindfa 文本格式。
static std::string escapeChar(unsigned char c) {
    switch (c) {
        case ' ': return "\\s";
        case '\t': return "\\t";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\\': return "\\\\";
        default:
            break;
    }
    if (c < 0x20 || c >= 0x7F) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\x%02X", (unsigned)c);
        return buf;
    }
    return std::string(1, (char)c);
}

static bool unescapeCharToken(const std::string& tok, char& out) {
    if (tok.empty()) return false;
    if (tok.size() == 1 && tok[0] != '\\') {
        out = tok[0];
        return true;
    }
    if (tok == "\\s") { out = ' '; return true; }
    if (tok == "\\t") { out = '\t'; return true; }
    if (tok == "\\n") { out = '\n'; return true; }
    if (tok == "\\r") { out = '\r'; return true; }
    if (tok == "\\\\") { out = '\\'; return true; }
    if (tok.size() == 4 && tok[0] == '\\' && tok[1] == 'x') {
        unsigned val = 0;
        for (int i = 2; i < 4; ++i) {
            char ch = tok[i];
            val <<= 4;
            if (ch >= '0' && ch <= '9') val |= (ch - '0');
            else if (ch >= 'A' && ch <= 'F') val |= (ch - 'A' + 10);
            else if (ch >= 'a' && ch <= 'f') val |= (ch - 'a' + 10);
            else return false;
        }
        out = (char)val;
        return true;
    }
    return false;
}

bool saveMindfa(const std::string& path, const DFA& dfa) {
    std::ofstream fout(path);
    if (!fout.is_open()) return false;

    fout << "start " << dfa.startState << "\n";
    for (int s : dfa.acceptStates) {
        auto it = dfa.tokenType.find(s);
        std::string tok = (it == dfa.tokenType.end()) ? "TOKEN" : it->second;
        fout << "accept " << s << " " << tok << "\n";
    }

    for (auto& kv : dfa.trans) {
        int from = kv.first.first;
        unsigned char c = (unsigned char)kv.first.second;
        int to = kv.second;
        fout << "trans " << from << " " << escapeChar(c) << " " << to << "\n";
    }

    return true;
}

bool loadMindfa(const std::string& path, DFA& dfa) {
    std::ifstream fin(path);
    if (!fin.is_open()) return false;

    dfa = DFA{};

    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line[0] == '#') continue;

        std::stringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "start") {
            ss >> dfa.startState;
        } else if (tag == "accept") {
            int st;
            std::string tok;
            ss >> st >> tok;
            dfa.acceptStates.insert(st);
            dfa.tokenType[st] = tok;
        } else if (tag == "trans") {
            int from, to;
            std::string cTok;
            ss >> from >> cTok >> to;
            char c;
            if (!unescapeCharToken(cTok, c)) {
                continue;
            }
            dfa.trans[{from, c}] = to;
        }
    }

    return true;
}
