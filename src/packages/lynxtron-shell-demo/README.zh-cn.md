# Lynxtron Shell Demo

## 技术栈

- `lynx`
- `lynxtron`
- `Typscript`
- `React`
- `Rspeedy` + `Rsbuild` + `Electron-Builder`（构建与应用打包）

## 特性

/* WEB_SUPPORT_START */
- 同一套 UI 代码可运行在桌面端 (Node.js) 和 Web 端 (Browser)
/* WEB_SUPPORT_END */
- 一键运行、调试、打包
- 支持类型增强语言 Typescript
- 完善的工程化体验
- 支持通过 `electron-builder` 打包为各平台应用

## 环境准备

- NodeJS >= 22
- TypeScript
- [LynxDevTool](https://github.com/lynx-family/lynx-devtool/releases/) >= 0.1.1

## 使用指南

### 安装依赖

```bash
npm install
```

### 开发模式

- **桌面端 (Desktop)**
  ```bash
  npm run dev
  ```

/* WEB_SUPPORT_START */
- **Web 端 (Browser)**
  ```bash
  npm run dev:web
  ```
/* WEB_SUPPORT_END */

### 构建与启动

- **构建桌面端**

  ```bash
  npm run build
  ```

- **启动桌面端**
  ```bash
  npm start
  ```

/* WEB_SUPPORT_START */
- **构建 Web 端**
  ```bash
  npm run build:web
  ```

- **启动 Web 端**
  ```bash
  npm run start:web
  ```
/* WEB_SUPPORT_END */

### 应用打包

- **打包桌面应用**

  ```bash
  npm run pack
  ```
