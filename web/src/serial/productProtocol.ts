import { SerialConnection } from "./serialConnection";

export const minimumPulseDurationMillis = 1;
export const maximumPulseDurationMillis = 60000;

export type OutputMode =
    | { kind: "toggle" }
    | { kind: "pulse"; durationMillis: number };

export interface ProductStatus {
    firmwareVersion: string;
    runtime: string;
    state: string;
    isBound: boolean;
    outputMode: OutputMode;
    isOutputActive: boolean;
    hasLocalBond?: boolean;
    isLinkConnected?: boolean;
    isLinkEncrypted?: boolean;
    isLinkBonded?: boolean;
    peerIdentity?: string;
    lastError?: string;
    rawLines: readonly string[];
}

export interface ProtocolResponse {
    command: string;
    lines: readonly string[];
}

export class ProductProtocolError extends Error {
    constructor(message: string, readonly lines: readonly string[] = []) {
        super(message);
        this.name = "ProductProtocolError";
    }
}

export function validatePulseDuration(durationMillis: number): number {
    if (!Number.isInteger(durationMillis) || durationMillis < minimumPulseDurationMillis || durationMillis > maximumPulseDurationMillis) {
        throw new ProductProtocolError(
            `Die Impulsdauer muss eine ganze Zahl zwischen ${minimumPulseDurationMillis} und ${maximumPulseDurationMillis} Millisekunden sein.`,
        );
    }
    return durationMillis;
}

export function parseOutputMode(value: string): OutputMode | undefined {
    const normalizedValue = value.trim().toLowerCase();
    if (normalizedValue === "toggle") return { kind: "toggle" };
    const pulseMatch = /^impuls\s+(\d+)$/.exec(normalizedValue);
    if (pulseMatch === null) return undefined;
    const durationMillis = Number(pulseMatch[1]);
    try {
        return { kind: "pulse", durationMillis: validatePulseDuration(durationMillis) };
    } catch {
        return undefined;
    }
}

/** Parses the stable, human-readable status output of product firmware 0.5.0. */
export function parseStatusResponse(lines: readonly string[]): ProductStatus | undefined {
    const statusLine = lines.find((line) => line.startsWith("Firmware "));
    if (statusLine === undefined) return undefined;
    const required = /^Firmware\s+([^,]+),\s*Runtime=([^,]+),\s*Zustand=([^,]+),\s*Zuordnung=(ja|nein),\s*Modus=([^,]+),\s*Ausgang=(aktiv|inaktiv)$/.exec(statusLine.trim());
    if (required === null) return undefined;
    const outputMode = parseOutputMode(required[5]);
    if (outputMode === undefined) return undefined;

    const findYesNo = (label: string): boolean | undefined => {
        const line = lines.find((candidate) => candidate.startsWith(`${label}:`));
        if (line === undefined) return undefined;
        const value = line.slice(label.length + 1).trim();
        return value === "ja" ? true : value === "nein" ? false : undefined;
    };
    const peerLine = lines.find((line) => line.startsWith("Peer-ID:"));
    const errorLine = lines.find((line) => line.startsWith("Letzter Fehler:"));
    return {
        firmwareVersion: required[1].trim(),
        runtime: required[2].trim(),
        state: required[3].trim(),
        isBound: required[4] === "ja",
        outputMode,
        isOutputActive: required[6] === "aktiv",
        hasLocalBond: findYesNo("Bond lokal"),
        isLinkConnected: findYesNo("Link verbunden"),
        isLinkEncrypted: findYesNo("Link verschlüsselt"),
        isLinkBonded: findYesNo("Link gebondet"),
        peerIdentity: peerLine?.slice("Peer-ID:".length).trim(),
        lastError: errorLine?.slice("Letzter Fehler:".length).trim(),
        rawLines: lines,
    };
}

export function parseModeResponse(lines: readonly string[]): OutputMode | undefined {
    const line = lines.find((candidate) => candidate.startsWith("Ausgangsmodus:"));
    return line === undefined ? undefined : parseOutputMode(line.slice("Ausgangsmodus:".length));
}

export interface ProductProtocolOptions {
    responseIdleMillis?: number;
    responseTimeoutMillis?: number;
}

/** Commands and verification for the existing OneKlickPony firmware. */
export class ProductProtocol {
    private readonly responseIdleMillis: number;
    private readonly responseTimeoutMillis: number;
    private commandInProgress = false;

    constructor(private readonly connection: SerialConnection, options: ProductProtocolOptions = {}) {
        this.responseIdleMillis = options.responseIdleMillis ?? 120;
        this.responseTimeoutMillis = options.responseTimeoutMillis ?? 3000;
    }

    async readStatus(): Promise<ProductStatus> {
        const response = await this.sendAndCollect("status");
        const status = parseStatusResponse(response.lines);
        if (status === undefined) throw new ProductProtocolError("Die Antwort gehört nicht zu einer unterstützten OneKlickPony-Produktfirmware.", response.lines);
        return status;
    }

    async readMode(): Promise<OutputMode> {
        const response = await this.sendAndCollect("mode");
        const outputMode = parseModeResponse(response.lines);
        if (outputMode === undefined) throw new ProductProtocolError("Der Ausgangsmodus konnte nicht ausgelesen werden.", response.lines);
        return outputMode;
    }

    async setToggleMode(): Promise<ProductStatus> {
        await this.sendAndCollect("mode ledtoggle");
        return this.verifyMode({ kind: "toggle" });
    }

    async setPulseMode(durationMillis: number): Promise<ProductStatus> {
        const validDurationMillis = validatePulseDuration(durationMillis);
        await this.sendAndCollect(`mode ledpulse,${validDurationMillis}`);
        return this.verifyMode({ kind: "pulse", durationMillis: validDurationMillis });
    }

    async startPairing(): Promise<ProtocolResponse> {
        return this.sendAndCollect("pair");
    }

    async confirmPairing(): Promise<ProtocolResponse> {
        return this.sendAndCollect("confirm");
    }

    async cancelPairing(): Promise<ProtocolResponse> {
        return this.sendAndCollect("cancel");
    }

    async clearBinding(): Promise<ProtocolResponse> {
        return this.sendAndCollect("clear");
    }

    private async verifyMode(expectedMode: OutputMode): Promise<ProductStatus> {
        const status = await this.readStatus();
        const reportedMode = await this.readMode();
        if (!sameOutputMode(status.outputMode, expectedMode) || !sameOutputMode(reportedMode, expectedMode)) {
            throw new ProductProtocolError("Die gespeicherte Modusänderung konnte nicht bestätigt werden.", status.rawLines);
        }
        return status;
    }

    private async sendAndCollect(command: string): Promise<ProtocolResponse> {
        if (this.commandInProgress) throw new ProductProtocolError("Eine serielle Anfrage läuft bereits.");
        this.commandInProgress = true;
        const lines: string[] = [];
        let idleTimer: ReturnType<typeof setTimeout> | undefined;
        let timeoutTimer: ReturnType<typeof setTimeout> | undefined;
        let settled = false;
        try {
            return await new Promise<ProtocolResponse>((resolve, reject) => {
                const finish = (error?: Error): void => {
                    if (settled) return;
                    settled = true;
                    if (idleTimer !== undefined) clearTimeout(idleTimer);
                    if (timeoutTimer !== undefined) clearTimeout(timeoutTimer);
                    unsubscribe();
                    if (error !== undefined) reject(error);
                    else resolve({ command, lines });
                };
                const scheduleIdleFinish = (): void => {
                    if (idleTimer !== undefined) clearTimeout(idleTimer);
                    idleTimer = setTimeout(() => finish(), this.responseIdleMillis);
                };
                const unsubscribe = this.connection.onLine((line) => {
                    lines.push(line);
                    scheduleIdleFinish();
                });
                timeoutTimer = setTimeout(
                    () => finish(new ProductProtocolError("Die serielle Antwort hat zu lange gedauert.", lines)),
                    this.responseTimeoutMillis,
                );
                void this.connection.writeLine(command).catch((reason: unknown) => {
                    finish(reason instanceof Error ? reason : new Error(String(reason)));
                });
            });
        } finally {
            this.commandInProgress = false;
        }
    }
}

function sameOutputMode(left: OutputMode, right: OutputMode): boolean {
    return left.kind === right.kind && (left.kind !== "pulse" || left.durationMillis === (right as { durationMillis: number }).durationMillis);
}
