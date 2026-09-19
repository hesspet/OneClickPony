import type { Language, TranslationKey } from "./i18n/i18n";
import { renderHomePage } from "./pages/homePage";
import { renderFlashPage } from "./pages/flashPage";
import { renderConfigurationPage } from "./pages/configurationRoute";
import { renderGuidesPage } from "./pages/guidesPage";
import { renderVideosPage } from "./pages/videosPage";

type Translate = (key: TranslationKey, parameters?: Record<string, string | number>) => string;

export function createRouter(container: HTMLElement, language: () => Language, translate: Translate): () => Promise<void> {
  let disposeCurrentPage: (() => Promise<void>) | undefined;
  const render = async (): Promise<void> => {
    await disposeCurrentPage?.();
    disposeCurrentPage = undefined;
    switch (location.hash || "#/") {
      case "#/flashen": renderFlashPage(container, language(), translate); break;
      case "#/konfigurieren": disposeCurrentPage = renderConfigurationPage(container, language(), translate); break;
      case "#/anleitungen": renderGuidesPage(container, language()); break;
      case "#/youtube": renderVideosPage(container, language()); break;
      default: renderHomePage(container, language(), translate);
    }
  };
  const handleHashChange = (): void => { void render(); };
  window.addEventListener("hashchange", handleHashChange);
  void render();
  return async () => {
    window.removeEventListener("hashchange", handleHashChange);
    await disposeCurrentPage?.();
  };
}
