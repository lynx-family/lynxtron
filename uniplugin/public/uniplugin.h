// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef UNIPLUGIN_PUBLIC_UNIPLUGIN_H_
#define UNIPLUGIN_PUBLIC_UNIPLUGIN_H_

#include <stdint.h>

#include "uniplugin_export.h"  // NOLINT(build/include_directory)

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#include <Windows.h>
typedef HANDLE ProcessHandle;
const ProcessHandle kNullProcessHandle = NULL;
#elif defined(__APPLE__) || defined(__linux__)
#include <sys/types.h>
typedef pid_t ProcessHandle;
const ProcessHandle kNullProcessHandle = 0;
#endif

// 全局初始化
UNIPLUGIN_EXPORT void UPInitialize(bool is_broker_process, bool enable_log);

// 全局反初始化
UNIPLUGIN_EXPORT void UPUninitialize();

// 运行子进程逻辑，建立与父进程的连接。函数参数为子进程命令行参数。
UNIPLUGIN_EXPORT void UPChildProcessStart(int argc, char* argv[]);

// MessagePort 类型，用于消息通信
typedef struct _UPMessagePort* UPMessagePort;

// 创建两个连接的 MessagePort
UNIPLUGIN_EXPORT bool UPCreateMessageChannel(UPMessagePort* port1,
                                             UPMessagePort* port2);

// 销毁 MessagePort
UNIPLUGIN_EXPORT void UPMessagePortDestroy(UPMessagePort port);

// 启动 MessagePort
UNIPLUGIN_EXPORT void UPMessagePortStart(UPMessagePort port);

// 发送消息
UNIPLUGIN_EXPORT void UPMessagePortPostMessage(UPMessagePort port,
                                               const char* message,
                                               uint32_t length,
                                               UPMessagePort* ports,
                                               uint32_t port_count);

// MessagePort 消息回调
typedef void (*UPMessagePortMessageCallback)(const char* message,
                                             uint32_t length,
                                             UPMessagePort* ports,
                                             uint32_t port_count);

typedef void (*UPConnectionCloseCallback)(int32_t exit_code);

// 设置 MessagePort 消息回调
void UPMessagePortSetMessageCallback(UPMessagePort port,
                                     UPMessagePortMessageCallback callback);

// void UPMessagePortSetMessageErrorCallback(
//     UPMessagePort port,
//     UPMessagePortMessageCallback callback);

typedef struct _UPProcessProcess* UPPluginProcess;

typedef void (*UPChildProcessCloseCallback)(UPPluginProcess process,
                                            int32_t exit_code);

typedef struct {
  const char* command_lines;
  const char* path;
} UPCommandLineArgs;

UNIPLUGIN_EXPORT UPPluginProcess UPPluginProcessCreate(UPCommandLineArgs* args);

// 创建进程
UNIPLUGIN_EXPORT bool UPPluginProcessFork(UPPluginProcess process);
// 销毁进程对象
UNIPLUGIN_EXPORT void UPPluginProcessDestroy(UPPluginProcess process);

UNIPLUGIN_EXPORT void UPPluginProcessCloseConnectorPort(
    UPPluginProcess process);

// 获取 Native 句柄
UNIPLUGIN_EXPORT ProcessHandle
UPPluginProcessGetNativeHandle(UPPluginProcess process);

// 发送消息
UNIPLUGIN_EXPORT void UPPluginProcessPostMessage(UPPluginProcess process,
                                                 const char* message,
                                                 uint32_t length,
                                                 UPMessagePort* ports,
                                                 uint32_t port_count);

// message 回调
UNIPLUGIN_EXPORT void UPPluginProcessSetMessageCallback(
    UPPluginProcess process,
    UPMessagePortMessageCallback callback,
    UPChildProcessCloseCallback close_callback);

// spawn 回调
typedef void (*UPPluginProcessSpawnCallback)();
void UPPluginProcessSetSpawnCallback(UPPluginProcess obj,
                                     UPPluginProcessSpawnCallback callback);

// exit 回调
typedef void (*UPPluginProcessExitCallback)(int32_t /*code*/);
void UPPluginProcessSetExitCallback(UPPluginProcess obj,
                                    UPPluginProcessExitCallback callback);

// 子进程发送消息
UNIPLUGIN_EXPORT void UPChildProcessPostMessage(const char* message,
                                                uint32_t length,
                                                UPMessagePort* ports,
                                                uint32_t port_count);

UNIPLUGIN_EXPORT void UPChildProcessSetMessageCallback(
    UPMessagePortMessageCallback callback,
    UPConnectionCloseCallback close_callback);

#ifdef __cplusplus
}
#endif

#endif  // UNIPLUGIN_PUBLIC_UNIPLUGIN_H_
