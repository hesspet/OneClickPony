import { createTableOfContents, renderSafeMarkdown } from "../components/safeMarkdown";
import { guideArticles, type GuideArticle } from "../content/guides";
import { translate, type Language } from "../i18n/i18n";

function renderArticle(container: HTMLElement, language: Language, article: GuideArticle): void {
  container.replaceChildren();

  const articleElement = document.createElement("article");
  articleElement.className = "card guide-article";
  articleElement.innerHTML = renderSafeMarkdown(article.markdown[language], article.images);

  const image = articleElement.querySelector<HTMLImageElement>("img");
  if (image) {
    image.loading = "lazy";
    image.decoding = "async";
  }

  const tableOfContents = createTableOfContents(articleElement);
  if (tableOfContents) {
    const navigation = document.createElement("nav");
    navigation.className = "guide-table-of-contents-container";
    navigation.setAttribute("aria-label", translate(language, "guides.contents"));
    const heading = document.createElement("h2");
    heading.textContent = translate(language, "guides.contents");
    navigation.append(heading, tableOfContents);
    container.append(navigation);
  }

  container.append(articleElement);
}

export function renderGuidesPage(container: HTMLElement, language: Language): void {
  const page = document.createElement("section");
  page.className = "page guides-page";

  const heading = document.createElement("h1");
  heading.textContent = translate(language, "guides.title");
  const introduction = document.createElement("p");
  introduction.className = "page-intro";
  introduction.textContent = translate(language, "guides.intro");

  const layout = document.createElement("div");
  layout.className = "content-grid guides-layout";
  const list = document.createElement("aside");
  list.className = "card guides-list";
  const listHeading = document.createElement("h2");
  listHeading.textContent = translate(language, "guides.listTitle");
  const choices = document.createElement("div");
  choices.className = "guides-list__choices";
  list.append(listHeading, choices);

  const articleContainer = document.createElement("div");
  articleContainer.className = "guides-reading-area";
  let selectedArticle = guideArticles[0];

  for (const article of guideArticles) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "secondary-button guide-choice";
    const title = document.createElement("span");
    title.className = "guide-choice__title";
    title.textContent = translate(language, article.titleKey);
    const description = document.createElement("span");
    description.className = "guide-choice__description";
    description.textContent = translate(language, article.descriptionKey);
    button.append(title, description);
    button.setAttribute("aria-pressed", String(article === selectedArticle));
    button.addEventListener("click", () => {
      selectedArticle = article;
      for (const choice of Array.from(choices.querySelectorAll<HTMLButtonElement>("button"))) {
        choice.setAttribute("aria-pressed", String(choice === button));
      }
      renderArticle(articleContainer, language, selectedArticle);
    });
    choices.append(button);
  }

  renderArticle(articleContainer, language, selectedArticle);
  layout.append(list, articleContainer);
  page.append(heading, introduction, layout);
  container.replaceChildren(page);
}
