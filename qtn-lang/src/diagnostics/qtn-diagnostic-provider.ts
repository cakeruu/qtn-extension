import * as vscode from 'vscode';
import { QtnParser } from '../parser/qtn-parser';

export class QtnDiagnosticProvider {
    private parser: QtnParser;
    private diagnosticCollection: vscode.DiagnosticCollection;
    private validationTimeout: Map<string, NodeJS.Timeout> = new Map();
    private readonly DEBOUNCE_TIME = 100;

    constructor(context: vscode.ExtensionContext, parser: QtnParser) {
        this.parser = parser;
        this.diagnosticCollection = vscode.languages.createDiagnosticCollection('qtn');
        context.subscriptions.push(this.diagnosticCollection);
    }

    async validateDocument(document: vscode.TextDocument, immediate: boolean = false): Promise<void> {
        if (document.languageId !== 'qtn') {
            return;
        }

        const uri = document.uri.toString();
        const existingTimeout = this.validationTimeout.get(uri);
        if (existingTimeout) {
            clearTimeout(existingTimeout);
            this.validationTimeout.delete(uri);
        }

        const doValidation = async () => {
            try {
                const result = await this.parser.parseFile(document.fileName, document);

                this.diagnosticCollection.delete(document.uri);

                if (!result.success && result.errors?.length) {
                    const diagnostics = this.parser.parseErrorsToDiagnostics(result.errors, document);
                    this.diagnosticCollection.set(document.uri, diagnostics);
                }
            } catch (error) {
                const message = error instanceof Error ? error.message : String(error);
                if (message.includes('Request cancelled - newer request')) {
                    // Expected during rapid typing: a newer validation superseded this one.
                    return;
                }
                this.diagnosticCollection.set(document.uri, [
                    new vscode.Diagnostic(
                        new vscode.Range(0, 0, 0, 1),
                        `Validation failed: ${message}`,
                        vscode.DiagnosticSeverity.Error
                    )
                ]);
            }
        };

        if (immediate) {
            await doValidation();
        } else {
            const timeout = setTimeout(doValidation, this.DEBOUNCE_TIME);
            this.validationTimeout.set(uri, timeout);
        }
    }

    dispose(): void {
        this.diagnosticCollection.dispose();
        for (const timeout of this.validationTimeout.values()) {
            clearTimeout(timeout);
        }
        this.validationTimeout.clear();
    }
}

