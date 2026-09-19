import { describe, expect, it } from "vitest";
import { renderSafeMarkdown } from "./safeMarkdown";

describe("renderSafeMarkdown", () => {
  it("does not render raw HTML from Markdown", () => {
    const rendered = renderSafeMarkdown("# Titel\n\n<script>alert('x')</script>", {});

    expect(rendered).not.toContain("<script>");
    expect(rendered).toContain("&lt;script&gt;");
  });

  it("uses a bundled local image for a known image key", () => {
    const rendered = renderSafeMarkdown("![Lokales Bild](guide-connection)", {
      "guide-connection": "/assets/connection.svg",
    });

    expect(rendered).toContain('src="/assets/connection.svg"');
    expect(rendered).toContain('alt="Lokales Bild"');
  });
});
