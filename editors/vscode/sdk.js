const fs = require('node:fs');
const path = require('node:path');
const https = require('node:https');
const crypto = require('node:crypto');
const { unzipSync } = require('fflate');
const tar = require('tar');

const releaseBase = 'https://github.com/2468785842/Ciallang/releases/download';

function packageIdentity(version, platform = process.platform, arch = process.arch) {
  if (!/^\d+\.\d+\.\d+(?:-[\da-zA-Z.-]+)?$/.test(version)) throw new Error('无效的 SDK 版本');
  const system = { win32: 'windows', linux: 'linux', darwin: 'macos' }[platform];
  if (!system || !['x64', 'arm64'].includes(arch)) throw new Error(`SDK 不支持 ${platform}/${arch}`);
  const name = `ciallang-${version}-${system}-${arch}`;
  return { name, archive: `${name}.${platform === 'win32' ? 'zip' : 'tar.gz'}`, platform, arch, version };
}

function download(url, limit = 256 * 1024 * 1024, redirects = 5) {
  return new Promise((resolve, reject) => {
    if (new URL(url).protocol !== 'https:') return reject(new Error('SDK 下载必须使用 HTTPS'));
    const request = https.get(url, { headers: { 'User-Agent': 'ciallang-vscode' } }, response => {
      if ([301, 302, 303, 307, 308].includes(response.statusCode)) {
        response.resume();
        if (!redirects || !response.headers.location) return reject(new Error('SDK 下载重定向过多'));
        return resolve(download(new URL(response.headers.location, url).href, limit, redirects - 1));
      }
      if (response.statusCode !== 200) {
        response.resume();
        return reject(new Error(`SDK 下载失败：HTTP ${response.statusCode} (${url})`));
      }
      let size = 0;
      const chunks = [];
      response.on('data', chunk => {
        size += chunk.length;
        if (size > limit) response.destroy(new Error('SDK 下载超过大小上限'));
        else chunks.push(chunk);
      });
      response.on('error', reject);
      response.on('end', () => resolve(Buffer.concat(chunks)));
    });
    request.setTimeout(60000, () => request.destroy(new Error('SDK 下载超时')));
    request.on('error', reject);
  });
}

function verifyChecksum(bytes, checksum, filename) {
  const match = checksum.trim().match(/^([a-fA-F0-9]{64})\s+\*?(.+)$/);
  if (!match || match[2] !== filename || crypto.createHash('sha256').update(bytes).digest('hex') !== match[1].toLowerCase()) {
    throw new Error('SDK SHA-256 校验失败');
  }
}

function safeMember(name, rootName) {
  if (name.includes('\\') || name.includes('\0') || name.startsWith('/') || /^[a-z]:/i.test(name) ||
      name.split('/').includes('..') || name.split('/')[0] !== rootName) {
    throw new Error(`不安全的 SDK 归档路径：${name}`);
  }
}

async function extract(bytes, destination, identity) {
  if (identity.platform === 'win32') {
    let size = 0;
    const files = unzipSync(bytes, { filter(entry) {
      safeMember(entry.name, identity.name);
      size += entry.originalSize;
      if (size > 512 * 1024 * 1024) throw new Error('SDK 解压超过大小上限');
      return true;
    } });
    for (const [name, data] of Object.entries(files)) {
      const target = path.join(destination, name);
      if (name.endsWith('/')) fs.mkdirSync(target, { recursive: true });
      else {
        fs.mkdirSync(path.dirname(target), { recursive: true });
        fs.writeFileSync(target, data);
      }
    }
  } else {
    const archive = path.join(destination, 'sdk.tar.gz');
    fs.writeFileSync(archive, bytes);
    let size = 0;
    tar.x({ file: archive, cwd: destination, sync: true, strict: true, filter(name, entry) {
      safeMember(name, identity.name);
      if (!['File', 'Directory'].includes(entry.type)) throw new Error('SDK 归档不允许链接或特殊文件');
      size += entry.size || 0;
      if (size > 512 * 1024 * 1024) throw new Error('SDK 解压超过大小上限');
      return true;
    } });
  }
}

function sdkServer(directory, platform = process.platform) {
  const executable = path.join(directory, 'bin', platform === 'win32' ? 'cial-lsp.exe' : 'cial-lsp');
  return fs.statSync(executable, { throwIfNoEntry: false })?.isFile() ? executable : undefined;
}

function validateSdk(directory, identity) {
  const server = sdkServer(directory, identity.platform);
  const info = JSON.parse(fs.readFileSync(path.join(directory, 'build-info.json'), 'utf8'));
  const system = { win32: 'Windows', linux: 'Linux', darwin: 'Darwin' }[identity.platform];
  if (!server || info.version !== identity.version || info.system !== system || info.architecture !== identity.arch ||
      !fs.statSync(path.join(directory, 'share/ciallang/declarations/Array.d.tjs'), { throwIfNoEntry: false })?.isFile()) {
    throw new Error('SDK 不完整或平台/版本不匹配（需要 cial-lsp 与系统声明）');
  }
  if (identity.platform !== 'win32') fs.chmodSync(server, 0o755);
  return server;
}

const installations = new Map();
async function ensureSdk({ storagePath, version, platform = process.platform, arch = process.arch, fetch = download, report = () => {} }) {
  const identity = packageIdentity(version, platform, arch);
  const destination = path.join(storagePath, identity.name);
  if (installations.has(destination)) return installations.get(destination);
  const install = async () => {
    try { return validateSdk(destination, identity); } catch { /* 缺失或损坏的缓存重新安装。 */ }
    fs.mkdirSync(storagePath, { recursive: true });
    const temporary = fs.mkdtempSync(path.join(storagePath, '.download-'));
    try {
      report(`下载 Cial SDK ${version} (${platform}/${arch})`);
      const url = `${releaseBase}/v${version}/${identity.archive}`;
      const checksum = await fetch(`${url}.sha256`, 4096);
      const bytes = await fetch(url);
      verifyChecksum(bytes, checksum.toString('utf8'), identity.archive);
      report('校验并解压 Cial SDK');
      await extract(bytes, temporary, identity);
      const unpacked = path.join(temporary, identity.name);
      validateSdk(unpacked, identity);
      // 只在完整下载、校验和布局验证后替换缓存。
      fs.rmSync(destination, { recursive: true, force: true });
      fs.renameSync(unpacked, destination);
      return validateSdk(destination, identity);
    } finally {
      fs.rmSync(temporary, { recursive: true, force: true });
    }
  };
  const pending = install();
  installations.set(destination, pending);
  try { return await pending; } finally { installations.delete(destination); }
}

module.exports = { packageIdentity, verifyChecksum, safeMember, extract, sdkServer, validateSdk, ensureSdk };
