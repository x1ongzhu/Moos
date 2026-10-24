# Moss 开发约定

- 目标：Palm OS 3.1–4.0 的 68K 原生纯文本聊天客户端，仅使用 OpenAI 兼容 API。
- 源码与资源在本地编辑。Palm PRC 编译必须在远程 Windows 7 上完成。
- 当前构建机：`drew@192.168.50.167`（原 .170 地址已变更）。
- 使用 `./build.sh` 上传、远程构建并取回 `dist/Moss.prc`；日志为 `dist/build.log`。
- 远程使用现代 Cygwin SSH/Bash，旧 prc-tools 在 `C:\Handspring\prc-tools`，GCC 2.95.2，SDK 3.5。编译器的 `int` / `unsigned` 为 16 位，Unicode 码点和大数运算须使用 32 位类型。
- 默认远程工作目录为 `/home/drew/workspace/Moss`，不要改动其他项目或工具链全局配置。
- 使用 `./run.sh` 启动本地 `/Applications/Palm OS.app/Contents/MacOS/CloudpilotMac`，通过 `MOSS_ROM` 切换 ROM。
- 本地 ROM 位于 `/Users/drew/workspace/palmos_roms`，默认 Palm IIIx 3.1；4.0 可使用 `Palm-IIIc-4.0-en.rom`。
- 便携协议核心可以在本地通过 `./tests/run.sh` 检查；这不替代远程 Palm PRC 构建。
- HTTPS 必须由 Palm 端独立 `MossTLS.prc` 执行 TLS；禁止通过代理终止 TLS。保持证书链、域名及有效期校验。
- `tls/core.c` 使用 32 位 int ABI；Palm 包装层为 16 位 int ABI，只通过显式 long/pointer 参数调用。
- 每台设备单独提供系统安全随机种子；禁止复用或反复覆盖种子数据库。
- 本地 mock API 只能用于协议与界面验证，报告时须与真实 AI 服务调用区分。
- 不将 API Key、用户 ROM 或构建产物纳入源码版本控制。详细流程见 README.md。
