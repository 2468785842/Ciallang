const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');
const { findServer, launchOptions } = require('../launch-options');

function fixture(t) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'cial editor 中文 '));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  return root;
}
function touch(file) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, '');
  return file;
}

test('discovers Debug build when opening a nested script directory', t => {
  const root = fixture(t);
  const exe = touch(path.join(root, 'build/Windows/Debug/cial-lsp.exe'));
  assert.equal(findServer([path.join(root, 'tests/tjs')], 'win32'), exe);
});

test('finds tool-exe layouts inside build/<platform>/<mode>/tools/lsp', t => {
  const root = fixture(t);
  const exe = touch(path.join(root, 'build/Windows/Debug/tools/lsp/cial-lsp.exe'));
  assert.equal(findServer([root], 'win32'), exe);
});

test('uses Release when Debug is absent, prefers Debug when both exist', t => {
  const root = fixture(t);
  const release = touch(path.join(root, 'build/Linux/Release/cial-lsp'));
  assert.equal(findServer([root], 'linux'), release);
  const debug = touch(path.join(root, 'build/Linux/Debug/cial-lsp'));
  assert.equal(findServer([root], 'linux'), debug);
});

test('checks other workspace folders and macOS paths', t => {
  const root = fixture(t);
  const exe = touch(path.join(root, 'second/build/MacOS/Release/cial-lsp'));
  assert.equal(findServer([path.join(root, 'first'), path.join(root, 'second')], 'darwin'), exe);
});

test('rejects a directory masquerading as an executable and falls back to PATH', t => {
  const root = fixture(t);
  fs.mkdirSync(path.join(root, 'build/Windows/Debug/cial-lsp.exe'), { recursive: true });
  assert.equal(findServer([root], 'win32'), 'cial-lsp.exe');
  assert.equal(findServer([root], 'linux'), 'cial-lsp');
});

test('keeps paths with spaces as single arguments and resolves configured relative paths', t => {
  const root = fixture(t);
  const options = launchOptions({ extensionPath: root, workspaceRoots: [root], executable: './binary dir/cial-lsp.exe' });
  assert.equal(options.command, path.join(root, 'binary dir/cial-lsp.exe'));
  assert.deepEqual(options.args, []);
  assert.equal(options.options.cwd, root);
});

test('source checkout without workspace locates its own build', t => {
  const root = fixture(t);
  const extensionPath = path.join(root, 'editors/vscode');
  touch(path.join(root, 'tools/lsp/main.cpp'));
  const exe = touch(path.join(root, 'build/Windows/Debug/tools/lsp/cial-lsp.exe'));
  const options = launchOptions({ extensionPath, workspaceRoots: [], platform: 'win32' });
  assert.equal(options.command, exe);
});

test('F5 binds Debug executable and declarations despite a different workspace and configured executable', t => {
  const root = fixture(t);
  const exe = touch(path.join(root, 'build/Windows/Debug/tools/lsp/cial-lsp.exe'));
  touch(path.join(root, 'build/Windows/Debug/declarations/Array.d.tjs'));
  const directory = path.join(root, 'other-workspace');
  const options = launchOptions({ extensionPath: root, workspaceRoots: [directory], executable: '/old/lsp', debugRoot: root, platform: 'win32' });
  assert.equal(options.command, exe);
  assert.deepEqual(options.args, ['--declarations', path.join(root, 'build/Windows/Debug/declarations')]);
  assert.equal(options.options.cwd, directory);
});

test('F5 never falls back to Release or SDK when Debug server or declarations are missing', t => {
  const root = fixture(t);
  touch(path.join(root, 'build/Linux/Release/cial-lsp'));
  const options = { extensionPath: root, workspaceRoots: [], debugRoot: root, platform: 'linux' };
  assert.throws(() => launchOptions(options), /Debug/);
  const exe = touch(path.join(root, 'build/Linux/Debug/cial-lsp'));
  assert.throws(() => launchOptions(options), /Debug/);
  touch(path.join(root, 'build/Linux/Debug/declarations/Array.d.tjs'));
  assert.equal(launchOptions(options).command, exe);
});
