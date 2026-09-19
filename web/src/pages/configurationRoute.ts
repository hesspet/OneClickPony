import { createConfigurationPage, type ConfigurationPageElement } from "./configurationPage";
import type { Language, TranslationKey } from "../i18n/i18n";

export function renderConfigurationPage(container: HTMLElement, language: Language, translate: (key: TranslationKey) => string): () => Promise<void> {
  const page: ConfigurationPageElement = createConfigurationPage({
    translate: (key, fallback) => translate(key as TranslationKey) || fallback,
  });
  page.classList.add("page");
  container.replaceChildren(page);
  return () => page.dispose();
}
