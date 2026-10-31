// Runs inside a real VSCode Extension Development Host, after `node prepare.js`.
const assert = require('node:assert/strict');
const vscode = require('vscode');

exports.run = async function run() {
  const root = vscode.workspace.workspaceFolders?.[0]?.uri;
  assert(root, 'Open an isolated smoke-test workspace');
  const uri = vscode.Uri.joinPath(root, 'smoke.tjs');
  const text = 'function greet(name) { return name; }\nvar result = greet("world");\nvar items = [];\nitems.push(1);\n';
  await vscode.workspace.fs.writeFile(uri, Buffer.from(text));
  const document = await vscode.workspace.openTextDocument(uri);
  await vscode.window.showTextDocument(document);
  await vscode.extensions.getExtension('ciallang.ciallang-language').activate();
  const completion = await vscode.commands.executeCommand('vscode.executeCompletionItemProvider', uri, new vscode.Position(3, 6));
  assert(completion.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'push'));
  const definitions = await vscode.commands.executeCommand('vscode.executeDefinitionProvider', uri, new vscode.Position(1, 15));
  assert.equal(definitions.length, 1);
  assert.equal((definitions[0].range || definitions[0].targetSelectionRange).start.line, 0);
  const edits = await vscode.commands.executeCommand('vscode.executeDocumentRenameProvider', uri, new vscode.Position(1, 15), 'welcome');
  assert(edits instanceof vscode.WorkspaceEdit);
  assert.equal(edits.get(uri).length, 2);
  await vscode.workspace.applyEdit(edits);
  assert(document.getText().startsWith('function welcome('));
  assert(document.getText().includes('= welcome("world")'));
  await document.save();
  console.log('Ciallang VSCode smoke: completion, definition, rename passed');
};
