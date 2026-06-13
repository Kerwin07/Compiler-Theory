# lexer 实验

本工程按「正则/文法 → DFA → 最小化DFA → 词法判定」的流水线拆分为 3 个可执行程序。

> 说明：当前正则语法是简化版，仅支持：字面量(字母/数字)、`()`, `|`, `*` 和隐式连接。

## 输入/输出文件

- 输入规则：`grammer.txt`
  - 格式：一行一条规则：`TOKEN_NAME REGEX`
  - 当前实现只读取第一条非注释规则（后续如需多规则需扩展）

- 输入字符流：`source_char.txt`
  - **逐行处理**：每行视为一个独立字符串

- 程序1输出（未最小化 DFA）：`dfa`
- 程序1.5输出（最小化 DFA）：`mindfa`
- 程序2输出：`source_token.txt`
  - 每行输出：
    - 匹配成功：`<TOKEN_NAME>\t<原串>`
    - 匹配失败：`FAIL\t<原串 + 失败位置提示>`

## 三段流水线

1. **生成 DFA（未最小化）**：`dfa_builder.exe`
   - 输入：`grammer.txt`
   - 输出：`dfa`

2. **最小化 DFA**：`dfa_minimizer.exe`
   - 输入：`dfa`
   - 输出：`mindfa`

3. **判定/输出 token**：`lexer_runner.exe`
   - 输入：`mindfa` + `source_char.txt`
   - 输出：`source_token.txt`
