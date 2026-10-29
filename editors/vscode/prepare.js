const fs = require('node:fs');
const path = require('node:path');
fs.mkdirSync(path.join(__dirname, 'server'), { recursive: true });
fs.copyFileSync(path.join(__dirname, '..', '..', 'tools', 'lsp', 'cial_lsp.py'), path.join(__dirname, 'server', 'cial_lsp.py'));
