const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { before, after, test } = require('node:test');
const { Registry, INITIAL } = require('vscode-textmate');
const onig = require('vscode-oniguruma');
let registry, grammar;

before(async () => {
  const binary = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
  await onig.loadWASM(binary.buffer.slice(binary.byteOffset, binary.byteOffset + binary.byteLength));
  const raw = JSON.parse(fs.readFileSync(path.join(__dirname, '../syntaxes/ciallang.tmLanguage.json'), 'utf8'));
  registry = new Registry({
    onigLib: Promise.resolve({createOnigScanner: patterns => new onig.OnigScanner(patterns), createOnigString: text => new onig.OnigString(text)}),
    loadGrammar: async scope => scope === raw.scopeName ? raw : null
  });
  grammar = await registry.loadGrammar('source.ciallang');
});
after(() => registry?.dispose());

function tokens(text) {
  let state = INITIAL;
  return text.split('\n').map(line => {
    const result = grammar.tokenizeLine(line, state);
    state = result.ruleStack;
    return result.tokens.map(token => ({text: line.slice(token.startIndex, token.endIndex), scopes: token.scopes}));
  });
}
function scoped(rows, text, scope, line = 0) {
  assert(rows[line].some(token => token.text.includes(text) && token.scopes.some(item => item.startsWith(scope))),
    `${JSON.stringify(text)} should have ${scope}: ${JSON.stringify(rows[line])}`);
}

test('manifest contributes the grammar and language editing configuration', () => {
  const manifest = require('../package.json');
  const contributed = manifest.contributes.grammars.find(item => item.language === 'ciallang');
  assert.equal(contributed.scopeName, 'source.ciallang');
  assert(fs.existsSync(path.join(__dirname, '..', contributed.path)));
  const config = require('../language-configuration.json');
  assert.equal(config.comments.lineComment, '//');
  assert.deepEqual(config.comments.blockComment, ['/*', '*/']);
  assert(config.brackets.some(([open, close]) => open === '{' && close === '}'));
});

test('keywords, Unicode declarations, constants and calls receive theme scopes', () => {
  const rows = tokens('class 容器 extends Array { function 计算(x) { var n = true; return 计算(x); } }');
  scoped(rows, 'class', 'storage.type.class');
  scoped(rows, '容器', 'entity.name.type.class');
  scoped(rows, 'Array', 'support.class');
  scoped(rows, '计算', 'entity.name.function');
  scoped(rows, 'true', 'constant.language');
  scoped(rows, 'return', 'keyword.control');
});

test('strings protect keywords and recognize escapes without interpolation', () => {
  const rows = tokens('var s = "if \\" ${name} &x;"; var n = 2;');
  scoped(rows, 'if', 'string.quoted.double');
  scoped(rows, '\\"', 'constant.character.escape');
  scoped(rows, '${name}', 'string.quoted.double');
  scoped(rows, 'var', 'storage.type');
  scoped(rows, '2', 'constant.numeric');
});

test('nested block comments retain state across lines and close at the right depth', () => {
  const rows = tokens('/* outer\n /* inner */ still outer\n */ var number = 42; // return');
  scoped(rows, 'still outer', 'comment.block', 1);
  scoped(rows, 'var', 'storage.type', 2);
  scoped(rows, 'return', 'comment.line', 2);
});

test('numeric syntax includes hex and binary floats, exponents and octets', () => {
  const rows = tokens('var values = [0x1.07CCp14, 0b1.1p-2, 1.5e-3, .5, <% 00 ff %>];');
  for (const value of ['0x1.07CCp14', '0b1.1p-2', '1.5e-3', '.5', '00', 'ff']) scoped(rows, value, 'constant.numeric');
});

test('template interpolation handles nested braces and ampersand expressions', () => {
  const rows = tokens('var s = @"text ${%[key: 1][\"key\"]} &2 + 3; end";');
  scoped(rows, '${', 'punctuation.section.interpolation');
  scoped(rows, '1', 'constant.numeric');
  scoped(rows, '2', 'constant.numeric');
  scoped(rows, 'end', 'string.quoted.double.template');
  const nested = tokens('var s = @"${function() { return 4; }} tail";');
  scoped(nested, '4', 'constant.numeric');
  scoped(nested, 'tail', 'string.quoted.double.template');
});

test('escaped template markers remain text and single quotes support interpolation', () => {
  const rows = tokens(String.raw`var s = @'\&name; \${name} &2;';`);
  scoped(rows, '\\&', 'constant.character.escape');
  scoped(rows, '\\$', 'constant.character.escape');
  scoped(rows, '2', 'constant.numeric');
});

test('regular expressions handle classes and flags while division stays an operator', () => {
  const rows = tokens('var re = /[a/b]+\\//gi; var n = total / 2 / count; value /= 3;');
  scoped(rows, 'a/b', 'constant.other.character-class.regexp');
  scoped(rows, 'gi', 'keyword.other.regexp-flags');
  assert.equal(rows[0].filter(token => token.text === '/' && token.scopes.includes('keyword.operator.ciallang')).length, 2);
  scoped(rows, '/=', 'keyword.operator');
  const returned = tokens('return /yes/.test(text);');
  scoped(returned, 'yes', 'string.regexp');
  scoped(returned, 'return', 'keyword.control');
});

test('preprocessor conditions are highlighted without confusing template prefixes', () => {
  const rows = tokens('@if(ENABLED == 1)\nvar s = @"if";\n@endif');
  scoped(rows, 'if', 'keyword.control.preprocessor');
  scoped(rows, '1', 'constant.numeric');
  scoped(rows, 'if', 'string.quoted.double.template', 1);
  scoped(rows, 'endif', 'keyword.control.preprocessor', 2);
});

test('an unfinished regex does not color following lines as a string', () => {
  const rows = tokens('var re = /unterminated\nvar n = 1;');
  scoped(rows, 'var', 'storage.type', 1);
  scoped(rows, '1', 'constant.numeric', 1);
});
