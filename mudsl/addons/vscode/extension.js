'use strict';
const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const { LanguageClient } = require('vscode-languageclient/node');

let client;
let restarting = Promise.resolve();

async function start(context) {
    if (client) {
        await client.stop();
        client = undefined;
    }
    if (!vscode.workspace.isTrusted) return;
    const config = vscode.workspace.getConfiguration('mu');
    const args = [context.asAbsolutePath('server/server.py'), '--stdio'];
    if (config.get('checkerPath')) args.push('--checker', config.get('checkerPath'));
    const languages = config.get('nativeApi') ? ['mull', 'c', 'cpp', 'd'] : ['mull'];
    client = new LanguageClient('mu', 'Mu Language Tools', {
        command: config.get('pythonPath'), args
    }, {
        documentSelector: languages.map(language => ({ language, scheme: 'file' })),
        initializationOptions: {
            repositoryRoot: config.get('repositoryRoot'),
            packagePaths: config.get('packagePaths')
        }
    });
    await client.start();
}

function restart(context) {
    restarting = restarting.then(() => start(context)).catch(error => {
        vscode.window.showErrorMessage(`Mu language server: ${error.message}`);
    });
    return restarting;
}

async function documentation() {
    const choice = await vscode.window.showQuickPick(['MuLL', 'MuDSL', 'MuLIB'], { placeHolder: 'Open a local manual' });
    if (!choice) return;
    const configured = vscode.workspace.getConfiguration('mu').get('repositoryRoot');
    const roots = configured ? [configured] : (vscode.workspace.workspaceFolders || []).map(folder => folder.uri.fsPath);
    for (const root of roots) {
        const mudsl = fs.existsSync(path.join(root, 'MuDSL.hpp')) ? root : path.join(root, 'mudsl');
        const mulib = mudsl === root ? path.dirname(root) : root;
        const file = choice === 'MuLL' ? path.join(mudsl, 'mull/README.md') :
            choice === 'MuDSL' ? path.join(mudsl, 'manual/README.md') : path.join(mulib, 'manual/README.md');
        if (fs.existsSync(file)) {
            await vscode.commands.executeCommand('markdown.showPreview', vscode.Uri.file(file));
            return;
        }
    }
    vscode.window.showInformationMessage('Set mu.repositoryRoot to your MuLIB or MuDSL checkout to open its manuals.');
}

function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand('mu.openDocumentation', documentation),
        vscode.commands.registerCommand('mu.restartServer', () => restart(context)),
        vscode.workspace.onDidGrantWorkspaceTrust(() => restart(context)),
        vscode.workspace.onDidChangeConfiguration(event => {
            if (event.affectsConfiguration('mu')) restart(context);
        })
    );
    return restart(context);
}

async function deactivate() {
    await restarting;
    if (client) await client.stop();
}

module.exports = { activate, deactivate };
