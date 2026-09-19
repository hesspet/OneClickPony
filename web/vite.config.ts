import { defineConfig } from "vite";

export default defineConfig({
  base: "/OneKlickPony/",
  test: {
    environment: "jsdom",
  },
});
