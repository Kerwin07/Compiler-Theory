#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>

struct DFA {
    int startState = 0;
    std::set<int> acceptStates;
    std::map<std::pair<int,char>, int> trans;
    std::map<int,std::string> tokenType; // accept state -> token name
};

// mindfa 文本格式：
// start <int>
// accept <state> <TOKEN_NAME>
// trans <from> <char> <to>

bool saveMindfa(const std::string& path, const DFA& dfa);
bool loadMindfa(const std::string& path, DFA& dfa);
