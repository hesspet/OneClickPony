import { describe, expect, it } from "vitest";
import { validateTranslationKeys } from "./i18n";

describe("Lokalisierung", () => {
  it("enthält in Deutsch und Englisch dieselben Schlüssel", () => {
    expect(validateTranslationKeys()).toEqual([]);
  });
});
