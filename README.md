# Event Reporter - 事件上报系统

## 项目简介

Event Reporter 是一个简化版的事件上报系统，用于监控和记录来自多个应用程序的事件。它通过命名管道（Named Pipe）接收事件数据，并支持多种事件输出方式，包括控制台显示和文件存储。

## 功能特点

- **多应用程序支持**：可以同时接收来自多个应用程序的事件
- **多种输出方式**：支持控制台显示和文件存储
- **分类日志**：按应用程序来源分类存储事件日志
- **实时处理**：使用多线程和事件队列实现高效处理
- **简单集成**：提供标准的事件上报接口

## 系统架构

### 核心组件

1. **Event 结构体**：存储事件的基本信息，包括事件名称、数据、来源和时间戳
2. **IEventSender 接口**：定义事件发送器的抽象接口
   - **ConsoleSender**：将事件输出到控制台
   - **FileSender**：将事件写入单一日志文件
   - **CategorizedFileSender**：按来源分类写入不同日志文件
3. **EventSystem 类**：核心事件处理系统，管理事件队列和发送器
4. **PipeServer 类**：处理命名管道通信，接收来自其他应用程序的事件
5. **控制台UI**：显示系统状态和事件信息

### 工作流程

1. 系统启动并初始化事件处理系统和管道服务器
2. 等待应用程序连接到命名管道
3. 接收应用程序发送的事件数据
4. 解析事件数据并添加到事件队列
5. 处理事件队列中的事件，通过注册的发送器输出
6. 按应用程序来源分类存储事件日志

## 技术实现

- **语言**：C++
- **平台**：Windows
- **通信**：命名管道（Named Pipe）
- **线程**：C++11 线程库
- **同步**：互斥锁和条件变量
- **文件操作**：标准文件流

## 使用方法

### 启动事件上报系统

1. 编译并运行 `event_reporter.exe`
2. 系统会自动启动并等待应用程序连接

### 应用程序集成

要将应用程序与事件上报系统集成，需要：

1. 连接到命名管道 `\\.\pipe\GameEventPipe`
2. 发送握手消息，格式为 `PROGRAM:应用程序名称`
3. 发送事件数据，格式为 JSON 字符串：
   ```json
   {"event":"事件名称","data":"事件数据"}
   ```

### 示例代码

```cpp
// 连接到事件上报系统
HANDLE hPipe = CreateFile(
    "\\.\\pipe\\GameEventPipe",
    GENERIC_READ | GENERIC_WRITE,
    0,
    NULL,
    OPEN_EXISTING,
    0,
    NULL
);

// 发送握手消息
string handshake = "PROGRAM:MathGame";
WriteFile(hPipe, handshake.c_str(), (DWORD)handshake.length(), &bytesWritten, NULL);

// 发送事件
string eventData = "{\"event\":\"game_start\",\"data\":\"New game started\"}";
WriteFile(hPipe, eventData.c_str(), (DWORD)eventData.length(), &bytesWritten, NULL);
```

## 日志管理

- 事件日志存储在 `logs/` 目录中
- 每个应用程序有独立的日志文件，命名格式为 `应用程序名称_events.log`
- 日志文件包含事件的时间戳、事件名称和数据

## 配置与扩展

### 配置选项

- **日志目录**：默认存储在 `logs/` 目录，可在 `CategorizedFileSender` 构造函数中修改
- **命名管道名称**：默认使用 `GameEventPipe`，可在 `PipeServer` 构造函数中修改

### 扩展方式

1. **添加新的发送器**：实现 `IEventSender` 接口并注册到 `EventSystem`
2. **修改事件处理逻辑**：修改 `EventSystem::processEvents` 方法
3. **添加新的通信方式**：实现新的服务器类，类似于 `PipeServer`

## 运行示例

系统启动后，会显示欢迎信息和当前状态。当有应用程序连接时，会显示连接信息，并开始接收和处理事件。

### 示例输出

```
========================================
       多程序事件上报监控系统
     Multi-Program Event Monitor
========================================

[状态] 系统初始化完成
[状态] 等待程序连接...
[状态] 管道地址: \\.\pipe\GameEventPipe

[说明] 支持的程序类型:
  1. MathGame (数学游戏小程序)
  2. OtherApp (其他应用程序)
  3. 任何通过管道连接的程序

[注意] 每次只能处理一个程序
[注意] 关闭当前程序后,可以处理下一个程序

----------------------------------------

[切换] 当前连接程序: MathGame
----------------------------------------
[控制台] [Mon Apr 22 14:30:00 2026] [来源:MathGame] 事件: game_start | 数据: New game started
----------------------------------------
[控制台] [Mon Apr 22 14:30:10 2026] [来源:MathGame] 事件: game_end | 数据: Game ended with score 100
----------------------------------------
```

## 编译说明

- 使用 Visual Studio 2022 或更高版本编译
- 支持 x64 平台
- 依赖 Windows SDK

## 项目结构

```
event_reporter/
├── event_reporter.sln                # 解决方案文件
├── event_reporter.vcxproj            # 项目文件
├── main.cpp                          # 主源代码文件
├── logs/                             # 日志存储目录
│   └── MathGame_events.log           # 示例日志文件
└── x64/
    └── Debug/                        # 编译输出目录
        └── event_reporter.exe        # 可执行文件
```

## 注意事项

- 系统仅支持 Windows 平台

- 每次只能处理一个应用程序连接

- 应用程序关闭后，系统会自动等待下一个连接

  

