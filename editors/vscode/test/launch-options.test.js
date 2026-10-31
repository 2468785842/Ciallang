const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');
const { findBinary, launchOptions } = require('../launch-options');

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
  const exe = touch(path.join(root, 'build/Windows/Debug/cial.exe'));
  assert.equal(findBinary([path.join(root, 'tests/tjs')], 'win32'), exe);
});

test('uses Release when Debug is absent, prefers Debug when both exist', t => {
  const root = fixture(t);
  const release = touch(path.join(root, 'build/Linux/Release/cial'));
  assert.equal(findBinary([root], 'linux'), release);
  const debug = touch(path.join(root, 'build/Linux/Debug/cial'));
  assert.equal(findBinary([root], 'linux'), debug);
});

test('checks other workspace folders and macOS paths', t => {
  const root = fixture(t);
  const exe = touch(path.join(root, 'second/build/MacOS/Release/cial'));
  assert.equal(findBinary([path.join(root, 'first'), path.join(root, 'second')], 'darwin'), exe);
});

test('rejects a directory masquerading as an executable and falls back to PATH', t => {
  const root = fixture(t);
  fs.mkdirSync(path.join(root, 'build/Windows/Debug/cial.exe'), { recursive: true });
  assert.equal(findBinary([root], 'win32'), 'cial');
});

test('keeps paths with spaces as single arguments and resolves configured relative paths', t => {
  const root = fixture(t);
  const extensionPath = path.join(root, 'extension');
  const server = touch(path.join(extensionPath, 'server/cial_lsp.py'));
  const options = launchOptions({extensionPath, workspaceRoots: [root], python: './python dir/python.exe', executable: './binary dir/cial.exe'});
  assert.equal(options.command, path.join(root, 'python dir/python.exe'));
  assert.deepEqual(options.args, [server, '--cial', path.join(root, 'binary dir/cial.exe')]);
  assert.equal(options.options.cwd, root);
});

test('source checkout without workspace locates its own build', t => {
  const root = fixture(t);
  const extensionPath = path.join(root, 'editors/vscode');
  touch(path.join(extensionPath, 'server/cial_lsp.py'));
  touch(path.join(root, 'tools/lsp/cial_lsp.py'));
  const exe = touch(path.join(root, 'build/Windows/Debug/cial.exe'));
  assert.equal(launchOptions({extensionPath, workspaceRoots: [], platform: 'win32'}).args[2], exe);
});

test('missing bundled server reports a preparation error', t => {
  const root = fixture(t);
  assert.throws(() => launchOptions({extensionPath: root, workspaceRoots: []}), /npm run prepare/);
});
