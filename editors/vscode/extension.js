const path = require('node:path');
const vscode = require('vscode');
const { LanguageClient } = require('vscode-languageclient/node');
const { launchOptions } = require('./launch-options');
let client;

async function activate(context) {
  const document = vscode.window.activeTextEditor?.document;
  const resource = document?.uri.scheme === 'file' ? document.uri : undefined;
  const settings = vscode.workspace.getConfiguration('ciallang', resource);
  const folders = vscode.workspace.workspaceFolders || [];
  const current = resource && vscode.workspace.getWorkspaceFolder(resource);
  const roots = [...new Set([current, ...folders].filter(Boolean).map(folder => folder.uri.fsPath))];
  try {
    const server = launchOptions({
      extensionPath: context.extensionPath,
      workspaceRoots: roots,
      documentDirectory: resource ? path.dirname(resource.fsPath) : undefined,
      python: settings.get('python'),
      executable: settings.get('executable')
    });
    client = new LanguageClient('ciallang', 'Ciallang', server, {
      documentSelector: [{ scheme: 'file', language: 'ciallang' }]
    });
    await client.start();
  } catch (error) {
    await client?.dispose();
    client = undefined;
    void vscode.window.showErrorMessage(
      `Ciallang 语言服务启动失败：${error.message}。请检查 Python 3 和 ciallang.executable 配置。`, '打开设置').then(action => {
      if (action === '打开设置') return vscode.commands.executeCommand('workbench.action.openSettings', '@ext:ciallang.ciallang-language');
    });
    throw error;
  }
}

async function deactivate() {
  const running = client;
  client = undefined;
  await running?.dispose();
}
module.exports = { activate, deactivate };
