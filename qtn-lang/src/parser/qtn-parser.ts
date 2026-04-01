import * as vscode from 'vscode';
import { spawn, ChildProcess } from 'child_process';
import { ParseResult } from './types';

interface QtndDiag {
    level?: string;
    line?: number;
    column?: number;
    message?: string;
}

interface QtndResponse {
    id?: number;
    ok?: boolean;
    hasErrors?: boolean;
    diagnostics?: QtndDiag[];
    error?: string;
}

export class QtnParser implements vscode.Disposable {
    private parserPath: string = 'qtnd';
    private outputChannel!: vscode.OutputChannel;
    private daemonProcess: ChildProcess | null = null;
    private isDaemonReady: boolean = false;
    private stdoutBuffer: string = '';
    private static instance: QtnParser | null = null;
    private initializationPromise: Promise<void> | null = null;
    private currentRequest: { filePath: string; resolve: Function; reject: Function; id: number } | null = null;
    private nextRequestId: number = 1;

    constructor(outputChannel: vscode.OutputChannel, parserPath?: string) {
        this.outputChannel = outputChannel;

        if (QtnParser.instance) {
            return QtnParser.instance;
        }

        if (parserPath) {
            this.parserPath = parserPath;
        }

        QtnParser.instance = this;
        return this;
    }

    public initialize(): Promise<void> {
        this.outputChannel.appendLine('🚀 Initializing qtnd daemon...');

        if (this.isDaemonReady && this.daemonProcess) {
            this.outputChannel.appendLine('✅ Daemon already ready');
            return Promise.resolve();
        }

        if (!this.initializationPromise) {
            this.initializationPromise = this.startDaemon();
        }
        return this.initializationPromise;
    }

    private startDaemon(): Promise<void> {
        return new Promise((resolveInitialize, rejectInitialize) => {
            if (this.isDaemonReady && this.daemonProcess) {
                resolveInitialize();
                return;
            }

            this.outputChannel.appendLine(`🚀 Starting qtnd daemon: ${this.parserPath} --stdio`);

            try {
                this.daemonProcess = spawn(this.parserPath, ['--stdio'], {
                    stdio: ['pipe', 'pipe', 'pipe'],
                    detached: false,
                    shell: process.platform === 'win32'
                });
            } catch (spawnError) {
                this.outputChannel.appendLine(`❌ Failed to spawn daemon: ${spawnError}`);
                rejectInitialize(new Error(`Failed to spawn: ${spawnError}`));
                return;
            }

            if (!this.daemonProcess) {
                const errorMsg = 'Failed to spawn qtnd daemon process.';
                this.outputChannel.appendLine(`❌ ${errorMsg}`);
                this.rejectAllPending(new Error(errorMsg));
                rejectInitialize(new Error(errorMsg));
                return;
            }

            const startupTimeout = setTimeout(() => {
                if (!this.isDaemonReady) {
                    const timeoutMsg = 'Daemon startup timeout - daemon did not respond within 10 seconds';
                    this.outputChannel.appendLine(`❌ ${timeoutMsg}`);
                    if (this.daemonProcess) {
                        this.daemonProcess.kill();
                    }
                    this.isDaemonReady = false;
                    this.daemonProcess = null;
                    rejectInitialize(new Error(timeoutMsg));
                    this.initializationPromise = null;
                }
            }, 10000);

            this.daemonProcess.on('error', (err: NodeJS.ErrnoException) => {
                clearTimeout(startupTimeout);
                this.isDaemonReady = false;
                this.daemonProcess = null;
                const errorMsg = err.code === 'ENOENT' ?
                    `qtnd command ('${this.parserPath}') not found. Please ensure qtnd is installed and in your system's PATH.` :
                    `Daemon error: ${err.message}`;
                this.outputChannel.appendLine(`❌ ${errorMsg}`);
                this.rejectAllPending(new Error(errorMsg));
                rejectInitialize(new Error(errorMsg));
                this.initializationPromise = null;
            });

            this.daemonProcess.on('close', (code, signal) => {
                clearTimeout(startupTimeout);
                this.isDaemonReady = false;
                this.daemonProcess = null;
                const errorMsg = `Daemon process closed (code: ${code}, signal: ${signal})`;
                this.outputChannel.appendLine(`❌ ${errorMsg}`);
                this.rejectAllPending(new Error(errorMsg));
                if (!this.isDaemonReady) {
                    rejectInitialize(new Error(errorMsg));
                }
                this.initializationPromise = null;
            });

            if (this.daemonProcess.stderr) {
                this.daemonProcess.stderr.setEncoding('utf8');
                this.daemonProcess.stderr.on('data', (data) => {
                    const errorText = data.toString().trim();
                    if (errorText.includes('error') || errorText.includes('Error')) {
                        this.outputChannel.appendLine(`🔴 STDERR: ${errorText}`);
                    }
                });
            }

            if (this.daemonProcess.stdout) {
                this.daemonProcess.stdout.setEncoding('utf8');
                this.daemonProcess.stdout.on('data', (data: string) => {
                    this.stdoutBuffer += data;
                    setImmediate(() => this.processStdoutBuffer(resolveInitialize, rejectInitialize, startupTimeout));
                });

                // handshake ping to confirm daemon is alive
                const pingId = this.nextRequestId++;
                const pingJson = JSON.stringify({ id: pingId, action: 'ping' });
                this.daemonProcess.stdin?.write(pingJson + '\n');
            } else {
                clearTimeout(startupTimeout);
                const errorMsg = 'Daemon stdout stream is not available.';
                this.outputChannel.appendLine(`❌ ${errorMsg}`);
                this.rejectAllPending(new Error(errorMsg));
                rejectInitialize(new Error(errorMsg));
            }
        });
    }

    private isProcessingBuffer = false;

    private extractNextJsonObject(): string | null {
        let start = -1;
        let depth = 0;
        let inString = false;
        let escaped = false;

        for (let i = 0; i < this.stdoutBuffer.length; i++) {
            const ch = this.stdoutBuffer[i];

            if (start < 0) {
                if (ch === '{') {
                    start = i;
                    depth = 1;
                    inString = false;
                    escaped = false;
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

            if (ch === '"') {
                inString = true;
                continue;
            }
            if (ch === '{') {
                depth++;
                continue;
            }
            if (ch === '}') {
                depth--;
                if (depth === 0) {
                    const jsonText = this.stdoutBuffer.slice(start, i + 1);
                    this.stdoutBuffer = this.stdoutBuffer.slice(i + 1);
                    return jsonText;
                }
            }
        }

        if (start > 0) {
            // Drop non-JSON prefix before the first object to avoid buffer growth.
            this.stdoutBuffer = this.stdoutBuffer.slice(start);
        }
        return null;
    }

    private processStdoutBuffer(resolveInitialize?: () => void, rejectInitialize?: (reason?: any) => void, startupTimeout?: NodeJS.Timeout) {
        if (this.isProcessingBuffer) {
            return;
        }

        this.isProcessingBuffer = true;

        try {
            while (true) {
                const jsonText = this.extractNextJsonObject();
                if (!jsonText) {
                    break;
                }

                try {
                    const result = JSON.parse(jsonText) as QtndResponse;

                    // first successful JSON response means daemon is ready
                    if (!this.isDaemonReady) {
                        this.isDaemonReady = true;
                        this.outputChannel.appendLine('✅ Daemon is ready!');
                        if (startupTimeout) {
                            clearTimeout(startupTimeout);
                        }
                        if (resolveInitialize) {
                            resolveInitialize();
                            resolveInitialize = undefined;
                        }
                    }

                    if (this.currentRequest && Number(result.id) === this.currentRequest.id) {
                        const parseResult = this.mapQtndResponseToParseResult(result, this.currentRequest.filePath);
                        this.currentRequest.resolve(parseResult);
                        this.currentRequest = null;
                    }
                } catch (parseError) {
                    this.outputChannel.appendLine(`❌ Error parsing daemon response: ${parseError}`);
                    continue;
                }
            }
        } finally {
            this.isProcessingBuffer = false;
        }
    }

    private mapQtndResponseToParseResult(result: QtndResponse, filePath: string): ParseResult {
        const diags = result.diagnostics ?? [];
        const errors = diags
            .filter(d => (d.level ?? 'error').toLowerCase() !== 'warning')
            .map(d => {
                const line = Math.max(1, Number(d.line ?? 1));
                const col = Math.max(1, Number(d.column ?? 1));
                return `${line}:${col}<SPACE>${d.message ?? 'QTN error'}`;
            });

        if (result.ok === false && result.error) {
            errors.unshift(`1:1<SPACE>${result.error}`);
        }

        return {
            success: errors.length === 0,
            errors,
            file: filePath
        };
    }

    async parseContent(content: string, filePath: string): Promise<ParseResult> {
        if (!this.isDaemonReady || !this.daemonProcess?.stdin?.writable) {
            await this.initialize();
        }

        return new Promise((resolve, reject) => {
            if (this.currentRequest) {
                this.currentRequest.reject(new Error('Request cancelled - newer request'));
            }

            const requestId = this.nextRequestId++;
            this.currentRequest = { filePath, resolve, reject, id: requestId };

            const request = {
                id: requestId,
                action: 'check',
                filename: filePath,
                content
            };

            const requestJson = JSON.stringify(request);

            this.daemonProcess!.stdin!.write(requestJson + '\n', (err) => {
                if (err) {
                    if (this.currentRequest && this.currentRequest.filePath === filePath) {
                        this.currentRequest.reject(new Error(`Failed to send request: ${err.message}`));
                        this.currentRequest = null;
                    }
                }
            });
        });
    }

    async parseFile(filePath: string, document?: vscode.TextDocument): Promise<ParseResult> {
        if (document) {
            return this.parseContent(document.getText(), filePath);
        }

        if (!this.isDaemonReady || !this.daemonProcess?.stdin?.writable) {
            await this.initialize();
        }

        return this.parseContent('', filePath);
    }

    private rejectAllPending(error: Error) {
        if (this.currentRequest) {
            this.currentRequest.reject(error);
            this.currentRequest = null;
        }
    }

    public dispose() {
        this.outputChannel.appendLine('🔄 Disposing qtnd daemon...');
        if (this.daemonProcess) {
            this.daemonProcess.kill();
            this.daemonProcess = null;
        }
        this.isDaemonReady = false;
        this.rejectAllPending(new Error('qtnd daemon disposed.'));
        this.initializationPromise = null;
        this.outputChannel.appendLine('✅ Daemon disposed');
    }

    parseErrorsToDiagnostics(errors: string[], document: vscode.TextDocument): vscode.Diagnostic[] {
        const diagnostics: vscode.Diagnostic[] = [];
        const lineCount = document.lineCount;

        for (let i = 0; i < errors.length; i++) {
            const error = errors[i];
            const match = error.match(/^(\d+)(?::(\d+))?<SPACE>([\s\S]+)$/);

            if (!match) {
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(0, 0, 0, 1),
                    error,
                    vscode.DiagnosticSeverity.Error
                ));
                continue;
            }

            const parsedLine = parseInt(match[1], 10);
            const parsedCol = parseInt(match[2] ?? '1', 10);
            const message = match[3];

            if (isNaN(parsedLine)) {
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(0, 0, 0, 1),
                    error,
                    vscode.DiagnosticSeverity.Error
                ));
                continue;
            }

            const line = Math.max(0, Math.min(parsedLine - 1, lineCount - 1));
            const specificLine = document.lineAt(line);
            const col = Math.max(0, Math.min(parsedCol - 1, Math.max(0, specificLine.text.length)));
            const endCol = Math.min(specificLine.text.length, col + 1);
            const range = new vscode.Range(
                line,
                col,
                line,
                endCol
            );

            diagnostics.push(new vscode.Diagnostic(
                range,
                message,
                vscode.DiagnosticSeverity.Error
            ));
        }

        return diagnostics;
    }
}

