# Moss

Palm OS 3.1–4.0 的 68K 原生纯文本 AI 聊天客户端，使用 OpenAI 兼容 Chat Completions API。

当前采用中间代理：Palm → HTTP → 代理 → HTTPS → AI 服务。TLS、证书校验及上游 API Key 可全部留在代理主机，Palm 不再需要 TLS 共享库或随机种子。此前的 Palm TLS 实验源码保留在 `tls/` 和 `vendor/`，不属于默认构建或运行依赖。

## 启动代理

Python 3 标准库即可运行，无需安装依赖：

```sh
export MOSS_UPSTREAM='https://api.deepseek.com/v1'
export MOSS_UPSTREAM_KEY='<你的上游 API Key>'
python3 tools/relay.py
```

默认仅监听本机 `127.0.0.1:8765`。通过 Palm OS MCP 启用模拟器原生网络重定向后，在 Palm 的 `Chat > API settings` 中填写：

- Relay URL：`http://127.0.0.1:8765/v1`
- Model ID：上游实际提供的模型 ID，例如本项目已验证的 `deepseek-flash`
- Relay key：本机代理未设置口令时留空；已有 Key 需点 Clear key 后 Save 才会清除

如需独立的代理口令，启动前设置 `MOSS_RELAY_KEY`，Palm 填写该口令。此模式必须同时设置 `MOSS_UPSTREAM_KEY`；上游服务 Key 不会被代理口令替代。未设置上游 Key 时支持透传 Palm 提供的 Authorization，优先使用主机保管上游 Key 的方式。

实体掌机应填写代理主机的可达局域网地址，并通过 `--bind <代理主机局域网IP> --port 8765` 监听该地址；`127.0.0.1` 仅适用于本机模拟器。局域网部署可配合 `MOSS_RELAY_KEY` 限制调用。Palm 与代理之间为明文 HTTP，代理可以读取聊天内容；代理到上游的 HTTPS 使用主机证书库校验域名和证书。不要把 API Key 写进源码或提交到 Git。

`--upstream` 可替代 `MOSS_UPSTREAM`。代理只转发固定的 `/v1/chat/completions`，保留上游 Base URL 路径前缀；不跟随重定向，不自动重放失败的 POST，不记录口令或消息正文。上游 HTTP 地址仅供本地协议测试使用。

## 构建与运行

源码在本机编辑，Palm PRC 必须在指定 Windows 7 构建机编译：

```sh
./build.sh
```

`build.sh` 通过 SSH 上传 `src/`、`resources/` 及构建脚本至 `drew@192.168.50.167:/home/drew/workspace/Moss`，运行 GCC 2.95.2 / SDK 3.5，并取回 `dist/Moss.prc`。日志为 `dist/build.log`。默认不编译、打包或安装 `MossTLS.prc`，也不生成随机种子。

如需覆盖构建机配置：

```sh
MOSS_BUILD_HOST=drew@192.168.50.167 MOSS_REMOTE_DIR=/home/drew/workspace/Moss ./build.sh
```

本地运行统一通过 Palm OS MCP（`palmos`）操作，不直接调用模拟器 App 或 `run.sh`。以下为 MCP 工具调用参数，路径必须使用本机绝对路径：

1. 调用 `palm_list_instances({})` 检查已有实例；优先复用目标 ROM 的已有会话。多个窗口时，后续调用必须传入所选 `instance_id`。
2. 没有可复用实例时，调用 `palm_open_emulator({"rom_path":"/Users/drew/workspace/palmos_roms/Palm-IIIx-3.1.rom"})`，使用返回的 `instance_id`。OS 4.0 验证改用 `Palm-IIIc-4.0-en.rom` 的完整路径。
3. 调用 `palm_wait_ready({"instance_id":实例ID,"timeout_seconds":30})`。首次启动若出现 Setup，使用 `palm_screenshot` 查看画面、`palm_tap` 完成欢迎页、触控校准和设置，点击 Done 进入应用主界面。
4. 调用 `palm_set_network({"instance_id":实例ID,"enabled":true})` 启用本机原生网络重定向。
5. 系统就绪后调用 `palm_install({"instance_id":实例ID,"path":"/Users/drew/workspace/palmos/Moss/dist/Moss.prc"})`，成功后默认启动 Moss。
6. 使用 `palm_screenshot`、`palm_tap`、`palm_type_text` 和 `palm_press_key` 验证界面与交互；操作完成后调用 `palm_save({"instance_id":实例ID})` 保存会话。截图和点击坐标以 MCP 返回的屏幕尺寸为准。

通过 `palm_status` 可检查 ROM、运行状态和安装就绪状态。升级时复用已有会话并等待就绪后覆盖安装，以保留 API 设置、草稿和聊天记录；不要为升级清空会话。旧版保存的 HTTPS 地址需手动改为 HTTP 代理地址。现有 TLS 库和种子数据库无需删除或覆盖。

## 使用

`Chat > API settings` 设置代理地址、模型及口令；`Test API connection` 会实际调用模型。聊天页点击 Send 提问，联网期间可 Cancel；Prev / Next 切换消息，右侧滚动条或硬件上下按钮翻页，Chats 管理会话。失败后可通过 `Retry last question` 手动重试。

Key 默认保存在本次运行内存；勾选 Save key on device 后存入未加密的 Palm 偏好数据库。设置页空白表示保留现有 Key，Clear key 在 Save 后生效。当前仅支持英文/ASCII 输入，Unicode 弯引号、破折号、省略号等常见标点转成 ASCII 显示，其余不支持的字符显示为 `?`；原始 UTF-8 留在消息数据库和上下文中。

## 连接与容量

- 成功且分帧完整的响应后，Palm 保留 HTTP 连接 60 秒；同来源连续提问显示 `Ready (reused)`。代理为每条 Palm 连接保留独立的上游连接，并在上游空闲超过 50 秒时重新建立。
- 退出、取消、设置保存、空闲到期或失败时释放 Palm 连接。复用连接失效，仅在尚未提交任何请求字节时自动重连；发送后错误需手动重试。
- 最多 32 个会话，每会话 128 条消息；问题最多 1024 字节，最近最多 6 条成功历史消息（约 3000 字节）进入上下文。
- 回答正文最多 4096 字节，超出后继续排空响应并显示截断提示。Palm 不缓存整段 HTTP/SSE/JSON；协议接收状态在本机 ABI 下为 5432 字节，另有 512 字节网络接收块和 UI 显示缓冲。
- 请求超时 180 秒；代理单次上游网络操作超时 80 秒。默认发送 `stream: true` 和 `Accept: text/plain`，通过纯文本 HTTP 正文边接收边显示。只有完整结束的回答会被保存。
- 暂未实现中文字体/输入、可配置 token 上限、导入导出及按最近活动排序。

## 流式显示

代理解析上游 SSE，只提取第一项回答的 `delta.content`，立即转换为 UTF-8 纯文本并 flush 到 Palm；心跳、角色、usage、JSON 字段及 `[DONE]` 均不传给 Palm。下游使用 `Content-Type: text/plain; charset=utf-8` 和 HTTP/1.1 chunked 分帧，不等上游结束才返回响应头或正文。

Palm 增量解析 HTTP 分帧及跨分包的 UTF-8，收到首段正文就显示，后续刷新限制为约每秒 10 次。只保留有界回答前缀，无需 SSE 事件或 JSON 缓冲。服务忽略 stream 参数而返回普通 JSON 时，由代理提取文字后返回带 Content-Length 的纯文本，此回退无法提前显示。

接收期间显示 `AI [Streaming]` 和 Receiving，并在每次新增内容时自动滚动到底部，回答完成后也停留在末尾；完成后可自由翻阅。代理确认上游 `[DONE]` 及完整 HTTP 结束后才发送下游终止 chunk；Palm 确认 HTTP 正文完整结束后保存一次回答，恢复常规消息页面。取消或中断的片段不保存，也不进入后续上下文；失败只允许手动重试，避免重复调用。

上游出错或缺失 `[DONE]` 时，代理关闭连接且不发送终止 chunk；Palm 将其识别为中断，不保存片段。代理的上游流式正文上限为 1 MiB，单个 SSE 行/事件上限为 65536 字符，Palm 含分帧的总接收量上限为 2 MiB。正常结束后保留连接复用；取消时 Palm 关闭连接，代理在检测到断开后的下一次写入关闭上游连接。

## 检查

```sh
./tests/run.sh
# 本地 mock 返回固定文本，不调用 AI；与真实服务结果分别记录：
python3 tests/mock_api.py --port 8766 --delay 1.5
# 另一个终端启动测试代理（先清除继承的真实服务口令）：
env -u MOSS_UPSTREAM_KEY -u MOSS_RELAY_KEY MOSS_UPSTREAM=http://127.0.0.1:8766/v1 python3 tools/relay.py --port 8767
# Palm 临时使用 http://127.0.0.1:8767/v1、mock-model；验证后恢复真实代理配置。
```

默认检查包括协议和实际 Palm 网络状态机的 ASan/UBSan 测试、Palm 增量 HTTP/UTF-8 分包与中断测试、代理 SSE 转纯文本测试、代理固定路由/鉴权/双端连接复用/响应容量/禁止重放，以及本地不受信任 HTTPS 证书拒绝测试。代理测试用可控的上游等待点确认首段在回答完成前到达。测试会绑定回环端口。历史 RSA/TLS 测试仍可单独运行，但不属于当前代理版依赖。

产品范围见 [FEATURE_SPEC.md](FEATURE_SPEC.md)，具体构建与真实调用结果见 [VALIDATION.md](VALIDATION.md)。
