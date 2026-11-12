const path = require('node:path');
const process = require('node:process');
const vscode = require('vscode');
const { LanguageClient } = require('vscode-languageclient/node');
const { launchOptions, findOnPath } = require('./launch-options');
const { ensureSdk, sdkServer, cachedSdk } = require('./sdk');
let client;

async function activate(context) {
  const document = vscode.window.activeTextEditor?.document;
  const resource = document?.uri.scheme === 'file' ? document.uri : undefined;
  const settings = vscode.workspace.getConfiguration('ciallang', resource);
  const folders = vscode.workspace.workspaceFolders || [];
  const current = resource && vscode.workspace.getWorkspaceFolder(resource);
  const roots = [...new Set([current, ...folders].filter(Boolean).map(folder => folder.uri.fsPath))];
  const development = context.extensionMode === vscode.ExtensionMode.Development || context.extensionMode === vscode.ExtensionMode.Test;
  const debugRoot = development ? process.env.CIALLANG_DEBUG_ROOT : undefined;
  const output = vscode.window.createOutputChannel('Ciallang');
  context.subscriptions.push(output);
  try {
    const server = launchOptions({
      extensionPath: context.extensionPath,
      workspaceRoots: roots,
      documentDirectory: resource ? path.dirname(resource.fsPath) : undefined,
      executable: settings.get('executable'),
      debugRoot
    });
    if (!debugRoot && !settings.get('executable')) {
      const sdkPath = settings.get('sdk.path', '');
      if (sdkPath) {
        server.command = sdkServer(path.resolve(server.options.cwd, sdkPath));
        if (!server.command) throw new Error('ciallang.sdk.path 中没有 bin/cial-lsp');
      } else if (!path.isAbsolute(server.command)) {
        const installed = findOnPath(server.command);
        if (installed) server.command = installed;
        else {
          // preview.2 的声明/索引修订使用独立缓存，避免升级插件后仍运行原始附件。
          const sdkOptions = { storagePath: path.join(context.globalStorageUri.fsPath, 'sdk', 'contracts-v1'), version: settings.get('sdk.version') };
          const cached = cachedSdk(sdkOptions);
          if (cached) server.command = cached;
          else if (settings.get('sdk.autoDownload', true)) {
            const action = await vscode.window.showInformationMessage('未找到 Cial SDK，下载后可启用补全、文档和声明跳转。', '下载 SDK', '打开设置');
            if (action !== '下载 SDK') {
              if (action === '打开设置') await vscode.commands.executeCommand('workbench.action.openSettings', '@ext:ciallang.ciallang-language');
              output.appendLine('未找到 SDK，语言服务尚未启动。');
              return;
            }
            server.command = await vscode.window.withProgress({
              location: vscode.ProgressLocation.Notification, title: 'Ciallang SDK'
            }, progress => ensureSdk({
              ...sdkOptions,
              report: message => progress.report({ message })
            }));
          }
        }
      }
    }
    output.appendLine(`语言服务器：${server.command}`);
    output.appendLine(`工作目录：${server.options.cwd}`);
    output.appendLine(`声明目录：${server.args[0] === '--declarations' ? server.args[1] : '由服务器自动发现'}`);
    const watcher = vscode.workspace.createFileSystemWatcher('**/*.[tT][jJ][sS]');
    context.subscriptions.push(watcher);
    client = new LanguageClient('ciallang', 'Ciallang', server, {
      outputChannel: output,
      documentSelector: [{ scheme: 'file', language: 'ciallang' }],
      synchronize: { fileEvents: watcher },
      initializationOptions: {
        declarationFiles: settings.get('declarationFiles', []).map(file =>
          vscode.Uri.file(path.resolve(roots[0] || (resource ? path.dirname(resource.fsPath) : context.extensionPath), file)).toString())
      }
    });
    output.appendLine('正在启动语言服务并准备工作区索引…');
    await client.start();
    const status = await client.sendRequest('ciallang/indexStatus').catch(() => undefined);
    if (typeof status?.files === 'number') output.appendLine(`工作区索引已就绪：${status.files} 个文件。`);
    else output.appendLine('语言服务已启动；此服务器未提供索引状态。');
  } catch (error) {
    output.appendLine(`启动失败：${error.message}`);
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
