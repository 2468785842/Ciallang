const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const test = require('node:test');

async function activate(settings = {}, { discovered = 'cial-lsp', onPath, sdkServer = '/offline/bin/cial-lsp' } = {}) {
  const calls = { downloads: 0, starts: 0 };
  const vscode = {
    workspace: { getConfiguration: () => ({ get: (key, fallback) => settings[key] ?? fallback }) },
    window: {
      withProgress: async (options, callback) => callback({ report() {} }),
      showErrorMessage: async () => undefined
    },
    ProgressLocation: { Notification: 15 }
  };
  class LanguageClient {
    constructor(id, name, server) { calls.command = server.command; }
    async start() { calls.starts++; }
    async dispose() {}
  }
  const sandbox = { module: { exports: {} }, require(name) {
    if (name === 'vscode') return vscode;
    if (name === 'vscode-languageclient/node') return { LanguageClient };
    if (name === './launch-options') return {
      launchOptions: () => ({ command: settings.executable || discovered, options: { cwd: '/workspace' } }),
      findOnPath: () => onPath
    };
    if (name === './sdk') return {
      sdkServer: () => sdkServer,
      ensureSdk: async options => { calls.downloads++; calls.version = options.version; return '/cached/bin/cial-lsp'; }
    };
    return require(name);
  } };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../extension.js'), 'utf8'), sandbox);
  await sandbox.module.exports.activate({ extensionPath: '/extension', globalStorageUri: { fsPath: '/storage' } });
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
