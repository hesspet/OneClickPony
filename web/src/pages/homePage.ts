import type { Language, TranslationKey } from "../i18n/i18n";
import { renderCapabilityNotice } from "../components/capabilityNotice";

export function renderHomePage(container: HTMLElement, language: Language, translate: (key: TranslationKey) => string): void {
  container.innerHTML = `<main id="inhalt" class="page"><p class="eyebrow">${translate("home.eyebrow")}</p><h1>${translate("home.title")}</h1><p class="lead">${translate("home.intro")}</p><div id="capability"></div><section class="content-grid" aria-label="Aktionen">${[
    ["home.flash.title", "home.flash.body", "#/flashen"], ["home.configuration.title", "home.configuration.body", "#/konfigurieren"], ["home.guides.title", "home.guides.body", "#/anleitungen"], ["home.videos.title", "home.videos.body", "#/youtube"],
  ].map(([title, body, hash]) => `<article class="card"><h2>${translate(title as TranslationKey)}</h2><p>${translate(body as TranslationKey)}</p><a class="secondary-button" href="${hash}">${translate("home.open")}</a></article>`).join("")}</section></main>`;
  renderCapabilityNotice(container.querySelector<HTMLElement>("#capability")!, language, translate);
}
