const fs = require('node:fs');
const path = require('node:path');

// Keep process commands as argv values: paths containing spaces must not be quoted.
function resolveCommand(command, directory) {
  if (!command) return command;
  return path.isAbsolute(command) || !/[\\/]/.test(command)
    ? command : path.resolve(directory, command);
}

function findOnPath(command, platform = process.platform) {
  for (const directory of (process.env.PATH || '').split(path.delimiter)) {
    if (!directory) continue;
    const candidate = path.join(directory.replace(/^"|"$/g, ''), command);
    try {
      fs.accessSync(candidate, platform === 'win32' ? fs.constants.F_OK : fs.constants.X_OK);
      if (fs.statSync(candidate).isFile()) return candidate;
    } catch { /* 继续检查 PATH。 */ }
  }
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

function debugLaunchOptions(root, directory, platform = process.platform) {
  const system = platform === 'win32' ? 'Windows' : platform === 'darwin' ? 'MacOS' : 'Linux';
  const name = platform === 'win32' ? 'cial-lsp.exe' : 'cial-lsp';
  const build = path.resolve(root, 'build', system, 'Debug');
  const candidates = [path.join(build, 'tools/lsp', name), path.join(build, name)];
  const command = candidates.find(file => fs.statSync(file, { throwIfNoEntry: false })?.isFile());
  const declarations = path.join(build, 'declarations');
  if (!command || !fs.statSync(path.join(declarations, 'Array.d.tjs'), { throwIfNoEntry: false })?.isFile()) {
    throw new Error(`Debug 服务器或系统声明缺失，请先构建 cial 和 cial-lsp：${build}`);
  }
  return { command, args: ['--declarations', declarations], options: { cwd: directory } };
}

function launchOptions({ extensionPath, workspaceRoots, documentDirectory, executable, debugRoot, platform = process.platform }) {
  const roots = [...workspaceRoots];
  if (documentDirectory) roots.unshift(documentDirectory);
  // Source checkouts can run the extension without an open workspace.
  if (fs.existsSync(path.resolve(extensionPath, '../../tools/lsp/main.cpp'))) roots.push(extensionPath);
  const directory = workspaceRoots[0] || documentDirectory || extensionPath;
  if (debugRoot) return debugLaunchOptions(debugRoot, directory, platform);
  const command = executable ? resolveCommand(executable, directory) : findServer(roots, platform);
  return {
    command,
    args: [],
    options: { cwd: directory }
  };
}

module.exports = { findServer, findOnPath, debugLaunchOptions, launchOptions };
