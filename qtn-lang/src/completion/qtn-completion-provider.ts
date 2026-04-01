import * as vscode from 'vscode';
import { QTN_TYPES } from '../utils/constants';

interface DocumentSymbols {
    components: Set<string>;
    structs: Set<string>;
    enums: Set<string>;
    unions: Set<string>;
    events: Set<string>;
}

export class QtnCompletionProvider implements vscode.CompletionItemProvider {
    private documentSymbols: Map<string, DocumentSymbols> = new Map();

    async provideCompletionItems(
        document: vscode.TextDocument,
        position: vscode.Position,
        token: vscode.CancellationToken
    ): Promise<vscode.CompletionItem[] | vscode.CompletionList | undefined> {
        if (token.isCancellationRequested) {
            return undefined;
        }

        await this.updateDocumentSymbols(document);
        const linePrefix = document.lineAt(position).text.substring(0, position.character);
        const insideBody = this.isInsideBraceBody(document, position);

        if (linePrefix.match(/:\s*\w*$/) || linePrefix.match(/<\s*\w*$/) || linePrefix.match(/\(\s*\w*$/)) {
            return this.provideTypeCompletions(document);
        }

        // Inside component/struct/event/global/input bodies, suggest QTN types on empty/new field lines.
        if (insideBody && linePrefix.match(/^\s*[A-Za-z_]*$/)) {
            return this.provideTypeCompletions(document);
        }

        if (linePrefix.trim() === '' || linePrefix.trim().match(/^[a-zA-Z_]*$/)) {
            return this.provideKeywordCompletions();
        }

        return undefined;
    }

    private provideTypeCompletions(document: vscode.TextDocument): vscode.CompletionItem[] {
        const items: vscode.CompletionItem[] = [];

        for (const type of QTN_TYPES) {
            const item = new vscode.CompletionItem(type, vscode.CompletionItemKind.TypeParameter);
            item.detail = `Built-in type: ${type}`;
            items.push(item);
        }

        const symbols = this.documentSymbols.get(document.uri.toString());
        if (symbols) {
            const userTypes = [
                ...Array.from(symbols.components),
                ...Array.from(symbols.structs),
                ...Array.from(symbols.enums),
                ...Array.from(symbols.unions),
                ...Array.from(symbols.events),
            ];
            for (const userType of userTypes) {
                const item = new vscode.CompletionItem(userType, vscode.CompletionItemKind.Class);
                item.detail = 'QTN user type';
                items.push(item);
            }
        }

        return items;
    }

    private provideKeywordCompletions(): vscode.CompletionItem[] {
        const keywords = [
            { label: 'component', detail: 'Create component', snippet: 'component ${1:Name} {\n\t${0}\n}' },
            { label: 'singleton component', detail: 'Create singleton component', snippet: 'singleton component ${1:Name} {\n\t${0}\n}' },
            { label: 'struct', detail: 'Create struct', snippet: 'struct ${1:Name} {\n\t${0}\n}' },
            { label: 'enum', detail: 'Create enum', snippet: 'enum ${1:Name} : ${2:byte} {\n\t${0}\n}' },
            { label: 'flags', detail: 'Create flags enum', snippet: 'flags ${1:Name} : ${2:byte} {\n\t${0}\n}' },
            { label: 'union', detail: 'Create union', snippet: 'union ${1:Name} {\n\t${0}\n}' },
            { label: 'input', detail: 'Create input block', snippet: 'input {\n\t${0}\n}' },
            { label: 'signal', detail: 'Create signal', snippet: 'signal ${1:OnEvent}(${2:FP value});' },
            { label: 'event', detail: 'Create event', snippet: 'event ${1:Name} {\n\t${0}\n}' },
            { label: 'global', detail: 'Create global block', snippet: 'global {\n\t${0}\n}' },
            { label: 'asset', detail: 'Create asset declaration', snippet: 'asset ${1:TypeName};' },
            { label: 'import', detail: 'Create import declaration', snippet: 'import ${1:Namespace.Type};' },
            { label: 'using', detail: 'Create using declaration', snippet: 'using ${1:Namespace};' },
            { label: '#define', detail: 'Define constant', snippet: '#define ${1:NAME} ${2:value}' },
            { label: '#pragma', detail: 'Pragma directive', snippet: '#pragma ${1:max_players} ${2:8}' }
        ];

        return keywords.map(keyword => {
            const item = new vscode.CompletionItem(keyword.label, vscode.CompletionItemKind.Keyword);
            item.detail = keyword.detail;
            item.insertText = new vscode.SnippetString(keyword.snippet);
            return item;
        });
    }

    private async updateDocumentSymbols(document: vscode.TextDocument): Promise<void> {
        const uri = document.uri.toString();
        const symbols: DocumentSymbols = {
            components: new Set<string>(),
            structs: new Set<string>(),
            enums: new Set<string>(),
            unions: new Set<string>(),
            events: new Set<string>()
        };

        const content = document.getText();
        let match: RegExpExecArray | null;

        const componentRegex = /\b(?:singleton\s+)?component\s+([A-Za-z_][A-Za-z0-9_]*)/g;
        while ((match = componentRegex.exec(content)) !== null) {
            symbols.components.add(match[1]);
        }

        const structRegex = /\bstruct\s+([A-Za-z_][A-Za-z0-9_]*)/g;
        while ((match = structRegex.exec(content)) !== null) {
            symbols.structs.add(match[1]);
        }

        const enumRegex = /\b(?:enum|flags)\s+([A-Za-z_][A-Za-z0-9_]*)/g;
        while ((match = enumRegex.exec(content)) !== null) {
            symbols.enums.add(match[1]);
        }

        const unionRegex = /\bunion\s+([A-Za-z_][A-Za-z0-9_]*)/g;
        while ((match = unionRegex.exec(content)) !== null) {
            symbols.unions.add(match[1]);
        }

        const eventRegex = /\bevent\s+([A-Za-z_][A-Za-z0-9_]*)/g;
        while ((match = eventRegex.exec(content)) !== null) {
            symbols.events.add(match[1]);
        }

        this.documentSymbols.set(uri, symbols);
    }

    private isInsideBraceBody(document: vscode.TextDocument, position: vscode.Position): boolean {
        const text = document.getText(new vscode.Range(0, 0, position.line, position.character));
        let depth = 0;
        let inString = false;
        let inLineComment = false;
        let inBlockComment = false;
        let escaped = false;

        for (let i = 0; i < text.length; i++) {
            const ch = text[i];
            const next = i + 1 < text.length ? text[i + 1] : '';

            if (inLineComment) {
                if (ch === '\n') {
                    inLineComment = false;
                }
                continue;
            }
            if (inBlockComment) {
                if (ch === '*' && next === '/') {
                    inBlockComment = false;
                    i++;
                }
                continue;
            }
            if (inString) {
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (ch === '\\') {
                    escaped = true;
                    continue;
                }
                if (ch === '"') {
                    inString = false;
                }
                continue;
            }

            if (ch === '/' && next === '/') {
                inLineComment = true;
                i++;
                continue;
            }
            if (ch === '/' && next === '*') {
                inBlockComment = true;
                i++;
                continue;
            }
            if (ch === '"') {
                inString = true;
                continue;
            }

            if (ch === '{') {
                depth++;
            } else if (ch === '}') {
                depth = Math.max(0, depth - 1);
            }
        }

        return depth > 0;
    }
}

