#include "token_io.h"

#include <fstream>
#include <sstream>

static inline void trim_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
}

bool readTokenFile(const std::string& path, std::vector<Token>& outTokens) {
    std::ifstream fin(path);
    if (!fin.is_open()) return false;

    outTokens.clear();

    std::string line;
    while (std::getline(fin, line)) {
        trim_cr(line);
        if (line.empty()) continue;

        // 允许 lexeme 含空格：只按首个 \t 切
        std::string type, lexeme;
        auto p = line.find('\t');
        if (p == std::string::npos) {
            // 兼容：没有 \t 的情况，整行当 type
            type = line;
            lexeme = "";
        } else {
            type = line.substr(0, p);
            lexeme = line.substr(p + 1);
        }

        if (type.empty()) return false;
        outTokens.push_back({type, lexeme});
    }

    return true;
}
