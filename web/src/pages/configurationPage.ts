import {
    maximumPulseDurationMillis,
    minimumPulseDurationMillis,
    OutputMode,
    ProductProtocol,
    ProductProtocolError,
    ProductStatus,
    validatePulseDuration,
} from "../serial/productProtocol";
import { SerialConnection, SerialPortLike } from "../serial/serialConnection";

export interface ConfigurationPageOptions {
    /** Receives a localisation key and a German fallback until the app i18n is connected. */
    translate?: (key: string, fallback: string) => string;
    requestPort?: () => Promise<SerialPortLike>;
}

export interface ConfigurationPageElement extends HTMLElement {
    dispose(): Promise<void>;
}

const text = (options: ConfigurationPageOptions, key: string, fallback: string): string =>
    options.translate?.(key, fallback) ?? fallback;

export function createConfigurationPage(options: ConfigurationPageOptions = {}): ConfigurationPageElement {
    const page = document.createElement("main") as ConfigurationPageElement;
    page.className = "configuration-page";
    page.innerHTML = `
        <section aria-labelledby="configuration-title">
            <h1 id="configuration-title">${text(options, "configuration.title", "OneKlickPony konfigurieren")}</h1>
            <p>${text(options, "configuration.introduction", "Verbinde das Gerät über USB und prüfe zuerst seinen Status.")}</p>
            <p id="configuration-message" role="status" aria-live="polite"></p>
            <div class="configuration-actions">
                <button type="button" id="configuration-connect">${text(options, "configuration.connect", "Gerät auswählen")}</button>
                <button type="button" id="configuration-refresh" disabled>${text(options, "configuration.refresh", "Status aktualisieren")}</button>
            </div>
            <dl id="configuration-status" hidden>
                <dt>${text(options, "configuration.firmware", "Firmware")}</dt><dd data-status="firmware"></dd>
                <dt>${text(options, "configuration.state", "Zustand")}</dt><dd data-status="state"></dd>
                <dt>${text(options, "configuration.assignment", "Zuordnung")}</dt><dd data-status="bound"></dd>
                <dt>${text(options, "configuration.mode", "Modus")}</dt><dd data-status="mode"></dd>
            </dl>
        </section>
        <section aria-labelledby="configuration-mode-title">
            <h2 id="configuration-mode-title">${text(options, "configuration.mode.title", "Ausgangsmodus")}</h2>
            <label for="configuration-mode">${text(options, "configuration.mode.label", "Funktion")}</label>
            <select id="configuration-mode" disabled>
                <option value="toggle">${text(options, "configuration.mode.toggle", "Umschalten")}</option>
                <option value="pulse">${text(options, "configuration.mode.pulse", "Impuls")}</option>
            </select>
            <label for="configuration-duration">${text(options, "configuration.duration.label", "Impulsdauer in Millisekunden")}</label>
            <input id="configuration-duration" type="number" inputmode="numeric" min="${minimumPulseDurationMillis}" max="${maximumPulseDurationMillis}" step="1" value="750" disabled>
            <button type="button" id="configuration-save-mode" disabled>${text(options, "configuration.mode.save", "Modus speichern")}</button>
        </section>
        <section aria-labelledby="configuration-pairing-title">
            <h2 id="configuration-pairing-title">${text(options, "configuration.pairing.title", "Pairing")}</h2>
            <p>${text(options, "configuration.pairing.help", "Starte Pairing nur, wenn der gewünschte Auslöser bereitliegt. Nach dem Lernen bestätigst du das Profil.")}</p>
            <div class="configuration-actions">
                <button type="button" id="configuration-pair" disabled>${text(options, "configuration.pairing.start", "Pairing starten")}</button>
                <button type="button" id="configuration-confirm" disabled>${text(options, "configuration.pairing.confirm", "Profil bestätigen")}</button>
                <button type="button" id="configuration-cancel" disabled>${text(options, "configuration.pairing.cancel", "Pairing abbrechen")}</button>
            </div>
        </section>
        <section aria-labelledby="configuration-clear-title">
            <h2 id="configuration-clear-title">${text(options, "configuration.clear.title", "Zuordnung löschen")}</h2>
            <p id="configuration-clear-warning">${text(options, "configuration.clear.warning", "Dabei werden die Zuordnung und gespeicherte Bonds dauerhaft entfernt.")}</p>
            <button type="button" id="configuration-clear" disabled>${text(options, "configuration.clear.prepare", "Löschen vorbereiten")}</button>
            <button type="button" id="configuration-clear-confirm" hidden>${text(options, "configuration.clear.confirm", "Löschen endgültig bestätigen")}</button>
            <button type="button" id="configuration-clear-cancel" hidden>${text(options, "configuration.clear.cancel", "Löschen abbrechen")}</button>
        </section>
        <details>
            <summary>${text(options, "configuration.technical.title", "Technische Ausgabe")}</summary>
            <pre id="configuration-technical-output" aria-label="${text(options, "configuration.technical.output", "Unbearbeitete serielle Ausgabe")}"></pre>
            <button type="button" id="configuration-copy-output">${text(options, "configuration.technical.copy", "Technische Ausgabe kopieren")}</button>
            <button type="button" id="configuration-clear-output">${text(options, "configuration.technical.clear", "Technische Ausgabe leeren")}</button>
        </details>
    `;

    const byId = <ElementType extends HTMLElement>(identifier: string): ElementType => {
        const element = page.querySelector<ElementType>(`#${identifier}`);
        if (element === null) throw new Error(`Konfigurationselement '${identifier}' fehlt.`);
        return element;
    };
    const connectButton = byId<HTMLButtonElement>("configuration-connect");
    const refreshButton = byId<HTMLButtonElement>("configuration-refresh");
    const modeSelect = byId<HTMLSelectElement>("configuration-mode");
    const durationInput = byId<HTMLInputElement>("configuration-duration");
    const saveModeButton = byId<HTMLButtonElement>("configuration-save-mode");
    const pairingButtons = ["configuration-pair", "configuration-confirm", "configuration-cancel"].map(byId<HTMLButtonElement>);
    const clearButton = byId<HTMLButtonElement>("configuration-clear");
    const clearConfirmButton = byId<HTMLButtonElement>("configuration-clear-confirm");
    const clearCancelButton = byId<HTMLButtonElement>("configuration-clear-cancel");
    const message = byId<HTMLElement>("configuration-message");
    const statusView = byId<HTMLDListElement>("configuration-status");
    const technicalOutput = byId<HTMLPreElement>("configuration-technical-output");
    const copyOutputButton = byId<HTMLButtonElement>("configuration-copy-output");
    const clearOutputButton = byId<HTMLButtonElement>("configuration-clear-output");
    let connection: SerialConnection | undefined;
    let protocol: ProductProtocol | undefined;
    let clearPrepared = false;
    let actionInProgress = false;

    const setMessage = (value: string, isError = false): void => {
        message.textContent = value;
        message.dataset.kind = isError ? "error" : "success";
    };
    const appendTechnicalLine = (line: string): void => {
        const timestamp = new Intl.DateTimeFormat("de-DE", { hour: "2-digit", minute: "2-digit", second: "2-digit" }).format(new Date());
        technicalOutput.textContent += `[${timestamp}] ${line}\n`;
    };
    const setControlsEnabled = (enabled: boolean): void => {
        refreshButton.disabled = !enabled;
        modeSelect.disabled = !enabled;
        durationInput.disabled = !enabled || modeSelect.value !== "pulse";
        saveModeButton.disabled = !enabled;
        pairingButtons.forEach((button) => (button.disabled = !enabled));
        clearButton.disabled = !enabled;
        clearConfirmButton.disabled = !enabled;
        clearCancelButton.disabled = !enabled;
    };
    const updateClearConfirmation = (): void => {
        clearButton.hidden = clearPrepared;
        clearConfirmButton.hidden = !clearPrepared;
        clearCancelButton.hidden = !clearPrepared;
    };
    const displayMode = (outputMode: OutputMode): string =>
        outputMode.kind === "toggle" ? text(options, "configuration.mode.toggle", "Umschalten") : `${text(options, "configuration.mode.pulse", "Impuls")} ${outputMode.durationMillis} ms`;
    const renderStatus = (status: ProductStatus): void => {
        statusView.hidden = false;
        statusView.querySelector<HTMLElement>("[data-status='firmware']")!.textContent = status.firmwareVersion;
        statusView.querySelector<HTMLElement>("[data-status='state']")!.textContent = status.state;
        statusView.querySelector<HTMLElement>("[data-status='bound']")!.textContent = status.isBound ? text(options, "common.yes", "Ja") : text(options, "common.no", "Nein");
        statusView.querySelector<HTMLElement>("[data-status='mode']")!.textContent = displayMode(status.outputMode);
        modeSelect.value = status.outputMode.kind;
        if (status.outputMode.kind === "pulse") durationInput.value = String(status.outputMode.durationMillis);
        durationInput.disabled = modeSelect.value !== "pulse";
    };
    const explainError = (reason: unknown): string => {
        if (reason instanceof ProductProtocolError) {
            reason.lines.forEach(appendTechnicalLine);
            return reason.message;
        }
        return reason instanceof Error ? reason.message : String(reason);
    };
    const runAction = async (work: () => Promise<void>): Promise<void> => {
        if (actionInProgress) return;
        actionInProgress = true;
        setControlsEnabled(false);
        try {
            await work();
        } catch (reason) {
            setMessage(explainError(reason), true);
        } finally {
            actionInProgress = false;
            setControlsEnabled(protocol !== undefined);
        }
    };
    const refreshStatus = async (): Promise<void> => {
        if (protocol === undefined) return;
        const status = await protocol.readStatus();
        await protocol.readMode();
        renderStatus(status);
        setMessage(text(options, "configuration.status.loaded", "Gerätestatus wurde aktualisiert."));
    };
    const requestPort = options.requestPort ?? (async (): Promise<SerialPortLike> => {
        const serial = (navigator as Navigator & { serial?: { requestPort(): Promise<SerialPortLike> } }).serial;
        if (serial === undefined) throw new Error(text(options, "configuration.serialUnavailable", "Web Serial ist in diesem Browser nicht verfügbar."));
        return serial.requestPort();
    });

    connectButton.addEventListener("click", () => void runAction(async () => {
        await connection?.close();
        const port = await requestPort();
        connection = new SerialConnection(port, {
            onLine: appendTechnicalLine,
            onDisconnect: () => {
                protocol = undefined;
                setControlsEnabled(false);
                setMessage(text(options, "configuration.disconnected", "Die USB-Verbindung wurde getrennt."), true);
            },
        });
        await connection.open();
        protocol = new ProductProtocol(connection);
        await refreshStatus();
    }));
    refreshButton.addEventListener("click", () => void runAction(refreshStatus));
    modeSelect.addEventListener("change", () => {
        durationInput.disabled = modeSelect.value !== "pulse" || protocol === undefined;
    });
    saveModeButton.addEventListener("click", () => void runAction(async () => {
        if (protocol === undefined) return;
        const status = modeSelect.value === "toggle"
            ? await protocol.setToggleMode()
            : await protocol.setPulseMode(validatePulseDuration(Number(durationInput.value)));
        renderStatus(status);
        setMessage(text(options, "configuration.mode.saved", "Der Modus wurde gespeichert und erneut geprüft."));
    }));
    pairingButtons[0].addEventListener("click", () => void runAction(async () => {
        if (protocol === undefined) return;
        await protocol.startPairing();
        setMessage(text(options, "configuration.pairing.started", "Pairing wurde gestartet. Folge nun den Hinweisen des Geräts."));
    }));
    pairingButtons[1].addEventListener("click", () => void runAction(async () => {
        if (protocol === undefined) return;
        await protocol.confirmPairing();
        await refreshStatus();
    }));
    pairingButtons[2].addEventListener("click", () => void runAction(async () => {
        if (protocol === undefined) return;
        await protocol.cancelPairing();
        await refreshStatus();
    }));
    clearButton.addEventListener("click", () => {
        clearPrepared = true;
        updateClearConfirmation();
        setMessage(text(options, "configuration.clear.secondWarning", "Bitte bestätige das endgültige Löschen bewusst ein zweites Mal."), true);
    });
    clearCancelButton.addEventListener("click", () => {
        clearPrepared = false;
        updateClearConfirmation();
        setMessage(text(options, "configuration.clear.cancelled", "Löschen wurde abgebrochen."));
    });
    clearConfirmButton.addEventListener("click", () => void runAction(async () => {
        if (protocol === undefined || !clearPrepared) return;
        await protocol.clearBinding();
        clearPrepared = false;
        updateClearConfirmation();
        await refreshStatus();
        setMessage(text(options, "configuration.clear.done", "Zuordnung und Bonds wurden gelöscht."));
    }));
    copyOutputButton.addEventListener("click", () => void runAction(async () => {
        await navigator.clipboard.writeText(technicalOutput.textContent ?? "");
        setMessage(text(options, "configuration.technical.copied", "Technische Ausgabe wurde kopiert."));
    }));
    clearOutputButton.addEventListener("click", () => {
        technicalOutput.textContent = "";
    });
    page.dispose = async (): Promise<void> => {
        protocol = undefined;
        await connection?.close();
        connection = undefined;
    };
    setControlsEnabled(false);
    updateClearConfirmation();
    return page;
}
