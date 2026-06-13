# 一键编译流水线：从源码到执行
# 用法：.\build_and_run.ps1

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

Write-Host "=== 1/4 词法分析 (Lexer) ===" -ForegroundColor Cyan
Push-Location "$root\Lexer"
& .\lexer_runner.exe *>&1 | Select-Object -Last 1
Pop-Location

Write-Host "=== 2/4 语法分析 (Parser) ===" -ForegroundColor Cyan
Push-Location "$root\Parser"
& .\ll_parser.exe *>&1 | Select-Object -Last 1
Pop-Location

Write-Host "=== 3/4 语义分析 (Semantic) ===" -ForegroundColor Cyan
Push-Location "$root\Semantic"
& .\semantic_runner.exe 2>&1
Pop-Location

Write-Host "=== 4/4 代码执行 (Codegen) ===" -ForegroundColor Cyan
Push-Location "$root\Codegen"
& .\codegen_runner.exe 2>&1
Pop-Location

Write-Host "`n完成." -ForegroundColor Green
