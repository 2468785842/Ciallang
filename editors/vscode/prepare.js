// F5 前的准备检查：定位 cial-lsp 语言服务器并打印路径。
// npm 安装时不下载 SDK；扩展激活时按需下载。
const path = require('node:path');
const { findServer } = require('./launch-options');

const repoRoot = path.resolve(__dirname, '../..');
const manifest = require('./package.json');
const runtimeVersion = require(path.join(repoRoot, 'vcpkg.json'))['version-string'];
if (manifest.contributes.configuration.properties['ciallang.sdk.version'].default !== runtimeVersion) {
  throw new Error('ciallang.sdk.version 默认值必须与 vcpkg.json 的发布版本一致');
}
const server = findServer([repoRoot]);
if (/[\\/]cial-lsp(\.exe)?$/.test(server)) {
  console.log(`cial-lsp: ${server}`);
} else {
  console.log(`cial-lsp 未找到（当前回落 PATH 的 "${server}"）。`);
  console.log('扩展启动时将自动下载 Cial SDK；也可设置 ciallang.sdk.path 或 ciallang.executable。');
}
