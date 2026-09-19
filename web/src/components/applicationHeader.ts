import type { Language, TranslationKey } from "../i18n/i18n";

type Translate = (key: TranslationKey) => string;

const routes: Array<{ hash: string; label: TranslationKey }> = [
  { hash: "#/", label: "navigation.home" },
  { hash: "#/flashen", label: "navigation.flash" },
  { hash: "#/konfigurieren", label: "navigation.configuration" },
  { hash: "#/anleitungen", label: "navigation.guides" },
  { hash: "#/youtube", label: "navigation.videos" },
];

export function renderApplicationHeader(container: HTMLElement, language: Language, translate: Translate, onLanguageChange: (language: Language) => void): void {
  const currentHash = location.hash || "#/";
  container.innerHTML = `<header class="site-header"><div class="header-inner"><a class="brand" href="#/">${translate("application.name")}</a><button class="menu-toggle" type="button" aria-expanded="false" aria-controls="main-navigation">${translate("menu.open")}</button><nav id="main-navigation" class="main-navigation" aria-label="${translate("navigation.home")}">${routes.map((route) => `<a href="${route.hash}" ${currentHash === route.hash ? 'aria-current="page"' : ""}>${translate(route.label)}</a>`).join("")}<label><span class="visually-hidden">${translate("navigation.language")}</span><select class="language-select" aria-label="${translate("navigation.language")}"><option value="de" ${language === "de" ? "selected" : ""}>${translate("language.de")}</option><option value="en" ${language === "en" ? "selected" : ""}>${translate("language.en")}</option></select></label></nav></div></header>`;
  const navigation = container.querySelector<HTMLElement>(".main-navigation")!;
  const menuButton = container.querySelector<HTMLButtonElement>(".menu-toggle")!;
  menuButton.addEventListener("click", () => { const isOpen = navigation.dataset.open === "true"; navigation.dataset.open = String(!isOpen); menuButton.setAttribute("aria-expanded", String(!isOpen)); menuButton.textContent = translate(isOpen ? "menu.open" : "menu.close"); });
  container.querySelector<HTMLSelectElement>("select")!.addEventListener("change", (event) => onLanguageChange((event.currentTarget as HTMLSelectElement).value as Language));
}
