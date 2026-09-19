import { translate, type Language, type TranslationKey } from "../i18n/i18n";

interface VideoDefinition {
  readonly sourceUrl: string;
  readonly titleKey: TranslationKey;
  readonly descriptionKey: TranslationKey;
}

const videos: readonly VideoDefinition[] = [
  {
    sourceUrl: "https://www.youtube.com/watch?v=Bc23cLfiF6I",
    titleKey: "videos.dummy1.title",
    descriptionKey: "videos.dummy1.description",
  },
  {
    sourceUrl: "https://www.youtube.com/shorts/OBm9twsoMng",
    titleKey: "videos.dummy2.title",
    descriptionKey: "videos.dummy2.description",
  },
];

export function toYoutubeEmbedUrl(sourceUrl: string): string | undefined {
  const source = new URL(sourceUrl);
  if (!new Set(["www.youtube.com", "youtube.com", "m.youtube.com"]).has(source.hostname)) {
    return undefined;
  }

  const videoIdentifier = source.pathname === "/watch"
    ? source.searchParams.get("v")
    : source.pathname.match(/^\/shorts\/([A-Za-z0-9_-]{11})$/)?.[1];

  return videoIdentifier && /^[A-Za-z0-9_-]{11}$/.test(videoIdentifier)
    ? `https://www.youtube-nocookie.com/embed/${videoIdentifier}`
    : undefined;
}

function createVideoCard(video: VideoDefinition, language: Language): HTMLElement {
  const card = document.createElement("article");
  card.className = "card video-card";
  const heading = document.createElement("h2");
  heading.textContent = translate(language, video.titleKey);
  const description = document.createElement("p");
  description.textContent = translate(language, video.descriptionKey);
  const privacyNotice = document.createElement("p");
  privacyNotice.className = "video-card__privacy";
  privacyNotice.textContent = translate(language, "videos.privacy");
  const loadButton = document.createElement("button");
  loadButton.className = "action-button";
  loadButton.type = "button";
  loadButton.textContent = translate(language, "videos.load");

  loadButton.addEventListener("click", () => {
    const embedUrl = toYoutubeEmbedUrl(video.sourceUrl);
    if (!embedUrl) {
      return;
    }

    const frame = document.createElement("iframe");
    frame.src = embedUrl;
    frame.title = translate(language, video.titleKey);
    frame.loading = "lazy";
    frame.allow = "accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share";
    frame.allowFullscreen = true;
    frame.referrerPolicy = "strict-origin-when-cross-origin";
    frame.className = "video-card__embed";

    const frameContainer = document.createElement("div");
    frameContainer.className = "video-card__frame";
    frameContainer.append(frame);
    loadButton.replaceWith(frameContainer);
    privacyNotice.remove();
  });

  card.append(heading, description, privacyNotice, loadButton);
  return card;
}

export function renderVideosPage(container: HTMLElement, language: Language): void {
  const page = document.createElement("section");
  page.className = "page videos-page";
  const heading = document.createElement("h1");
  heading.textContent = translate(language, "videos.title");
  const introduction = document.createElement("p");
  introduction.className = "page-intro";
  introduction.textContent = translate(language, "videos.intro");
  const videoGrid = document.createElement("div");
  videoGrid.className = "content-grid videos-grid";

  for (const video of videos) {
    videoGrid.append(createVideoCard(video, language));
  }

  page.append(heading, introduction, videoGrid);
  container.replaceChildren(page);
}
