import * as vscode from 'vscode';
import { QtnParser } from './parser/qtn-parser';
import { QtnDiagnosticProvider } from './diagnostics/qtn-diagnostic-provider';
import { QtnCompletionProvider } from './completion/qtn-completion-provider';
import { QtnFormatter } from './formatting/qtn-formatter';

let qtnOutputChannel: vscode.OutputChannel;
let diagnosticProvider: QtnDiagnosticProvider | null = null;

export async function activate(context: vscode.ExtensionContext) {
    qtnOutputChannel = vscode.window.createOutputChannel('QTN');
    context.subscriptions.push(qtnOutputChannel);

    const showOutputCommand = vscode.commands.registerCommand('qtn.showOutput', () => {
        qtnOutputChannel.show();
    });
    context.subscriptions.push(showOutputCommand);

    const parser = new QtnParser(qtnOutputChannel, 'qtnd');
    context.subscriptions.push(parser);

    const restartQtndServerCommand = vscode.commands.registerCommand('qtn.restartQtndServer', async () => {
        parser.dispose();
        try {
            await parser.initialize();
            qtnOutputChannel.appendLine('✅ qtnd server restarted successfully');

            const openDocuments = vscode.workspace.textDocuments.filter(doc => doc.languageId === 'qtn');
            for (const doc of openDocuments) {
                if (diagnosticProvider) {
                    diagnosticProvider.validateDocument(doc, true);
                }
            }
        } catch (error) {
            const errorMessage = error instanceof Error ? error.message : String(error);
            qtnOutputChannel.appendLine(`❌ Failed to restart qtnd server: ${errorMessage}`);
        }
    });
    context.subscriptions.push(restartQtndServerCommand);

    try {
        await parser.initialize();
        qtnOutputChannel.appendLine('✅ qtnd server started');
    } catch (error) {
        const errorMessage = error instanceof Error ? error.message : String(error);
        qtnOutputChannel.appendLine(`❌ Failed to connect to qtnd server: ${errorMessage}`);
        qtnOutputChannel.show();
        qtnOutputChannel.appendLine('💡 qtnd is not installed or not found in PATH.');
        qtnOutputChannel.appendLine('💡 Use "QTN: Restart Qtnd Server" command after installing it.');
        return;
    }

    diagnosticProvider = new QtnDiagnosticProvider(context, parser);

    context.subscriptions.push(
        vscode.workspace.onDidChangeTextDocument(event => {
            if (event.document.languageId === 'qtn' && event.contentChanges.length > 0) {
                diagnosticProvider?.validateDocument(event.document);
            }
        })
    );

    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument(document => {
            if (document.languageId === 'qtn') {
                diagnosticProvider?.validateDocument(document, true);
            }
        })
    );

    const completionProvider = new QtnCompletionProvider();
    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider(
            { language: 'qtn', scheme: 'file' },
            completionProvider,
            ':', '<', '&', ' ', '=', '{', '"', "'"
        )
    );

    const formatter = new QtnFormatter();
    context.subscriptions.push(
        vscode.languages.registerDocumentFormattingEditProvider(
            { language: 'qtn', scheme: 'file' },
            formatter
        )
    );

    const openDocuments = vscode.workspace.textDocuments.filter(doc => doc.languageId === 'qtn');
    for (const doc of openDocuments) {
        diagnosticProvider.validateDocument(doc, true);
    }

    qtnOutputChannel.appendLine('QTN extension activation complete');
}

export function deactivate() {
    // cleanup via disposables
}