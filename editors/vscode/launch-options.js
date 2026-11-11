const fs = require('node:fs');
const path = require('node:path');

// Keep process commands as argv values: paths containing spaces must not be quoted.
function resolveCommand(command, directory) {
  if (!command) return command;
  return path.isAbsolute(command) || !/[\\/]/.test(command)
    ? command : path.resolve(directory, command);
}

// cial-lsp 服务器发现：构建树可能把工具 exe 放在 tools/lsp 子目录。
function findServer(roots, platform = process.platform) {
  const system = platform === 'win32' ? 'Windows' : platform === 'darwin' ? 'MacOS' : 'Linux';
  const name = platform === 'win32' ? 'cial-lsp.exe' : 'cial-lsp';
  const visited = new Set();
  for (const root of roots) {
    for (let directory = path.resolve(root); !visited.has(directory); directory = path.dirname(directory)) {
      visited.add(directory);
      for (const mode of ['Debug', 'Release']) {
        for (const relative of [path.join('build', system, mode, name), path.join('build', system, mode, 'tools', 'lsp', name)]) {
          const candidate = path.join(directory, relative);
          try {
            if (fs.statSync(candidate).isFile()) return candidate;
          } catch (error) {
            if (!['ENOENT', 'ENOTDIR', 'EACCES', 'EPERM'].includes(error.code)) throw error;
          }
        }
      }
      if (path.dirname(directory) === directory) break;
    }
  }
  return name; // 最后回落 PATH
}

function launchOptions({ extensionPath, workspaceRoots, documentDirectory, executable, platform = process.platform }) {
  const roots = [...workspaceRoots];
  if (documentDirectory) roots.unshift(documentDirectory);
  // Source checkouts can run the extension without an open workspace.
  if (fs.existsSync(path.resolve(extensionPath, '../../tools/lsp/main.cpp'))) roots.push(extensionPath);
  const directory = workspaceRoots[0] || documentDirectory || extensionPath;
  const command = executable ? resolveCommand(executable, directory) : findServer(roots, platform);
  return {
    command,
    args: [],
    options: { cwd: directory }
  };
}

module.exports = { findServer, launchOptions };
