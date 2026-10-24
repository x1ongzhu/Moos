# PalmOS AI Chat 功能文档

**版本：** 0.3（产品讨论稿）  
**目标平台：** Palm OS 3.1–4.0，68K 设备  
**产品形态：** Palm PRC 纯文本客户端，通过 OpenAI 兼容 API 交互；独立 TLS 共享库直连 HTTPS

## 1. 产品概述

为 Palm OS 老设备提供轻量文字 AI 对话。客户端负责输入、显示、保存和连接管理，通过 OpenAI 兼容 API 调用用户指定的模型服务。AI 推理由服务端完成；设备通过独立的 TLS Library PRC 直连 HTTPS。目标是在小屏、低内存、间歇连接和 Graffiti 输入条件下，让用户能够发起问题、阅读短回答并继续追问。

### 产品目标

- 在 Palm OS 3.1–4.0 的经典单色/低分辨率设备上提供可读、可操作的文字聊天。
- 通过设备已有的 TCP/IP 网络能力或外接联网方式连接 OpenAI 兼容服务。
- 控制每次请求和本地保存的数据量，避免大响应耗尽设备内存。
- 网络失败、应用退出或设备自动关机后，不丢失已保存对话。

### 已确认的功能边界

- 仅通过 OpenAI 兼容 API 交互，不在 Palm 上运行本地大模型。
- 仅支持文字输入和文字回答，不包含图片、语音、视频或文件收发。
- 仅在应用前台处理用户主动发起的请求，不做后台常驻、后台重试或推送。

### 首版实现建议

- 使用非流式回答，完成接收后显示；流式输出尚未作为用户确认需求。
- 使用原生 Palm 界面显示文本。
- TLS 1.2 在独立 Library PRC 内实现，并在目标 ROM 与实际服务上验证。

## 2. 用户与典型场景

- Palm 爱好者在真实设备或模拟器上体验 AI 对话。
- 使用串口/调制解调器、支持 TCP/IP 的网络配置或设备扩展连接服务。
- 用户在断网时查看历史记录，恢复连接后重新发送失败消息。

## 3. 设备与平台约束

- Palm OS 3.1 是基础兼容线；OS 3.2 起的 Wireless Internet 专属能力不可作为全体设备前提。联网须通过运行时能力探测和可替换传输层处理。
- 网络配置依赖设备、系统组件和网络驱动。启动时检查 Net Library/接口可用性；连接失败应说明是网络未配置、无接口还是服务不可达。
- OS 4.0 的扩展/VFS 能力仅作为可选项；基础聊天记录必须存于 Palm Database（PDB），不能要求 SD 卡。
- 面向 160×160 级别屏幕设计，支持硬件按钮、五向/方向键（若设备提供）、触笔和 Graffiti。布局按运行时屏幕尺寸适配。
- 系统资源紧张：响应分块读取，限制单条消息、上下文和历史条数；所有网络操作均有超时/取消路径。
- 字符集先支持 ASCII/Latin 基础集；中文显示取决于设备 ROM/字体及输入法，不作普遍承诺。API 协议采用 UTF-8 编码，客户端需检测/转换或对不支持字符给出提示。

## 4. 功能范围与优先级

| 优先级 | 功能 | 说明 |
|---|---|---|
| P0 | 新建与继续对话 | 对话列表、创建、打开、重命名、删除 |
| P0 | 发送文字消息 | 单行/多行输入、发送、取消/重试 |
| P0 | 查看回答 | 按屏幕分页显示纯文本；保留换行，简化 Markdown 标记 |
| P0 | 本地历史 | PDB 存储用户消息、回答、时间和状态；删除会话/清理旧记录 |
| P0 | API 配置 | Base URL、API Key、模型 ID、回答长度设置；Key 默认不回显 |
| P0 | 连接诊断 | 网络组件/接口检查、DNS/连接/HTTP 错误分类、测试请求 |
| P0 | 失败恢复 | 待发送/发送中/成功/失败状态；失败后保留草稿和重试入口 |
| P1 | 上下文长度设置 | 最近 N 轮、发送前预览估算字符数、超限提醒 |
| P1 | 复制/分享 | 复制文本到剪贴板；通过 Palm Exchange Manager 或红外分享（有能力时） |
| P1 | 快捷提示词 | 可编辑的常用问题模板 |
| P1 | 会话导出/导入 | 文本导出；PDB 备份与恢复 |
| P2 | 可选 VFS 存档 | OS 4.0 且设备有扩展卡时将归档导出到卡 |
| P2 | 多服务配置 | 保存多组 OpenAI 兼容 API 配置 |

## 5. 核心用户流程

### 首次配置

1. 启动后进入欢迎页，说明 AI 服务需要网络和 OpenAI 兼容 API 配置。
2. 检查 Palm OS 版本、网络库和当前接口；缺失时仍允许进入离线历史。
3. 输入 Base URL、API Key 和模型 ID，主动发送一个最小文字请求进行连接测试；测试可能消耗服务额度。
4. 测试成功后创建首个对话；失败时显示具体排查建议。

### 发送消息

1. 打开或创建会话，输入文本。
2. 客户端检查空内容、消息长度、字符支持和上下文预算。
3. 建立网络连接并提交一次完整请求；界面显示“连接中/等待回答”。
4. 分块读取响应并写入受限缓冲区；完成后保存回答并显示。
5. 遇到错误时保存原问题，允许重试或编辑后重发。
6. 离开应用时保存草稿和消息状态、结束本地请求；下次打开时由用户手动重试。结束本地请求不保证服务端停止生成或计费。

### 恢复历史

启动进入会话列表。会话按最近活动排序；打开会话时按消息分页，避免一次把全部记录载入内存。

## 6. 屏幕与交互

### 会话列表

- 标题栏：应用名与连接状态图标。
- 列表行：会话名称、最近活动时间、未完成消息标记。
- 操作：新建、打开、菜单（重命名、删除、设置）。

### 对话页

- 顶部显示会话名及网络状态。
- 消息按“我 / AI”区分，使用紧凑纯文本；不依赖颜色区分角色。
- 默认显示最近消息，使用“上一页/下一页”或滚动按钮浏览。
- 底部固定输入区与发送按钮；设备屏幕不足时切换到独立编辑页。

### 状态与错误文案

使用简短、可行动的提示，例如“网络未连接：请先在系统 Network 设置中连接”“服务超时：消息已保存，可重试”“回答过长：已截断，可降低回答长度设置”。

## 7. OpenAI 兼容 API 约定

采用 Chat Completions 的文字交互子集。用户可配置 OpenAI 或其他提供兼容接口的服务；不要求自定义聊天后端。兼容标准以本节的最小请求和文本响应为准，各服务的扩展参数不属于必需功能。

### 核心配置

- `Base URL`：服务基础地址，允许包含路径前缀，例如 `https://example.com/v1`。
- `API Key`：按服务要求配置，默认遮蔽显示；允许不需要 Key 的服务留空。
- `Model`：可手动填写服务提供的模型 ID，不硬编码模型名称，不强制依赖模型列表接口。
- 超时、上下文预算、本地文本容量和可选的服务端输出 token 上限分别设置；字符数与 token 数不视为等价。

### 最小请求与响应

- 请求：向 Base URL 追加 `/chat/completions` 后发送 POST；例如上述地址得到 `/v1/chat/completions`。
- JSON 请求至少包含 `model`、文字 `messages` 和 `stream: false`；消息角色以 `user`、`assistant` 为基础。可选系统提示词须按目标服务与模型支持情况启用。
- 使用 `Content-Type: application/json`；配置 Key 时按服务要求使用 `Authorization: Bearer <API Key>`。
- 读取 `choices[0].message.content` 作为回答，并处理 `finish_reason`；`usage` 等附加字段可忽略。缺少有效文字回答时提示不支持的响应格式。
- 会话 ID、标题和本地消息状态由客户端维护；每次请求显式携带选定的历史上下文，不依赖服务端保存会话。
- 输出 token 限制参数按服务兼容性配置：不假设所有服务或模型都接受同一个参数；无法设置时仍执行客户端容量限制。
- 本地容量超限时保留可用文本并标识截断。客户端截断不保证减少服务端生成量或费用。
- 识别网络/超时错误、HTTP 401/403、404、429、5xx，以及无效 JSON、服务错误对象和不支持的响应；保留原问题，允许手动重试。

接口字段参考 [OpenAI Chat Completions 官方文档](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create)。第三方兼容服务的实际支持范围需单独验证。

### HTTPS 与共享库

- 直接连接 HTTPS API，不使用转发代理；TLS 在 Palm 68K 端完成。
- TLS 实现为独立 `MossTLS.prc`，提供会话创建、握手推进、读写与销毁接口，供其他应用复用。
- 校验证书链、域名及有效期；协议和信任根的覆盖范围需明确列出。
- 初始随机种子按设备独立提供，后续由库持久化更新；退出后不后台常驻。

### 已确认的开发环境

| 环节 | 环境与约定 |
|---|---|
| 源码开发与编辑 | 本地 macOS，项目目录 `/Users/drew/workspace/palmos/Moss` |
| 编译构建 | 远程 Windows 7 机器，通过 SSH 登录 `drew@192.168.50.167` |
| 产物回收 | 构建完成后，将 PRC 和构建日志取回本地；可通过 SCP/SFTP 传输 |
| 本地运行 | Palm OS 模拟器 `/Applications/Palm OS.app` |
| 模拟器可执行文件 | `/Applications/Palm OS.app/Contents/MacOS/CloudpilotMac` |

### 标准开发流程

1. 在本地修改源码、资源文件和构建配置，本地目录作为源码的主副本。
2. 将当前构建所需文件传输到远程机器的项目工作目录。
3. 通过 SSH 在 Windows 7 上执行远程工具链的构建命令，记录退出状态和构建日志。
4. 构建成功后取回本次生成的 PRC；构建失败时取回日志供本地修正。避免将旧 PRC 当成本次构建结果。
5. 在本地选择目标 Palm OS ROM，使用取回的 PRC 启动模拟器。
6. 根据运行结果继续在本地修改，再重复远程构建与本地运行流程。

SSH 登录入口：

```sh
ssh drew@192.168.50.167
```

本地模拟器启动命令（`ROM_FILE` 和 `PRC_FILE` 替换为实际路径）：

```sh
"/Applications/Palm OS.app/Contents/MacOS/CloudpilotMac" \
  --rom "ROM_FILE" \
  --install "PRC_FILE" \
  --reset
```

### 构建流程配置与验证

- 已落实：远程默认 shell 为 Bash，工作目录 `/home/drew/workspace/Moss`，通过 `scripts/remote-build.sh` 构建。
- 本地 ROM 位于 `/Users/drew/workspace/palmos_roms`，包含 3.1、3.5 与 4.0；`run.sh` 默认使用 Palm IIIx 3.1 ROM。
- 已落实：`build.sh` 通过 tar/SSH 上传源码、构建并取回 `dist/Moss.prc`，`run.sh` 安装运行。
- 模拟器网络配置及其连接 OpenAI 兼容 API的方式。

已通过更新后的 `drew@192.168.50.167` 执行远程编译并取回 PRC。远程使用 Cygwin 与 `C:\Handspring\prc-tools`（GCC 2.95.2、SDK 3.5）；本地已在 Palm OS 3.1 ROM 上验证基本问答、上下文、记录恢复、连接测试与失败重试。逐项运行结果与未覆盖项目记录在 `VALIDATION.md`。

## 13. 参考资料

- [Palm OS Programmer’s Companion：Internet 与 Messaging 应用](https://palm.wiki/development/docs/601/PalmOSCompanion2/InternetApps.html) — OS 3.2 起的无线互联网能力及设备条件。
- [Palm OS Programmer’s Companion：Network Communication](https://users.fuw.edu.pl/~michalj/palmos/NetworkCommunication.html) — Net Library 与 Internet Library 概述及兼容限制。
- [Palm OS Programmer’s API Reference：Compatibility Guide](https://www.ppl.palmwareinfo.com/index.cgi?page=Palm+OS+Programmer%27s+API+Reference%2FB) — 各版本新功能集和运行时兼容检查。
- [Palm OS 4.0 SDK headers](https://github.com/jichu4n/palm-os-sdk/tree/master/sdk-4/include) — Palm OS 4.0 SDK 头文件参考。

> 备注：联网实现和字符集覆盖需在明确设备清单后做原型验证。本文件是产品范围草案，不等同于已验证的设备兼容承诺。
