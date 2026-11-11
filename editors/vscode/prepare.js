// F5 前的准备检查：定位 cial-lsp 语言服务器并打印路径。
// 服务器可执行文件不在扩展内，而是随源码构建（CIAL_BUILD_LSP）；缺失时提示
// 构建方式但不失败——npm prepare 是生命周期脚本，npm install 时不应报错。
const path = require('node:path');
const { findServer } = require('./launch-options');

const repoRoot = path.resolve(__dirname, '../..');
const server = findServer([repoRoot]);
if (/[\\/]cial-lsp(\.exe)?$/.test(server)) {
  console.log(`cial-lsp: ${server}`);
} else {
  console.log(`cial-lsp 未找到（当前回落 PATH 的 "${server}"）。`);
  console.log('请先构建：cmake --preset "Windows Debug"（预设开启 CIAL_BUILD_LSP），');
  console.log('或在 VSCode 设置中将 ciallang.executable 指向已构建的 cial-lsp。');
}
