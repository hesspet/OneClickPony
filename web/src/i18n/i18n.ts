import de from "./de.json";
import en from "./en.json";

export type Language = "de" | "en";
export type TranslationKey = keyof typeof de;

const dictionaries: Record<Language, Record<TranslationKey, string>> = { de, en };
const storageKey = "oneklickpony-language";

export function getLanguage(): Language {
  return localStorage.getItem(storageKey) === "en" ? "en" : "de";
}

export function setLanguage(language: Language): void {
  localStorage.setItem(storageKey, language);
}

export function translate(language: Language, key: TranslationKey, parameters: Record<string, string | number> = {}): string {
  return dictionaries[language][key].replace(/\{(\w+)\}/g, (_, name: string) => String(parameters[name] ?? `{${name}}`));
}

export function validateTranslationKeys(): string[] {
  const germanKeys = Object.keys(de).sort();
  const englishKeys = Object.keys(en).sort();
  return [...new Set([...germanKeys, ...englishKeys])].filter((key) => !(key in de) || !(key in en));
}
