const fs = require('node:fs');
const path = require('node:path');
fs.mkdirSync(path.join(__dirname, 'server'), { recursive: true });
for (const name of ['cial_lsp.py', 'decl_index.py', 'script-builtins.d.tjs']) {
  fs.copyFileSync(path.join(__dirname, '..', '..', 'tools', 'lsp', name), path.join(__dirname, 'server', name));
}
