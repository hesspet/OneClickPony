export const standardBaudRate = 115200;

export interface SerialConnectionOptions {
    baudRate?: number;
    onLine?: (line: string) => void;
    onDisconnect?: (reason?: unknown) => void;
}

export interface SerialPortLike {
    open(options: { baudRate: number }): Promise<void>;
    close(): Promise<void>;
    readable: ReadableStream<Uint8Array> | null;
    writable: WritableStream<Uint8Array> | null;
}

type LineListener = (line: string) => void;

/** Splits decoded serial chunks without losing CRLF boundaries or partial UTF-8 lines. */
export class LineBuffer {
    private pending = "";
    private endedWithCarriageReturn = false;

    push(chunk: string): string[] {
        if (this.endedWithCarriageReturn && chunk.startsWith("\n")) chunk = chunk.slice(1);
        this.endedWithCarriageReturn = false;
        const completeLines: string[] = [];
        for (let index = 0; index < chunk.length; index += 1) {
            const character = chunk[index];
            if (character === "\r" || character === "\n") {
                completeLines.push(this.pending);
                this.pending = "";
                this.endedWithCarriageReturn = character === "\r" && index === chunk.length - 1;
            } else {
                this.pending += character;
            }
        }
        return completeLines;
    }

    finish(): string[] {
        this.endedWithCarriageReturn = false;
        if (this.pending.length === 0) return [];
        const line = this.pending;
        this.pending = "";
        return [line];
    }
}

/**
 * Owns exactly one Web-Serial port. It turns its byte stream into complete lines
 * and deliberately has no knowledge of OneKlickPony commands.
 */
export class SerialConnection {
    private readonly textEncoder = new TextEncoder();
    private readonly textDecoder = new TextDecoder();
    private readonly lineBuffer = new LineBuffer();
    private readonly lineListeners = new Set<LineListener>();
    private reader: ReadableStreamDefaultReader<Uint8Array> | undefined;
    private readTask: Promise<void> | undefined;
    private isClosing = false;

    constructor(private readonly port: SerialPortLike, private readonly options: SerialConnectionOptions = {}) {}

    get isOpen(): boolean {
        return this.reader !== undefined;
    }

    onLine(listener: LineListener): () => void {
        this.lineListeners.add(listener);
        return () => this.lineListeners.delete(listener);
    }

    async open(): Promise<void> {
        if (this.isOpen) return;
        this.isClosing = false;
        await this.port.open({ baudRate: this.options.baudRate ?? standardBaudRate });
        if (this.port.readable === null) {
            await this.port.close();
            throw new Error("Der serielle Anschluss stellt keinen Lesestrom bereit.");
        }
        this.reader = this.port.readable.getReader();
        this.readTask = this.readLines();
    }

    async close(): Promise<void> {
        this.isClosing = true;
        const reader = this.reader;
        this.reader = undefined;
        if (reader !== undefined) {
            try {
                await reader.cancel();
            } catch {
                // A disconnected port may already have cancelled the reader.
            }
        }
        await this.readTask?.catch(() => undefined);
        this.readTask = undefined;
        try {
            await this.port.close();
        } catch (reason) {
            if (!this.isClosing) throw reason;
        }
    }

    async writeLine(command: string): Promise<void> {
        if (!this.isOpen || this.port.writable === null) {
            throw new Error("Der serielle Anschluss ist nicht geöffnet.");
        }
        if (command.includes("\r") || command.includes("\n")) {
            throw new Error("Ein serieller Befehl darf keinen Zeilenumbruch enthalten.");
        }
        const writer = this.port.writable.getWriter();
        try {
            await writer.write(this.textEncoder.encode(`${command}\n`));
        } finally {
            writer.releaseLock();
        }
    }

    private async readLines(): Promise<void> {
        const reader = this.reader;
        if (reader === undefined) return;
        let disconnectReason: unknown;
        try {
            while (!this.isClosing) {
                const { done, value } = await reader.read();
                if (done) break;
                if (value !== undefined) this.emitLines(this.lineBuffer.push(this.textDecoder.decode(value, { stream: true })));
            }
            this.emitLines(this.lineBuffer.push(this.textDecoder.decode()));
            this.emitLines(this.lineBuffer.finish());
        } catch (reason) {
            disconnectReason = reason;
            if (!this.isClosing) throw reason;
        } finally {
            reader.releaseLock();
            const disconnectedUnexpectedly = !this.isClosing;
            if (this.reader === reader) this.reader = undefined;
            if (disconnectedUnexpectedly) this.options.onDisconnect?.(disconnectReason);
        }
    }

    private emitLines(lines: string[]): void {
        for (const line of lines) {
            this.options.onLine?.(line);
            for (const listener of this.lineListeners) listener(line);
        }
    }
}
