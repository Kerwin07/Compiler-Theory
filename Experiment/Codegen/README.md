# Codegen — 代码执行器（树遍历解释器）

在 Parser 输出的 `ast.json` 基础上，通过递归遍历 AST 直接执行源程序。

## 程序

| 程序 | 输入 | 输出 |
|------|------|------|
| `codegen_runner.exe` | `ast.json`（来自 Parser） | 终端输出 + `execution_trace.txt` |

## 执行特性

| 特性 | 说明 |
|------|------|
| **解释方式** | 树遍历解释器（Tree-walking Interpreter），递归遍历 AST 执行 |
| **变量环境** | `Env`: `map<string, int>`，单一全局作用域 |
| **表达式求值** | 5 层递归求值器，遵循 AST 编码的优先级 |
| **控制流** | if-else 按条件分支、while 真正循环执行、return 传播退出 |
| **函数调用** | 内置 printf（输出）和 scanf（输入）|
| **类型声明** | `int x = expr;` 和 `int x;` 两种形式 |
| **return 传播** | 通过 `ExecResult.hasReturn` 标记沿调用链传播 |

## 支持的语句

| 语句类型 | 示例 | 说明 |
|---------|------|------|
| 空语句 | `;` | 跳过 |
| 类型声明 | `int x = 5;` | 声明变量并初始化 |
| 赋值 | `x = 10;` | 修改已有变量 |
| 函数调用 | `printf(x);` `printf("text", val);` | 内置输出函数 |
| 输入 | `x = scanf();` | 从控制台读取整数 |
| 条件 | `if (x == 5) { ... } else { ... }` | 就近匹配 else |
| 循环 | `while (i < 3) { ... }` | 每次迭代前重新求值条件 |
| 返回 | `return 0;` | 退出程序并返回值 |
| 块 | `{ int x = 1; }` | 嵌套作用域（变量统一管理在 Env） |

## 运算符支持

| 运算符 | 实现方式 |
|--------|---------|
| `^` | 循环累乘（右结合） |
| `-` (一元) | 直接取负 |
| `*` `/` | 整数运算（除零抛异常） |
| `+` `-` | 整数运算 |
| `==` `!=` `<` `>` `<=` `>=` | 整数比较，返回 1/0 |

## 执行轨迹示例

**输入**：`int x = 5; int y = 10; int z = x + y; printf(z); return 0;`

**终端输出**：
```
15
Program returned: 0
```

**execution_trace.txt**：
```
=== Execution Trace ===
  int x = 5
  int y = 10
  int z = 15
  call printf
  RETURN 0
Program returned: 0
```

## 与语义分析器的区别

| 维度 | 语义分析器 | 代码执行器 |
|------|-----------|-----------|
| 目的 | 静态检查 + 诊断报告 | 实际执行程序 |
| 循环处理 | 只走一遍 | 真正循环执行 |
| 分支处理 | 两条分支都分析 | 只走条件为真的路径 |
| 函数调用 | 跳过 | 实际输出/输入 |
| 类型系统 | int/float/bool/error | 仅 int |
| 幂运算 | std::pow（double） | 循环累乘 |
| 输出 | semantic_report.txt | execution_trace.txt + 控制台 |

## 编译

```bash
cd Codegen
g++ -std=c++17 -O2 codegen.cpp codegen_runner.cpp -o codegen_runner.exe
```

## 单独运行

```bash
cd Codegen
.\codegen_runner.exe           # 普通运行
echo 42 | .\codegen_runner.exe # scanf 从管道读入
```
