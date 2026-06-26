# 简易编译器

一个从零实现的编译前端 + 树遍历解释器后端，支持类 C 语法的子集。C++17 编写，Windows 平台（MSYS2 ucrt64 g++）。

## 快速开始

```powershell
# 1. 修改源码
notepad Lexer\source_char.txt

# 2. 一键编译运行
.\build_and_run.ps1
```

## 支持的语言特性

```c
// 变量声明（C 风格）
int x = 5;
int y = 10;

// 表达式（运算符优先级从低到高）
// == != > < >= <=  <  + -  <  * /  <  ^  <  一元 -
int z = x + y * 2;    // 10 + 20 = 30
int p = 2 ^ 4;        // 指数：2 的 4 次方 = 16
int q = -a;           // 一元负号
bool r = x != y;      // 比较：不等，结果为 1（真）

// 条件分支（可嵌套，else 绑定最近 if）
if (x == 5) {
    z = z + 1;
} else {
    z = 0;
}

// 循环
int i = 0;
while (i < 3) {
    x = x + 1;
    i = i + 1;
}

// 函数调用
printf(x);
printf("result=", z);

// 返回
return 0;
```

## 总体流水线

```
source_char.txt        grammer.txt          grammar_cfg.txt
       │                    │                     │
       ▼                    ▼                     ▼
   ┌───────┐         ┌────────────┐        ┌───────────────┐
   │ Lexer │◄────────│ DFA Builder │        │ LL Table      │
   │ 词法  │         │   (离线)     │        │ Builder (离线) │
   │ 分析  │         └────────────┘        └───────┬───────┘
   └───┬───┘                                      │
       │ source_token.txt                 ll_table.txt
       │          │                              │
       ▼          └──────────┬───────────────────┘
   ┌────────┐                ▼
   │ Parser │──────────▶ ast.json / ast.txt
   │ 语法   │
   │ 分析   │
   └──┬─────┘
      │
  ┌───┴───┐
  ▼       ▼
┌────────┐ ┌────────┐
│Semantic│ │Codegen │
│ 语义   │ │ 代码   │
│ 分析   │ │ 执行   │
└───┬────┘ └───┬────┘
    ▼           ▼
semantic     Program
_report.txt  returned: N
```

## 目录结构

```
Experiment/
├── build_and_run.ps1           # 一键运行全部
├── README.md                   # 本文件
├── 项目说明.md                  # 详细项目说明
├── 编译器总览_流水线与数据流.md  # 数据流详解
├── Lexer/                      # 词法分析
│   ├── grammer.txt             # Token 正则规则（55 种）
│   ├── source_char.txt         # ★ 修改此文件改变输入
│   ├── source_token.txt        # 词法输出
│   ├── dfa / mindfa            # DFA 中间产物
│   ├── dfa_builder.exe         # 正则→DFA
│   ├── dfa_minimizer.exe       # DFA 最小化
│   ├── lexer_runner.exe        # 运行时分词
│   ├── 代码详解_词法分析器.md   # 逐函数详细讲解
│   ├── 实验报告_词法分析器.md   # 实验报告
│   └── README.md
├── Parser/                     # 语法分析
│   ├── grammar_cfg.txt         # CFG 文法（49 条产生式）
│   ├── ll_table.txt            # LL(1) 预测分析表
│   ├── ast.json / ast.txt      # 抽象语法树
│   ├── parse_steps.txt         # 推导步骤
│   ├── ll_table_builder.exe    # 文法→分析表
│   ├── ll_parser.exe           # 运行时解析
│   ├── 代码详解_语法分析器.md   # 逐函数详细讲解
│   ├── 实验报告_语法分析器.md
│   └── README.md
├── Semantic/                   # 语义分析
│   ├── semantic_runner.exe     # 运行时分析
│   ├── semantic_report.txt     # 分析报告
│   ├── 代码详解_语义分析器.md   # 逐函数详细讲解
│   ├── 实验报告_语义分析器.md
│   └── README.md
└── Codegen/                    # 代码执行
    ├── codegen_runner.exe      # 运行时执行
    ├── execution_trace.txt     # 执行轨迹
    ├── 代码详解_代码执行器.md   # 逐函数详细讲解
    └── README.md
```

## 技术栈总结

| 项目 | 技术方案 |
|------|---------|
| **语言** | C++17 |
| **词法分析** | 正则→NFA(Thompson)→DFA(子集构造)→最小化(Hopcroft)→贪心最长匹配 |
| **语法分析** | LL(1) 预测分析，FIRST/FOLLOW 不动点迭代，Collector 栈 AST 构造，Panic Recovery |
| **语义分析** | 嵌套作用域符号表，int/float/bool/error 类型系统，表达式常量求值，use-before-def 检查 |
| **代码执行** | 树遍历解释器，Env 变量环境，内置 printf/scanf |
| **Token 类型** | 55 种 |
| **CFG 产生式** | 49 条 |
| **AST 格式** | 嵌套 JSON（`sym`/`lexeme`/`children`）+ 缩进文本 |
