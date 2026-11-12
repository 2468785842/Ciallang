const path = require('node:path');
const vscode = require('vscode');
const { LanguageClient } = require('vscode-languageclient/node');
const { launchOptions, findOnPath } = require('./launch-options');
const { ensureSdk, sdkServer } = require('./sdk');
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
      executable: settings.get('executable')
    });
    if (!settings.get('executable')) {
      const sdkPath = settings.get('sdk.path', '');
      if (sdkPath) {
        server.command = sdkServer(path.resolve(server.options.cwd, sdkPath));
        if (!server.command) throw new Error('ciallang.sdk.path 中没有 bin/cial-lsp');
      } else if (!path.isAbsolute(server.command)) {
        const installed = findOnPath(server.command);
        if (installed) server.command = installed;
        else if (settings.get('sdk.autoDownload', true)) {
          server.command = await vscode.window.withProgress({
            location: vscode.ProgressLocation.Notification, title: 'Ciallang SDK'
          }, progress => ensureSdk({
            storagePath: path.join(context.globalStorageUri.fsPath, 'sdk'),
            version: settings.get('sdk.version'),
            report: message => progress.report({ message })
          }));
        }
      }
    }
    client = new LanguageClient('ciallang', 'Ciallang', server, {
      documentSelector: [{ scheme: 'file', language: 'ciallang' }],
      initializationOptions: {
        declarationFiles: settings.get('declarationFiles', []).map(file =>
          vscode.Uri.file(path.resolve(roots[0] || (resource ? path.dirname(resource.fsPath) : context.extensionPath), file)).toString())
      }
    });
    await client.start();
  } catch (error) {
    await client?.dispose();
    client = undefined;
    void vscode.window.showErrorMessage(
      `Ciallang 语言服务启动失败：${error.message}。请检查网络与 SDK 版本，或设置 ciallang.sdk.path / ciallang.executable。`, '打开设置').then(action => {
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
