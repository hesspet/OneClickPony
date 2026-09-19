import connectionGuideGerman from "./de/integration-beispiel-1.md?raw";
import installationGuideGerman from "./de/integration-beispiel-2.md?raw";
import connectionGuideEnglish from "./en/integration-example-1.md?raw";
import installationGuideEnglish from "./en/integration-example-2.md?raw";
import connectionImageUrl from "../assets/guides/beispiel-anschluss.svg";
import installationImageUrl from "../assets/guides/beispiel-einbau.svg";
import type { Language, TranslationKey } from "../i18n/i18n";

export interface GuideArticle {
  readonly id: "connection" | "installation";
  readonly titleKey: TranslationKey;
  readonly descriptionKey: TranslationKey;
  readonly markdown: Record<Language, string>;
  readonly images: Readonly<Record<string, string>>;
}

export const guideArticles: readonly GuideArticle[] = [
  {
    id: "connection",
    titleKey: "guides.article.connection.title",
    descriptionKey: "guides.article.connection.description",
    markdown: {
      de: connectionGuideGerman,
      en: connectionGuideEnglish,
    },
    images: { "guide-connection": connectionImageUrl },
  },
  {
    id: "installation",
    titleKey: "guides.article.installation.title",
    descriptionKey: "guides.article.installation.description",
    markdown: {
      de: installationGuideGerman,
      en: installationGuideEnglish,
    },
    images: { "guide-installation": installationImageUrl },
  },
];
