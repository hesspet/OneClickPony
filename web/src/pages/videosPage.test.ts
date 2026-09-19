import { describe, expect, it } from "vitest";
import { toYoutubeEmbedUrl } from "./videosPage";

describe("toYoutubeEmbedUrl", () => {
  it("converts a normal YouTube address to the no-cookie embed address", () => {
    expect(toYoutubeEmbedUrl("https://www.youtube.com/watch?v=Bc23cLfiF6I"))
      .toBe("https://www.youtube-nocookie.com/embed/Bc23cLfiF6I");
  });

  it("converts a YouTube Shorts address to the no-cookie embed address", () => {
    expect(toYoutubeEmbedUrl("https://www.youtube.com/shorts/OBm9twsoMng"))
      .toBe("https://www.youtube-nocookie.com/embed/OBm9twsoMng");
  });

  it("rejects unsupported hosts and malformed video identifiers", () => {
    expect(toYoutubeEmbedUrl("https://example.com/watch?v=Bc23cLfiF6I")).toBeUndefined();
    expect(toYoutubeEmbedUrl("https://www.youtube.com/shorts/not-valid")).toBeUndefined();
  });
});
