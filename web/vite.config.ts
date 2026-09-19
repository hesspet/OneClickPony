import { defineConfig } from "vite";

export default defineConfig({
  base: "/OneClickPony/",
  test: {
    environment: "jsdom",
  },
});
