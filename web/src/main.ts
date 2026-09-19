import "./styles/main.css";
import { getLanguage, setLanguage, translate } from "./i18n/i18n";
import { renderApplicationHeader } from "./components/applicationHeader";
import { createRouter } from "./router";

const application = document.querySelector<HTMLElement>("#app");
if (!application) throw new Error("Anwendungscontainer fehlt.");

let language = getLanguage();
let disposeRouter: (() => Promise<void>) | undefined;
const render = (): void => {
  void disposeRouter?.();
  const localized = (key: Parameters<typeof translate>[1], parameters?: Parameters<typeof translate>[2]) => translate(language, key, parameters);
  application.innerHTML = "<div id=\"header\"></div><div id=\"page\"></div><footer class=\"site-footer\"></footer>";
  renderApplicationHeader(application.querySelector<HTMLElement>("#header")!, language, localized, (nextLanguage) => { setLanguage(nextLanguage); language = nextLanguage; render(); });
  application.querySelector<HTMLElement>("footer")!.textContent = localized("footer.privacy");
  disposeRouter = createRouter(application.querySelector<HTMLElement>("#page")!, () => language, localized);
};

render();
