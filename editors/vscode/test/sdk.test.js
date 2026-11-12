const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const crypto = require('node:crypto');
const test = require('node:test');
const { zipSync, strToU8 } = require('fflate');
const tar = require('tar');
const { packageIdentity, verifyChecksum, safeMember, ensureSdk, validateSdk } = require('../sdk');

function fixture(t) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'cial sdk 中文 '));
  t.after(() => fs.rmSync(directory, { recursive: true, force: true }));
  return directory;
}

function sdkFiles(identity) {
  const system = { win32: 'Windows', linux: 'Linux', darwin: 'Darwin' }[identity.platform];
  return {
    [`${identity.name}/bin/cial-lsp${identity.platform === 'win32' ? '.exe' : ''}`]: strToU8('server'),
    [`${identity.name}/build-info.json`]: strToU8(JSON.stringify({ version: identity.version, system, architecture: identity.arch })),
    [`${identity.name}/share/ciallang/declarations/Array.d.tjs`]: strToU8('class Array {}')
  };
}

function downloader(bytes, identity, requests) {
  return async url => {
    requests.push(url);
    return url.endsWith('.sha256')
      ? Buffer.from(`${crypto.createHash('sha256').update(bytes).digest('hex')}  ${identity.archive}\n`)
      : bytes;
  };
}

test('release names match the native package matrix and reject unsupported targets', () => {
  assert.equal(packageIdentity('0.1.0-preview.2', 'win32', 'x64').archive, 'ciallang-0.1.0-preview.2-windows-x64.zip');
  assert.equal(packageIdentity('1.0.0', 'darwin', 'arm64').archive, 'ciallang-1.0.0-macos-arm64.tar.gz');
  assert.throws(() => packageIdentity('../../bad', 'linux', 'x64'));
  assert.throws(() => packageIdentity('1.0.0', 'win32', 'ia32'));
});

test('checksum binds the exact archive bytes and filename', () => {
  const bytes = Buffer.from('sdk');
  const sum = crypto.createHash('sha256').update(bytes).digest('hex');
  verifyChecksum(bytes, `${sum}  sdk.zip\r\n`, 'sdk.zip');
  assert.throws(() => verifyChecksum(Buffer.from('corrupt'), `${sum}  sdk.zip`, 'sdk.zip'));
  assert.throws(() => verifyChecksum(bytes, `${sum}  other.zip`, 'sdk.zip'));
});

test('archive paths stay inside the expected SDK root', () => {
  for (const name of ['../escape', '/absolute', 'C:/drive', 'sdk/../escape', 'sdk\\escape', 'other/bin/lsp']) {
    assert.throws(() => safeMember(name, 'sdk'));
  }
  safeMember('sdk/bin/cial-lsp', 'sdk');
});

test('installs ZIP, reuses cache offline and repairs incomplete installations', async t => {
  const storagePath = fixture(t);
  const identity = packageIdentity('1.2.3', 'win32', 'x64');
  const bytes = Buffer.from(zipSync(sdkFiles(identity)));
  const requests = [];
  const options = { storagePath, version: identity.version, platform: 'win32', arch: 'x64', fetch: downloader(bytes, identity, requests) };
  const server = await ensureSdk(options);
  assert.equal(server, path.join(storagePath, identity.name, 'bin/cial-lsp.exe'));
  assert.equal(requests.length, 2);
  assert.equal(await ensureSdk({ ...options, fetch: () => { throw new Error('offline'); } }), server);
  fs.unlinkSync(server);
  assert.equal(await ensureSdk(options), server);
  assert.equal(requests.length, 4);
});

test('bad checksum and missing server never leave a partial cache; retry succeeds', async t => {
  const storagePath = fixture(t);
  const identity = packageIdentity('1.2.3', 'win32', 'x64');
  const files = sdkFiles(identity);
  delete files[`${identity.name}/bin/cial-lsp.exe`];
  const incomplete = Buffer.from(zipSync(files));
  const options = { storagePath, version: '1.2.3', platform: 'win32', arch: 'x64' };
  await assert.rejects(ensureSdk({ ...options, fetch: downloader(incomplete, identity, []) }), /不完整/);
  assert.deepEqual(fs.readdirSync(storagePath), []);
  await assert.rejects(ensureSdk({ ...options, fetch: async () => Buffer.from('bad') }), /SHA-256/);
  assert.deepEqual(fs.readdirSync(storagePath), []);
  const bytes = Buffer.from(zipSync(sdkFiles(identity)));
  await ensureSdk({ ...options, fetch: downloader(bytes, identity, []) });
});

test('extracts real tar.gz SDK and rejects symbolic links', async t => {
  const directory = fixture(t);
  const identity = packageIdentity('1.2.3', 'linux', 'x64');
  const source = path.join(directory, 'source');
  for (const [name, data] of Object.entries(sdkFiles(identity))) {
    const target = path.join(source, name);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.writeFileSync(target, data);
  }
  const archive = path.join(directory, 'sdk.tar.gz');
  await tar.c({ gzip: true, cwd: source, file: archive }, [identity.name]);
  const bytes = fs.readFileSync(archive);
  const storagePath = path.join(directory, 'cache');
  const server = await ensureSdk({ storagePath, version: '1.2.3', platform: 'linux', arch: 'x64', fetch: downloader(bytes, identity, []) });
  assert.equal(validateSdk(path.dirname(path.dirname(server)), identity), server);
  // 以 tar 写入合成链接条目，无需 Windows 的符号链接权限。
  const header = new tar.Header({ path: `${identity.name}/link`, type: 'SymbolicLink', linkpath: '../../escape', size: 0 });
  header.encode();
  const zlib = require('node:zlib');
  const linked = zlib.gzipSync(Buffer.concat([header.block, Buffer.alloc(1024)]));
  await assert.rejects(ensureSdk({ storagePath: path.join(directory, 'links'), version: '1.2.3', platform: 'linux', arch: 'x64', fetch: downloader(linked, identity, []) }), /链接/);
});
