# Moss 开发约定

- 目标：Palm OS 3.1–4.0 的 68K 原生纯文本聊天客户端，仅使用 OpenAI 兼容 API。
- 源码与资源在本地编辑。Palm PRC 编译必须在远程 Windows 7 上完成。
- 当前构建机：`drew@192.168.50.167`（原 .170 地址已变更）。
- 使用 `./build.sh` 上传、远程构建并取回 `dist/Moss.prc`；日志为 `dist/build.log`。
- 远程使用现代 Cygwin SSH/Bash，旧 prc-tools 在 `C:\Handspring\prc-tools`，GCC 2.95.2，SDK 3.5。编译器的 `int` / `unsigned` 为 16 位，Unicode 码点和大数运算须使用 32 位类型。
- 默认远程工作目录为 `/home/drew/workspace/Moss`，不要改动其他项目或工具链全局配置。
- 本地模拟器统一通过 Palm OS MCP（`palmos`）操作，不直接启动 App 或调用 `./run.sh`。先用 `palm_list_instances` 检查并优先复用已有会话；无合适实例时用 `palm_open_emulator` 的 `rom_path` 选择 ROM。多个窗口时显式传入 `instance_id`。
- 安装前调用 `palm_wait_ready`；首次启动通过 `palm_screenshot` / `palm_tap` 完成 Setup。用 `palm_set_network` 启用原生网络重定向，`palm_install` 安装本地绝对路径的 `dist/Moss.prc`，通过 MCP 截图、点击、输入验证，最后 `palm_save` 保存会话。
- 本地 ROM 位于 `/Users/drew/workspace/palmos_roms`，默认 Palm IIIx 3.1；4.0 可使用 `Palm-IIIc-4.0-en.rom`。
- 便携协议核心可以在本地通过 `./tests/run.sh` 检查；这不替代远程 Palm PRC 构建。
- 当前采用中间代理方案：Palm 通过 HTTP 连接代理，代理负责上游 HTTPS 与证书校验；默认构建和启动仅需 `Moss.prc`。`tls/` 保留为历史实验，不属于当前运行依赖。
- 代理解析上游 SSE，立即将文字增量作为纯文本 HTTP chunked 正文返回；Palm 增量解析 HTTP/UTF-8 并刷新 UI，不接收 SSE，不缓存完整 HTTP 正文。
- 历史 TLS 实验的 `tls/core.c` 使用 32 位 int ABI；Palm 包装层为 16 位 int ABI，只通过显式 long/pointer 参数调用。
- 当前代理版不需要 Palm TLS 随机种子；保留已有种子数据库，不覆盖或删除。
- 本地 mock API 只能用于协议与界面验证，报告时须与真实 AI 服务调用区分。
- 不将 API Key、用户 ROM 或构建产物纳入源码版本控制。详细流程见 README.md。
