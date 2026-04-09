# Lynxtron Shell Demo

## 技术栈

- `lynx`
- `lynxtron`
- `Typscript`
- `React`
- `Rspack` + `Electron-Builder` (JS 压缩与应用打包)

## 特性

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

### 一键启动

执行以下命令可以一键完成代码编译并启动应用。

```bash
npm run start
```

### 调试

本项目支持对 Lynx (渲染进程) 和 Lynxtron (主进程) 的分别或同时调试。

- **同时调试 Lynx 和 Lynxtron (推荐)**

  在一个终端中执行以下命令，将会同时以调试模式启动 Lynx 和 Lynxtron，并支持热更新。

  ```bash
  npm run dev
  ```

- **仅调试 Lynx**

  如果你只需要关注界面部分，可以只启动 Lynx 的调试服务。你需要打开 **LynxDevTool** 工具来进行调试。

  ```bash
  npm run dev:lynx
  ```

- **仅调试 Lynxtron**

  如果你只需要关注主进程逻辑，可以只启动 Lynxtron 的调试服务。
  **注意**: 此命令会等待 Lynx 的开发服务器 (`http://localhost:3000`) 启动后再执行。

  ```bash
  npm run dev:node
  ```
  Lynxtron 主进程的调试端口为 `9222`。你需要通过 Chrome 浏览器打开 `chrome://inspect` 地址，并添加对 `9222` 端口的监听来进行调试。

### 应用打包

使用以下命令为不同平台打包应用。

- **打包 macOS (x64) 架构**

  ```bash
  npm run pack:mac:x64
  ```

- **打包 macOS (arm64) 架构**

  ```bash
  npm run pack:mac:arm64
  ```

- **打包 macOS (Universal) 通用版本**

  此命令会构建一个同时支持 x64 和 arm64 架构的 macOS 通用应用。

  ```bash
  npm run pack:mac:universal
  ```

- **打包 Windows (ia32) 架构**

  ```bash
  npm run pack:win
  ```
