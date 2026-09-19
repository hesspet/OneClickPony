import { describe, expect, it } from "vitest";
import {
    maximumPulseDurationMillis,
    minimumPulseDurationMillis,
    parseModeResponse,
    parseStatusResponse,
    validatePulseDuration,
} from "./productProtocol";
import { LineBuffer } from "./serialConnection";

describe("LineBuffer", () => {
    it("keeps partial lines and accepts CRLF across chunks", () => {
        const buffer = new LineBuffer();

        expect(buffer.push("Firm")).toEqual([]);
        expect(buffer.push("ware 0.5.0\r")).toEqual(["Firmware 0.5.0"]);
        expect(buffer.push("Ausgangsmodus: Impuls 750\nLetzter")).toEqual(["Ausgangsmodus: Impuls 750"]);
        expect(buffer.finish()).toEqual(["Letzter"]);
    });
});

describe("OneKlickPony product protocol parser", () => {
    const statusLines = [
        "Firmware 0.5.0, Runtime=bereit, Zustand=bereit, Zuordnung=ja, Modus=Impuls 750, Ausgang=inaktiv",
        "Bond lokal: ja",
        "Link verbunden: nein",
        "Link verschlüsselt: nein",
        "Link gebondet: nein",
        "Peer-ID: nicht verbunden",
    ];

    it("parses the complete current status response", () => {
        expect(parseStatusResponse(statusLines)).toMatchObject({
            firmwareVersion: "0.5.0",
            runtime: "bereit",
            state: "bereit",
            isBound: true,
            outputMode: { kind: "pulse", durationMillis: 750 },
            hasLocalBond: true,
            isLinkConnected: false,
        });
    });

    it("rejects incomplete or unsupported status lines", () => {
        expect(parseStatusResponse(["Firmware 0.5.0, Runtime=bereit"])).toBeUndefined();
        expect(parseModeResponse(["Ausgangsmodus: Servo"])).toBeUndefined();
        expect(parseModeResponse(["Ausgangsmodus: Toggle"])).toEqual({ kind: "toggle" });
    });

    it("allows both valid duration boundaries and rejects invalid values", () => {
        expect(validatePulseDuration(minimumPulseDurationMillis)).toBe(minimumPulseDurationMillis);
        expect(validatePulseDuration(maximumPulseDurationMillis)).toBe(maximumPulseDurationMillis);
        expect(() => validatePulseDuration(0)).toThrow();
        expect(() => validatePulseDuration(60001)).toThrow();
        expect(() => validatePulseDuration(1.5)).toThrow();
    });
});
