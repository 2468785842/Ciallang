const path = require('node:path');
const vscode = require('vscode');
const { LanguageClient } = require('vscode-languageclient/node');
let client;

async function activate(context) {
  const settings = vscode.workspace.getConfiguration('ciallang');
  const root = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
  const platform = process.platform === 'win32' ? 'Windows' : process.platform === 'darwin' ? 'MacOS' : 'Linux';
  const binary = settings.get('executable') || (root
    ? path.join(root, 'build', platform, 'Debug', process.platform === 'win32' ? 'cial.exe' : 'cial') : 'cial');
  const python = settings.get('python') || (process.platform === 'win32' ? 'python' : 'python3');
  client = new LanguageClient('ciallang', 'Ciallang', {
    command: python,
    args: [context.asAbsolutePath('server/cial_lsp.py'), '--cial', binary]
  }, { documentSelector: [{ scheme: 'file', language: 'ciallang' }] });
  context.subscriptions.push({ dispose: () => { if (client) void client.stop(); } });
  await client.start();
}

function deactivate() { return client?.stop(); }
module.exports = { activate, deactivate };
