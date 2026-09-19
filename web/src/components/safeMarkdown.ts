import MarkdownIt from "markdown-it";

const markdownRenderer = new MarkdownIt({
  html: false,
  linkify: false,
  typographer: false,
});

/**
 * Renders only repository-owned Markdown. Raw HTML is disabled, so authored
 * guide content cannot introduce scripts, event handlers, or arbitrary tags.
 */
export function renderSafeMarkdown(markdown: string, imageUrls: Readonly<Record<string, string>>): string {
  const rendered = markdownRenderer.render(markdown);
  return rendered.replace(/(<img\b[^>]*?\bsrc=")(guide-(?:connection|installation))("[^>]*>)/g,
    (match, prefix: string, imageKey: string, suffix: string) => {
      const imageUrl = imageUrls[imageKey];
      if (imageUrl === undefined) return match;
      return `${prefix}${escapeAttribute(imageUrl)}${suffix}`;
    });
}

function escapeAttribute(value: string): string {
  return value.replace(/&/g, "&amp;").replace(/"/g, "&quot;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

export function createTableOfContents(article: HTMLElement): HTMLUListElement | undefined {
  const headings = Array.from(article.querySelectorAll<HTMLHeadingElement>("h2, h3"));
  if (headings.length < 2) {
    return undefined;
  }

  const tableOfContents = document.createElement("ul");
  tableOfContents.className = "guide-table-of-contents";

  headings.forEach((heading, index) => {
    const identifier = `guide-section-${index + 1}`;
    heading.id = identifier;

    const item = document.createElement("li");
    item.className = heading.tagName === "H3" ? "guide-table-of-contents__subitem" : "";
    const link = document.createElement("a");
    link.href = `#${identifier}`;
    link.textContent = heading.textContent ?? "";
    item.append(link);
    tableOfContents.append(item);
  });

  return tableOfContents;
}
