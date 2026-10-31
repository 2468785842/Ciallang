const fs = require('node:fs');
const path = require('node:path');

// Keep process commands as argv values: paths containing spaces must not be quoted.
function resolveCommand(command, directory) {
  if (!command) return command;
  return path.isAbsolute(command) || !/[\\/]/.test(command)
    ? command : path.resolve(directory, command);
}

function findBinary(roots, platform = process.platform) {
  const system = platform === 'win32' ? 'Windows' : platform === 'darwin' ? 'MacOS' : 'Linux';
  const executable = platform === 'win32' ? 'cial.exe' : 'cial';
  const visited = new Set();
  for (const root of roots) {
    for (let directory = path.resolve(root); !visited.has(directory); directory = path.dirname(directory)) {
      visited.add(directory);
      for (const mode of ['Debug', 'Release']) {
        const candidate = path.join(directory, 'build', system, mode, executable);
        try {
          if (fs.statSync(candidate).isFile()) return candidate;
        } catch (error) {
          if (!['ENOENT', 'ENOTDIR', 'EACCES', 'EPERM'].includes(error.code)) throw error;
        }
      }
      if (path.dirname(directory) === directory) break;
    }
  }
  return 'cial';
}

function launchOptions({ extensionPath, workspaceRoots, documentDirectory, python, executable, platform = process.platform }) {
  const roots = [...workspaceRoots];
  if (documentDirectory) roots.unshift(documentDirectory);
  // Source checkouts can run the extension without an open workspace.
  if (fs.existsSync(path.resolve(extensionPath, '../../tools/lsp/cial_lsp.py'))) roots.push(extensionPath);
  const directory = workspaceRoots[0] || documentDirectory || extensionPath;
  const server = path.join(extensionPath, 'server', 'cial_lsp.py');
  if (!fs.existsSync(server)) throw new Error('缺少语言服务脚本 server/cial_lsp.py；源码调试请先运行 npm run prepare，安装版请重新安装扩展。');
  return {
    command: resolveCommand(python || (platform === 'win32' ? 'python' : 'python3'), directory),
    args: [server, '--cial', executable ? resolveCommand(executable, directory) : findBinary(roots, platform)],
    options: { cwd: directory }
  };
}

module.exports = { findBinary, launchOptions };
