import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  server: {
    // Local dev: proxy /api to the deployed test backend
    proxy: {
      "/api": { target: "http://192.168.0.116:8050", changeOrigin: true },
    },
  },
});
