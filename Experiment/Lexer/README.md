# Lexer — 词法分析器

将字符流切分为 Token 序列。采用正则式 → NFA（Thompson）→ DFA（子集构造）→ 最小化 DFA（Hopcroft）的标准编译器前端流程。

## 三段流水线

| 步骤 | 程序 | 输入 | 输出 | 作用 |
|------|------|------|------|------|
| 1 | `dfa_builder.exe` | `grammer.txt` | `dfa` | 正则→后缀式→Thompson NFA→子集构造 DFA |
| 2 | `dfa_minimizer.exe` | `dfa` | `mindfa` | Hopcroft 最小化 |
| 3 | `lexer_runner.exe` | `mindfa` + `source_char.txt` | `source_token.txt` | 贪心最长匹配分词 |

## 输入文件

- **`grammer.txt`**：Token 正则规则，55 种类型，优先级从上到下递减。★修改规则时需重新运行 dfa_builder + dfa_minimizer
- **`source_char.txt`**：待分析的源程序 ★每次运行前修改此文件

## 输出文件

- **`source_token.txt`**：Token 序列，每行 `<TOKEN_TYPE>\t<lexeme>`（供 Parser 使用）
- **`dfa`**：未最小化的 DFA 文本格式（中间产物）
- **`mindfa`**：最小化 DFA 文本格式（运行时加载）

## 支持的 Token（55 种）

| 类别 | Token 类型 |
|------|-----------|
| 跳过 | WS, LINE_COMMENT, BLOCK_COMMENT, PREPROC |
| 控制流 | IF, ELSE, SWITCH, CASE, DEFAULT, BREAK, CONTINUE, RETURN, DO, WHILE, FOR |
| 类型关键字 | INT_KW, FLOAT_KW, DOUBLE_KW, CHAR_KW, VOID_KW, BOOL_KW |
| 修饰符/类型 | CONST, STATIC, CLASS, STRUCT, ENUM, PUBLIC, PRIVATE |
| 字面量 | TRUE, FALSE, STRING, CHAR_LIT |
| 标识符/数字 | ID, INT |
| 比较运算符 | LE(<=), GE(>=), EQ(==), NE(!=), LT(<), GT(>) |
| 逻辑/增减 | LOG_AND(&&), LOG_OR(\|\|), INC(++), DEC(--) |
| 分隔符 | LPAREN, RPAREN, LBRACE, RBRACE, SEMI, COMMA |
| 单字符运算 | POW(^), PLUS(+), MINUS(-), MUL(*), DIV(/), ASSIGN(=), LT(<), GT(>) |

## 编译（如需重新生成 exe）

```bash
cd Lexer
g++ -std=c++17 -O2 dfa_builder.cpp automata.cpp regex_nfa_dfa.cpp -o dfa_builder.exe
g++ -std=c++17 -O2 dfa_minimizer.cpp dfa_minimize.cpp automata.cpp -o dfa_minimizer.exe
g++ -std=c++17 -O2 lexer_runner.cpp automata.cpp regex_nfa_dfa.cpp -o lexer_runner.exe
```

## 单独运行

```bash
cd Lexer
.\dfa_builder.exe         # 离线：生成 dfa
.\dfa_minimizer.exe       # 离线：生成 mindfa
.\lexer_runner.exe        # 运行时：分词 → source_token.txt
```

## 输出解读

终端输出格式：`TOKEN_TYPE: lexeme`

```
INT_KW: int      ← 类型关键字（不是标识符 ID）
ID: main         ← 函数名
LPAREN: (        ← 左括号
STRING: "hello"  ← 字符串字面量
INT: 42          ← 整数
SEMI: ;          ← 分号
EQ: ==           ← 比较运算符（贪心最长匹配，不拆成两个 =）
```
