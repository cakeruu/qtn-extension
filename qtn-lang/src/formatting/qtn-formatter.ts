import * as vscode from 'vscode';

export class QtnFormatter implements vscode.DocumentFormattingEditProvider {
    provideDocumentFormattingEdits(
        document: vscode.TextDocument,
        _options: vscode.FormattingOptions,
        token: vscode.CancellationToken
    ): vscode.TextEdit[] {
        if (token.isCancellationRequested) {
            return [];
        }

        const text = document.getText();
        const formattedText = this.formatQtnContent(text);

        if (formattedText === text) {
            return [];
        }

        const fullRange = new vscode.Range(
            document.positionAt(0),
            document.positionAt(text.length)
        );

        return [vscode.TextEdit.replace(fullRange, formattedText)];
    }

    private formatQtnContent(content: string): string {
        const lines = content.split('\n');
        const formattedLines: string[] = [];
        let indentLevel = 0;

        for (let i = 0; i < lines.length; i++) {
            const raw = lines[i];
            const line = raw.trim();

            if (line === '') {
                formattedLines.push('');
                continue;
            }

            if (line.startsWith('}') || line.startsWith(']') || line.startsWith(')')) {
                indentLevel = Math.max(0, indentLevel - 1);
            }

            formattedLines.push('\t'.repeat(indentLevel) + line);

            if (line.endsWith('{') || line.endsWith('(') || line.endsWith('[')) {
                indentLevel++;
            }
        }

        while (formattedLines.length > 0 && formattedLines[formattedLines.length - 1].trim() === '') {
            formattedLines.pop();
        }

        return formattedLines.join('\n') + (formattedLines.length > 0 ? '\n' : '');
    }
}

