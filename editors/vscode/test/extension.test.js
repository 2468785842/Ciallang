const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const test = require('node:test');

async function activate(settings = {}, { discovered = 'cial-lsp', onPath, sdkServer = '/offline/bin/cial-lsp', debugRoot, development = false, testing = false, prompt = "下载 SDK", cached } = {}) {
  const calls = { downloads: 0, starts: 0, logs: [] };
  const vscode = {
    workspace: { getConfiguration: () => ({ get: (key, fallback) => settings[key] ?? fallback }), createFileSystemWatcher: () => ({ dispose() {} }) },
    window: {
      withProgress: async (options, callback) => callback({ report() {} }),
      showErrorMessage: async () => undefined,
      showInformationMessage: async () => { calls.prompted = true; return prompt; },
      createOutputChannel: () => ({ appendLine: line => calls.logs.push(line), dispose() {} })
    },
    ExtensionMode: { Production: 1, Development: 2, Test: 3 },
    ProgressLocation: { Notification: 15 }
  };
  class LanguageClient {
    constructor(id, name, server) { calls.command = server.command; }
    async start() { calls.starts++; }
    async sendRequest() { return { files: 3 }; }
    async dispose() {}
  }
  const sandbox = { module: { exports: {} }, require(name) {
    if (name === 'vscode') return vscode;
    if (name === 'node:process') return { env: { CIALLANG_DEBUG_ROOT: debugRoot } };
    if (name === 'vscode-languageclient/node') return { LanguageClient };
    if (name === './launch-options') return {
      launchOptions: options => {
        calls.debugRoot = options.debugRoot;
        return { command: options.debugRoot ? '/debug/cial-lsp' : settings.executable || discovered, args: [], options: { cwd: '/workspace' } };
      },
      findOnPath: () => onPath
    };
    if (name === './sdk') return {
      sdkServer: () => sdkServer,
      cachedSdk: () => cached,
      ensureSdk: async options => { calls.downloads++; calls.version = options.version; return '/cached/bin/cial-lsp'; }
    };
    return require(name);
  } };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../extension.js'), 'utf8'), sandbox);
  await sandbox.module.exports.activate({ extensionPath: '/extension', globalStorageUri: { fsPath: '/storage' }, subscriptions: [], extensionMode: testing ? 3 : development ? 2 : 1 });
  return calls;
}

test('activation downloads the SDK and launches its server when local discovery fails', async () => {
  const calls = await activate({ 'sdk.version': '1.2.3' });
  assert.equal(calls.downloads, 1);
  assert.equal(calls.version, '1.2.3');
  assert.equal(calls.command, '/cached/bin/cial-lsp');
  assert.equal(calls.starts, 1);
});

test('configured executable, offline SDK, local builds and PATH avoid downloading', async () => {
  for (const [settings, discovery, expected] of [
    [{ executable: '/custom/lsp' }, {}, '/custom/lsp'],
    [{ 'sdk.path': '/offline' }, {}, '/offline/bin/cial-lsp'],
    [{}, { discovered: '/build/cial-lsp' }, '/build/cial-lsp'],
    [{}, { onPath: '/system/cial-lsp' }, '/system/cial-lsp']
  ]) {
    const calls = await activate(settings, discovery);
    assert.equal(calls.downloads, 0);
    assert.equal(calls.command, expected);
  }
});

test('disabling downloads preserves the PATH fallback', async () => {
  const calls = await activate({ 'sdk.autoDownload': false });
  assert.equal(calls.downloads, 0);
  assert.equal(calls.command, 'cial-lsp');
});

test('an invalid explicit SDK reports failure instead of downloading another server', async () => {
  await assert.rejects(activate({ 'sdk.path': '/missing' }, { sdkServer: null }), /没有 bin/);
});

test('F5 debug root overrides saved SDK settings and is logged', async () => {
  const calls = await activate({ executable: '/old/lsp', 'sdk.path': '/old/sdk' }, { debugRoot: '/checkout', development: true });
  assert.equal(calls.debugRoot, '/checkout');
  assert.equal(calls.command, '/debug/cial-lsp');
  assert.equal(calls.downloads, 0);
  assert(calls.logs.some(line => line.includes('/debug/cial-lsp')));
});

test('installed extensions ignore the development-only environment override', async () => {
  const calls = await activate({ executable: '/custom/lsp' }, { debugRoot: '/checkout' });
  assert.equal(calls.debugRoot, undefined);
  assert.equal(calls.command, '/custom/lsp');
});

test('extension tests use the same Debug binding as the development host', async () => {
  const calls = await activate({ executable: '/old/lsp' }, { debugRoot: '/checkout', testing: true });
  assert.equal(calls.command, '/debug/cial-lsp');
});


test('missing SDK notification can be dismissed without downloading or launching', async () => {
  const calls = await activate({}, { prompt: null });
  assert.equal(calls.downloads, 0);
  assert.equal(calls.starts, 0);
});

test('valid cached SDK launches offline without prompting', async () => {
  const calls = await activate({ 'sdk.autoDownload': false }, { cached: '/cached/cial-lsp' });
  assert.equal(calls.command, '/cached/cial-lsp');
  assert.equal(calls.prompted, undefined);
  assert.equal(calls.downloads, 0);
});
