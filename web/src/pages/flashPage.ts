import type { Language, TranslationKey } from "../i18n/i18n";
import { renderCapabilityNotice } from "../components/capabilityNotice";
import { renderStatusMessage } from "../components/statusMessage";
import { loadStableFirmware } from "../flash/firmwareCatalog";
import { FirmwareFlasher } from "../flash/firmwareFlasher";
import { SerialConnection } from "../serial/serialConnection";
import { ProductProtocol } from "../serial/productProtocol";
import { getPreviouslyApprovedPorts } from "../serial/serialCapability";

export function renderFlashPage(container: HTMLElement, language: Language, translate: (key: TranslationKey, parameters?: Record<string, string | number>) => string): void {
  container.innerHTML = `<main id="inhalt" class="page"><p class="eyebrow">${translate("navigation.flash")}</p><h1>${translate("flash.title")}</h1><p class="lead">${translate("flash.intro")}</p><section class="card"><h2>${translate("flash.requirements.title")}</h2><ul class="checklist"><li>${translate("flash.requirements.cable")}</li><li>${translate("flash.requirements.direct")}</li><li>${translate("flash.requirements.monitor")}</li><li>${translate("flash.requirements.power")}</li></ul></section><div id="capability"></div><div id="release"></div><p id="known-ports"></p><p><button id="choose-device" class="action-button" type="button">${translate("flash.choose")}</button> <button id="flash-device" class="secondary-button" type="button" disabled>${translate("flash.start")}</button> <button id="verify-device" class="secondary-button" type="button" disabled>${translate("flash.verify")}</button></p><div id="progress" hidden><div class="progress" aria-label="Fortschritt"><span style="width: 0%"></span></div><p></p></div><div id="status"></div></main>`;
  if (!renderCapabilityNotice(container.querySelector<HTMLElement>("#capability")!, language, translate)) return;
  const status = container.querySelector<HTMLElement>("#status")!;
  const releaseElement = container.querySelector<HTMLElement>("#release")!;
  const chooseButton = container.querySelector<HTMLButtonElement>("#choose-device")!;
  const flashButton = container.querySelector<HTMLButtonElement>("#flash-device")!;
  const verifyButton = container.querySelector<HTMLButtonElement>("#verify-device")!;
  const progress = container.querySelector<HTMLElement>("#progress")!;
  const progressBar = progress.querySelector<HTMLElement>("span")!;
  const progressText = progress.querySelector<HTMLParagraphElement>("p")!;
  const flasher = new FirmwareFlasher();
  let manifestLoaded = false;
  let expectedVersion = "";
  void getPreviouslyApprovedPorts().then((ports) => { container.querySelector<HTMLElement>("#known-ports")!.textContent = ports.length > 0 ? translate("flash.knownPorts", { count: ports.length }) : ""; });
  void loadStableFirmware().then(({ release }) => { manifestLoaded = true; expectedVersion = release.version; releaseElement.innerHTML = `<section class="card"><strong>${release.productName} ${release.version}</strong><p>${release.releasedAt} · ${release.releaseNotes[language]}</p></section>`; }).catch(() => renderStatusMessage(status, translate("flash.catalogError"), "error"));
  chooseButton.addEventListener("click", async () => { try { renderStatusMessage(status, translate("flash.detecting")); const port = await (navigator as SerialNavigator).serial!.requestPort(); const chip = await flasher.detect(port, () => undefined); renderStatusMessage(status, translate("flash.detected", { device: chip }), "success"); flashButton.disabled = !manifestLoaded; } catch (error) { renderStatusMessage(status, error instanceof Error && /Falsche Chipfamilie/.test(error.message) ? translate("flash.wrongChip") : translate("flash.failed"), "error"); await flasher.close().catch(() => undefined); } });
  flashButton.addEventListener("click", async () => { try { const { manifest } = await loadStableFirmware(); flashButton.disabled = true; chooseButton.disabled = true; progress.hidden = false; await flasher.flash(manifest, ({ percent, detail }) => { progressBar.style.width = `${percent}%`; progressText.textContent = translate("flash.inProgress", { progress: percent }) + ` ${detail}`; }); renderStatusMessage(status, translate("flash.finished"), "success"); verifyButton.disabled = false; } catch { renderStatusMessage(status, translate("flash.failed"), "error"); } finally { await flasher.close().catch(() => undefined); chooseButton.disabled = false; } });
  verifyButton.addEventListener("click", async () => { let connection: SerialConnection | undefined; try { const port = await (navigator as SerialNavigator).serial!.requestPort(); connection = new SerialConnection(port); await connection.open(); const reportedVersion = (await new ProductProtocol(connection).readStatus()).firmwareVersion; renderStatusMessage(status, reportedVersion === expectedVersion ? translate("flash.verifySuccess") : translate("flash.verifyFailed"), reportedVersion === expectedVersion ? "success" : "error"); } catch { renderStatusMessage(status, translate("flash.verifyFailed"), "error"); } finally { await connection?.close().catch(() => undefined); } });
}
