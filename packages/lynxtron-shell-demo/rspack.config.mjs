import { defineConfig } from "@rspack/cli";
import { rspack } from "@rspack/core";
import * as path from "path";
import { fileURLToPath } from "url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const SOURCE = path.resolve(__dirname, "src");
const isDev = process.env.NODE_ENV === "development";
const devReadyRspack = await (async () => {
  try {
    const mod = await import('@lynx-js/dev-ready-rsbuild-plugin/rspack')
    return mod.rspackDevReadyPlugin
  } catch {
    const mod = await import('../dev-ready-rsbuild-plugin/dist/rspack.js')
    return mod.rspackDevReadyPlugin
  }
})()

export default defineConfig({
  target: "electron-main",
  entry: {
    main: "./src/main/main.ts",
  },
  output: {
    path: path.resolve(__dirname, "dist/assets/"),
    filename: "[name].js",
    module: true,
  },
  module: {
    rules: [
      {
        test: /\.svg$/,
        type: "asset",
      },
    ],
  },
  plugins: [
    new rspack.CopyRspackPlugin({
      patterns: [
        { from: "./package.json", to: "package.json" },
        { from: "./output/bundle/"},
      ],
    }),
    devReadyRspack({}),
  ],
  optimization: {
    minimize: !isDev,
    nodeEnv: false,
  },
  experiments: {
    css: true,
  },
});
