// Runs inside a real VSCode Extension Development Host.
const assert = require('node:assert/strict');
const vscode = require('vscode');

function nextDiagnostics(uri, hasErrors) {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => { subscription.dispose(); reject(new Error('Ciallang diagnostics timed out')); }, 15000);
    const subscription = vscode.languages.onDidChangeDiagnostics(event => {
      if (!event.uris.some(changed => changed.toString() === uri.toString())) return;
      const diagnostics = vscode.languages.getDiagnostics(uri);
      const unavailable = diagnostics.find(item => /checker unavailable/i.test(item.message));
      if (unavailable || (diagnostics.length > 0) === hasErrors) {
        clearTimeout(timeout);
        subscription.dispose();
        if (unavailable) reject(new Error(unavailable.message));
        else resolve();
      }
    });
  });
}

exports.run = async function run() {
  const root = vscode.workspace.workspaceFolders?.[0]?.uri;
  assert(root, 'Open an isolated smoke-test workspace');
  const uri = vscode.Uri.joinPath(root, 'smoke.tjs');
  const text = 'function greet(name) { return name; }\nvar result = greet("world");\nvar items = [];\nitems.push(1);\nArray.push;\nMath.abs;\n';
  await vscode.workspace.fs.writeFile(uri, Buffer.from(text));
  const document = await vscode.workspace.openTextDocument(uri);
  const editor = await vscode.window.showTextDocument(document);
  assert.equal(document.languageId, 'ciallang');
  editor.selection = new vscode.Selection(0, 0, 0, 0);
  await vscode.commands.executeCommand('editor.action.commentLine');
  assert(document.lineAt(0).text.startsWith('//'), 'language configuration should enable line comments');
  await vscode.commands.executeCommand('editor.action.commentLine');
  assert.equal(document.getText(), text);
  await vscode.extensions.getExtension('ciallang.ciallang-language').activate();
  const completion = await vscode.commands.executeCommand('vscode.executeCompletionItemProvider', uri, new vscode.Position(3, 6));
  assert(completion.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'push'));
  const definitions = await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, new vscode.Position(1, 15));
  assert.equal(definitions.length, 1);
  assert.equal((definitions[0].range || definitions[0].targetSelectionRange).start.line, 0);
  const memberDefinitions = await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, new vscode.Position(3, 7));
  assert.equal(memberDefinitions.length, 1, 'Array.push should have a declaration definition');
  const memberUri = memberDefinitions[0].uri || memberDefinitions[0].targetUri;
  assert(memberUri.path.endsWith('/Array.d.tjs'), memberUri.toString());
  const declarationDocument = await vscode.workspace.openTextDocument(memberUri);
  assert(declarationDocument.getText().includes('function push(values: any*): Integer;'), 'definition target should be a readable declaration');
  for (const [line, character, filename, signature] of [
    [4, 7, 'Array.d.tjs', 'function push(values: any*): Integer;'],
    [5, 7, 'Math.d.tjs', 'function abs(']
  ]) {
    const locations = await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, new vscode.Position(line, character));
    assert.equal(locations.length, 1, `qualified member should resolve to ${filename}`);
    const target = locations[0].uri || locations[0].targetUri;
    assert(target.path.endsWith('/' + filename), target.toString());
    const targetDocument = await vscode.workspace.openTextDocument(target);
    assert(targetDocument.getText().includes(signature), 'qualified definition target should be readable');
  }
  await document.save();
  const validText = document.getText();
  const invalid = new vscode.WorkspaceEdit();
  invalid.insert(uri, document.positionAt(validText.length), '\nvar broken = ;');
  const errorPublished = nextDiagnostics(uri, true);
  await vscode.workspace.applyEdit(invalid);
  await errorPublished;
  const fix = new vscode.WorkspaceEdit();
  fix.replace(uri, new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)), validText);
  const errorCleared = nextDiagnostics(uri, false);
  await vscode.workspace.applyEdit(fix);
  await errorCleared;
  await document.save();
  console.log('Ciallang VSCode smoke: startup, diagnostics, completion, definition and declaration file reading passed');
};
