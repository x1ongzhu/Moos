# Moss

Palm OS 3.1–4.0 原生文字 AI 聊天客户端。使用 OpenAI 兼容的 Chat Completions API，源码在 macOS 编辑，PRC 在远程 Windows 7 构建。

## 构建与运行

```sh
./build.sh
./run.sh
```

`build.sh` 上传构建输入，通过 SSH 在 `drew@192.168.50.167` 上执行 prc-tools，并将成功构建的 PRC 下载到 `dist/Moss.prc` 和 `dist/MossTLS.prc`。日志位于 `dist/build.log`。构建失败会以非零状态退出，不覆盖上次成功的本地产物。

远程依赖为 Cygwin、原有 cygwin-b20 运行时、`C:\Handspring\prc-tools` 和 SDK 3.5。编译使用 GCC 2.95.2，代码只调用基线系统 API；兼容性仍需通过目标 ROM 验证。

```sh
MOSS_BUILD_HOST=drew@192.168.50.167 \
MOSS_REMOTE_DIR=/home/drew/workspace/Moss ./build.sh

MOSS_ROM='/Users/drew/workspace/palmos_roms/Palm-IIIc-4.0-en.rom' ./run.sh
```

默认 ROM 为本机 `/Users/drew/workspace/palmos_roms/Palm-IIIx-3.1.rom`；可通过 `MOSS_ROM` 修改。启动等价于：

```sh
'/Applications/Palm OS.app/Contents/MacOS/CloudpilotMac' \
  --rom "$MOSS_ROM" --install "$PWD/dist/MossTLS.prc" --install "$PWD/dist/Moss.prc" --reset
```

OS 4.0 已保存会话曾在 CLI 启动时立即覆盖安装触发模拟器内部断言。已验证的绕行方法是先 `MOSS_ROM=... ./run.sh --no-install` 恢复会话，待系统启动器可见后，通过模拟器工具栏“安装 PRC/PDB”选择 `dist/Moss.prc`；该方式可成功覆盖安装并保留聊天数据。

## 首次使用

1. 通过 Palm 菜单区打开 `Chat > API settings`。
2. 填写包含版本前缀的 Base URL、模型 ID 和 API Key。Base URL 后会追加 `/chat/completions`。
3. 保存后通过 `Chat > Test API connection` 发起最小请求；该操作会调用所配置的服务，可能产生费用。
4. 在聊天页输入文字并点击 Send。联网时 Send 变为 Cancel。
5. Prev / Next 切换消息，硬件上下按钮滚动长文本；Chats 打开会话列表，New 新建会话。
6. 失败问题可通过 `Chat > Retry last question` 重试。重命名与删除也在 Chat 菜单中。

API Key 默认只保存在本次运行内存中。勾选 Save key on device 后会写入系统应用偏好数据库，该存储没有加密。设置页不回显已有 Key；空白表示保留，Clear key 在 Save 后生效。英文/ASCII 输入可用，其他文字回答以 `?` 显示；原始 UTF-8 回答保留在本地数据库及后续上下文中。

模拟器粘贴文字可能逐字进入 Palm 事件队列，输入结束后再移动焦点。替换字段时可用 Palm 的 Edit > Select all，按退格清空后再输入。

## HTTPS 服务连接

HTTPS 由独立的 `dist/MossTLS.prc` 在 Palm 端执行，直接通过设备 TCP 网络连接 API，不使用 HTTP/TLS 转发代理。默认 Base URL 为 `https://api.deepseek.com/v1`；模型填写服务当前提供的 ID。

首次给当前模拟器提供独立随机种子：

```sh
./run.sh --provision
# 此后普通启动，不要反复覆盖种子：
./run.sh
```

`--provision` 只可对尚未配置种子的设备使用一次。每台设备需要不同种子；恢复旧快照后应重新生成新种子。API Key 默认只在内存中保存。

TLS 1.2 支持 RSA / AES-128-GCM / SHA-256，开启证书链、域名与有效期校验。当前采用用户批准的静态 RSA 兼容模式，不提供前向保密。只内置信任 DigiCert Global Root G2，并非通用 HTTPS 兼容库；请先校准 Palm 日期、时间与时区。移植仍在验证中，具体结果见 VALIDATION.md。

库的复用接口、内存需求和部署约束见 [tls/README.md](tls/README.md)。

## 当前范围与容量

- 原生聊天页、会话列表、重命名/删除、API 设置、连接测试、手动重试及取消。
- 两个 Palm 数据库保存会话、草稿和消息；退出或中断的请求不会在后台恢复。
- 最多 32 个会话，每会话 128 条消息；每条问题最多 1024 字节。
- 最近最多 6 条成功历史消息进入请求，历史文本预算约 3000 字节；追加当前问题。尚未提供上下文长度设置界面。
- HTTP 响应上限 16 KB，回答正文最多 4096 字节；超限情况有明确提示。
- 请求超时 180 秒；收发按小块推进。系统联网打开及 DNS/连接仍可能短暂阻塞，取消在调用返回后生效。
- 会话列表当前按创建顺序倒序，未实现按最后活动时间排序。
- 暂未实现中文字体/输入适配、服务端输出 token 设置、导入导出、红外分享和 VFS 归档。

## 检查

```sh
./tests/run.sh
# 真实 DNS/TLS 与证书正反向校验；不发送 HTTP 请求或 API Key：
./tests/tls_handshake.sh
# 仅用于端到端协议检查，返回固定文本，不调用 AI：
python3 tests/mock_api.py
# 对应 Base URL: http://127.0.0.1:8765/v1，模型 mock-model，Key 留空。
```

便携协议核心在本地用 AddressSanitizer / UndefinedBehaviorSanitizer 检查 JSON、Unicode、HTTP 长度与 chunked 分块、异常响应和容量边界。TLS 主机测试使用同一可移植 TLS 配置连接真实服务，验证证书链、主机名与有效期；需要网络，但不会发送 API Key 或聊天内容。Palm PRC 始终在远程 Windows 构建。

详细产品范围见 [FEATURE_SPEC.md](FEATURE_SPEC.md)。运行验证结果见 [VALIDATION.md](VALIDATION.md)。
