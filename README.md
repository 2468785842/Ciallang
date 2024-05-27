# VSCode
- 使用clangd插件:
  - 添加此代码到config.yaml不然会将正常代码分析为错误
  ```
  CompileFlags:
  Add: -ferror-limit=0
  ```