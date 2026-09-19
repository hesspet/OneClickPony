import type { Language, TranslationKey } from "../i18n/i18n";
import { getSerialCapability } from "../serial/serialCapability";

export function renderCapabilityNotice(container: HTMLElement, language: Language, translate: (key: TranslationKey) => string): boolean {
  const capability = getSerialCapability();
  const prefix = capability.kind === "ready" ? "capability.ready" : capability.kind === "insecure" ? "capability.insecure" : "capability.unsupported";
  container.innerHTML = `<section class="status-message" data-kind="${capability.kind === "ready" ? "success" : "error"}" role="status"><span aria-hidden="true">${capability.kind === "ready" ? "✓" : "!"}</span><div><strong>${translate(`${prefix}.title` as TranslationKey)}</strong><p>${translate(`${prefix}.body` as TranslationKey)}</p></div></section>`;
  return capability.kind === "ready";
}
