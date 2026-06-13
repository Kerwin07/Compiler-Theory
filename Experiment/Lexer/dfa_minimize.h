#pragma once

#include "automata.h"
#include <set>

// 最小化 DFA，alphabet 是 DFA 的字母表
DFA minimizeDFA(const DFA& dfa, const std::set<char>& alphabet);
