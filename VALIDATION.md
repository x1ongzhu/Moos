# 验证记录

日期：2026-10-25（Asia/Shanghai）

## 当前模拟器操作方式

本地启动、安装与界面操作统一使用 Palm OS MCP，步骤见 [README.md](README.md#构建与运行)。已验证实例查询、加载 OS 3.1 ROM、等待就绪、截图、点击完成 Setup 并保存会话；本次 MCP 检查未安装 Moss，也未调用 AI 服务。下文历史记录中的 `run.sh`、CLI 和工具栏操作仅描述当时的验证方式。

## 当前补充：长回答滚动及标点兼容

- 回答区增加可拖动滚动条，硬件键按可见行数翻页并同步位置；按最新要求，流式每次新增内容都强制滚动到底部，回答完成并保存后也停留在末尾。
- 在 Palm IIIx / OS 3.1 上用已有长回答验证：可拖至故事最后一句，硬件向上键能返回前页，原有对话保留。本项未新增 AI 调用。
- 显示转换将 Unicode 弯引号、破折号、省略号、不换行空格等转换成 ASCII 标点；数据库及请求上下文仍使用原始 UTF-8。仍未提供完整 Unicode 字体。
- 安装后在原有故事回答中确认 `He's out back. Go talk to him.` 的单引号及外围双引号正常显示，无需重新调用模型。
- 全套本机检查通过，包含新增标点、ASCII 原样保留、中文/emoji 回退和不完整 UTF-8 的 ASan/UBSan 检查，以及 20 项代理测试。
- Windows 7 / GCC 2.95.2 / SDK 3.5 构建并安装：`dist/Moss.prc` 21350 字节，SHA-256 `6ab9b647661df744a6448b9bd58dda98bdc3b1d91dfcca2b0dc394412ffce6b3`。该最终滚动策略通过远程编译；前述全套本机测试针对标点修复运行，协议和网络逻辑未再修改。

## 当前：纯文本 HTTP 增量流版本

- Windows 7 / GCC 2.95.2 / SDK 3.5 远程构建成功，`dist/Moss.prc` 20600 字节，SHA-256 `bbcff2e1bac2c6058d4550e90260ab1749c97b138a8138dd5f56692ba3ff3afe`。已安装到 Palm IIIx / OS 3.1 模拟器。
- Palm 发送 `stream: true` 和 `Accept: text/plain`。代理解析上游 SSE，只将 UTF-8 文字增量作为 HTTP chunked 正文立即返回；下游不包含 SSE/JSON 事件。代理确认上游结束标记及 HTTP 完整结束后才发送终止 chunk。
- Palm 收到正文即更新 `AI [Streaming]`，后续刷新约限为每秒 10 次；UTF-8 可跨 TCP 包和 HTTP chunk。回答前缀上限 4096 字节，超出继续排空，协议状态在本机 ABI 下为 5432 字节（不是整个应用堆占用或 Palm ABI 实测值），无需完整 HTTP/SSE/JSON 缓冲。
- 本地协议、增量正文、实际 Palm 网络状态机通过 ASan/UBSan，覆盖分包、UTF-8、超长截断、提前显示、复用、取消及异常不保存、不自动重放。
- 代理 20 项测试通过，包含受控等待点确认首段在上游结束前到达、纯文本输出、SSE 分片/Unicode、JSON 回退、缺失 DONE/错误事件/中途断开时下游 HTTP 不完整，以及既有鉴权、路由、复用和证书拒绝检查。这些测试只使用本机临时服务。
- 本地 mock 界面验收：延迟 SSE 上游 `8766` 经代理 `8767` 转成纯文本；发送 `Plain HTTP streaming demo` 后，Palm 在 `Receiving` / Cancel 状态先显示 `Mock`，结束后保存 `2/2 AI`、完整 mock 文本及 `Context messages: 1`。这不是 AI 服务调用。
- 已恢复 `http://127.0.0.1:8765/v1` / `deepseek-flash`，并重启真实代理加载新协议。新空会话连续发送两轮已授权的 `Reply with exactly OK.`，真实 DeepSeek 分别返回 `2/2 AI` / `OK`、`4/4 AI` / `OK`，第二轮显示 `Ready (reused)`。服务 Key 仅在代理进程环境中，未进入源码。
- Python 语法、Git whitespace、源码凭据模式扫描通过。未做实体设备、OS 4.0 或精确带宽/延迟基准，不报告提速比例。

## 历史：HTTP 中间代理非流式版本

用户选择放弃 Palm 端 TLS，改用中间代理。当前 `./build.sh` 仅远程编译 `Moss.prc`，`./run.sh` 仅安装该应用；`tls/`、BearSSL 与历史测试保留源码，不再构成当前运行依赖。旧库和种子数据库未删除、未覆盖。

- 构建：Windows 7 / GCC 2.95.2 / SDK 3.5，`dist/Moss.prc` 20396 字节，SHA-256 `fb562787e082a6701fa238a6cfc3be17db31a3196524ade60365a4fbf1ec67c9`。
- Palm IIIx / OS 3.1 安装启动成功，已有用户会话保留。设置改为 `http://127.0.0.1:8765/v1`、模型 `deepseek-flash`，清除 Palm 上原服务 Key；实际服务 Key 仅在本机代理进程环境内，未写入源码。
- 新空会话两轮真实服务调用成功：分别发送 `Reply with exactly OK.`，显示 `2/2 AI` / `OK` 和 `4/4 AI` / `OK`，第二轮显示 `Ready (reused)`。代理日志记录两次 HTTP 200，无聊天正文或凭据。此结果是经代理调用真实 DeepSeek，区别于本地 mock，也不再声称 Palm 独立 TLS。
- 两轮真实网络检查使用与最终版相同的网络代码；最终仅把 About 页说明由 native TLS 改为 HTTP relay，并重新远程编译安装。
- 本地协议及实际 Palm 网络状态机通过 ASan/UBSan；HTTP 复用、空闲超时、地址变化、发送后不重放、取消释放与拒绝直接 HTTPS 的分支通过。
- 代理 12 项集成测试通过：转发及路径前缀、独立鉴权、固定路由、非流式限制、容量上限、客户端及上游 TCP 复用、失败 POST 不重放、重定向拒绝、HTTP 错误状态保留、压缩响应拒绝、重复 Content-Length 拒绝、拒绝本地不受信任 HTTPS 证书且服务端收到零个 HTTP 请求。
- 代理上游通过 Python `ssl.create_default_context()` 校验证书及主机名；不受 Palm 历史 TLS 套件限制。Palm 到代理为明文 HTTP。
- Shell 和 Python 语法检查、Git whitespace 检查通过。代理测试只使用本机临时 HTTP/HTTPS 服务，不调用 AI。
- 尚未验收实体设备、局域网代理部署或本版 OS 4.0 收发。未进行精确延迟基准测试，不报告提速比例。

以下为历史 Palm 原生 TLS 版本及早期 mock 的记录，不代表当前部署架构。

## 历史：原生 TLS 连接复用版本

- Windows 7 / GCC 2.95.2 构建成功：`dist/Moss.prc`，22270 字节，SHA-256 `626d85299a430b4e108fd31095bc4949c686b3cfbab25c08d50f44e936d39292`。实验 TLS 库未改动，哈希与下节相同。
- 已在 Palm IIIx / OS 3.1 覆盖安装最终构建，启动正常；此前四条测试记录及草稿保留。
- 成功且明确分帧的响应后保留同来源 HTTP/TLS 连接 60 秒；退出、取消、设置保存、超时、失败时释放。保留会话导致低内存时退回新连接。
- `./tests/run.sh` 通过，含协议 ASan/UBSan、网络状态机 ASan/UBSan 和 36 组双路径 RSA 参考向量。
- 协议新增持久连接检查：HTTP/1.0 显式许可、HTTP/1.1 默认许可、大小写和逗号分隔的 Connection 头、重复头中 close 优先、chunked 与 trailer、截断与多余字节、依赖 EOF 的响应不复用。
- 网络测试运行实际 `src/network.c`，使用确定性的模拟 Palm/HTTP/TLS 传输，确认两次回复只连接/建会话一次、失效后发送前重连、发送后失败不自动重放、空闲释放、取消释放、域名/协议/端口变化和低内存回退。它不构成真实 TLS 或 AI 请求验收。
- 低内存回退修订前的 22186 字节构建已在 Palm 上取得两轮真实 DeepSeek `OK`；第二轮因操作延迟超过空闲窗口，实际重新握手，不能算复用成功。
- 最终 60 秒版本已完成 Palm IIIx / OS 3.1 真实连续复用验收：新建空会话，向 `https://api.deepseek.com/v1` / `deepseek-flash` 连续发送两次 `Reply with exactly OK.`。首轮显示 `2/2 AI` / `OK`，次轮显示 `4/4 AI` / `OK` 和 `Ready (dev reuse)`，确认沿用同一 TLS 会话，未重新握手。
- 两轮发送起点间隔约 35 秒；第二轮发送后约 15 秒观察时已完成。这些是 UI 观察时间，不是精确延迟或提速比例。原 15 秒版本在发送间隔约 31 秒时超出保留窗口而重新连接，因此将窗口调整为 60 秒。
- UI 自动审批最初因无法核实目标与历史拒绝提交；用户随后明确授权指定的两轮测试内容，验收已完成。未向原有用户会话插入测试消息。
- OS 4.0、实体设备及默认验证库的真实连接复用尚未验收。

## 此前跳过验签实验版验收

用户明确选择跳过证书校验。`MOSS_TLS_INSECURE=1 ./build.sh` 已在 Windows 7 构建，并通过 `MOSS_TLS_PRC="$PWD/dist/MossTLS-insecure.prc" ./run.sh` 安装到 Palm IIIx / OS 3.1。

新建空会话，核对目标为 `https://api.deepseek.com/v1`、模型 `deepseek-flash` 后发送 `Reply with exactly OK.`，实际显示 `2/2 AI`、`OK` 及 `Ready (TLS dev)`。发送后约 15 秒观察时已完成；此数值是观察上界，不是精确响应耗时。原有用户会话未发送测试消息。

- 此前应用：`dist/Moss.prc`，20846 字节，SHA-256 `d1b69f49989c58f1a2d241f231183fc8e3de19b94124d035c3ae4b8df0263c27`。
- 实验库：`dist/MossTLS-insecure.prc`，49114 字节，SHA-256 `7a58dea779a616811ca9462facd0417394ec940f0ed9f1577f43d09e1e47e69e`。
- 保留的默认验证库：`dist/MossTLS.prc`，49182 字节，SHA-256 `bb53ebc10cae321fd6dfa06c838a9346cb129da3def7e2eb44778666c7c1a3e4`。
- 实验版保留 Palm 端 TLS 加密，跳过证书链、域名及有效期验证；没有 TLS 终止代理。状态位明确标记实验模式，不报告 X.509 验证成功。
- 主机实验测试确认错误域名/时间仍能完成加密握手；默认版本负向检查仍拒绝错误证书。
- 默认库新增 Mac 预验证指纹路径，已通过主机首次命中、指纹变化回退及负向检查，尚未单独完成该路径的 Palm 实机验收。

下列构建标识和两轮真实调用记录属于此前完整验签版本；不代表当前实验版完成了这些相同检查。

## 此前完整验签构建产物

- 文件：`dist/Moss.prc`
- 长度：20790 字节
- SHA-256：`92a74b1cde3f18ff0c5476a917d59c97d5bb951473264e744d4a2768b133d6d9`
- TLS 库：`dist/MossTLS.prc`，49038 字节，SHA-256：`3594fa578a1455ae99d738269b51f32854df916d4016d4f85c3cb4612e457611`。
- 当前引擎镜像 43368 字节，520 个重定位项。此前 mock 界面检查与下列真实 DeepSeek 调用分别记录。
- Palm 数据库名称：Moss，类型 `appl`，Creator ID `Moss`。
- 编译位置：Windows 7 / Cygwin，`drew@192.168.50.167`。
- 工具链：远程 prc-tools GCC 2.95.2、Palm-RC、SDK 3.5。
- 编译启用 `-Wall -Werror`，仅屏蔽旧 SDK 的未知 pragma 和 Creator ID 多字符字面量警告。
- PRC 包含聊天、设置、会话列表、重命名表单及菜单、提示资源。

## 已完成检查

| 项目 | 结果 |
|---|---|
| 远程上传、编译、下载 | 成功；日志 `dist/build.log` |
| Palm IIIx / OS 3.1 ROM 安装启动 | 成功，原生聊天与设置页面正常 |
| Palm OS 3.1 首次问答 | 成功，本地 mock 接口返回分块 HTTP 响应 |
| Palm OS 3.1 多轮对话 | 成功，第二轮请求包含 3 条消息（第一问、第一答、第二问） |
| Palm OS 3.1 本地记录恢复 | 成功，退回系统启动器后重新打开 Moss，4 条消息保持可读 |
| Palm OS 3.1 会话列表 | 成功，列出已保存的 Hello Palm 会话 |
| Palm OS 3.1 独立连接测试 | 成功；不向会话添加测试消息 |
| Palm OS 3.1 401 错误提示 | 成功，提示检查 API Key，问题保留为可重试 |
| Palm OS 3.1 失败重试 | 成功，重发相同问题，记录总数保持 5 条，没有重复插入 |
| Palm IIIc / OS 4.0 ROM 安装启动 | 成功，彩色 ROM 上原生页面正常 |
| 最终 PRC 在 OS 4.0 覆盖安装 | 成功；先恢复会话，再从工具栏安装，历史记录保留 |
| Palm OS 3.1 新会话草稿隔离 | 成功；新会话输入为空，切回原会话仍保留 draft |
| Palm OS 4.0 首次问答 | 成功，本地 mock 接口返回 `Mock reply: Hello OS4` |
| 协议核心检查 | 通过，AddressSanitizer / UndefinedBehaviorSanitizer 未报告错误 |
| 真实服务 TLS 证书校验 | `api.deepseek.com:443` 握手成功；错误主机名返回 X509 56，过期时间返回 X509 54 |
| 配套代理检查 | 5 项通过：转发、独立鉴权、固定路径、非流式限制、容量限制 |
| Shell 脚本语法检查 | 通过 |
| RSA 独立模幂参考测试 | 36 组通过；两个算术入口分别与 Python 模幂比较，ASan/UBSan 无报告 |
| Palm OS 3.1 真实证书校验 | 成功；完整验证通过后预验证重连，才发送 HTTP |
| 断开连接处理 | Palm 上前一构建在对端关闭后及时报 code 32005；不再无限重试零字节发送 |
| Palm OS 3.1 真实 DeepSeek 首轮调用 | 成功；`https://api.deepseek.com/v1` / `deepseek-flash`，问题 `Reply with exactly OK.`，屏幕显示 `4/4 AI` 和 `OK` |
| Palm OS 3.1 真实多轮上下文 | 成功；第二问 `What was your previous reply? Repeat it exactly.`，屏幕显示 `6/6 AI` 和 `OK` |
| Palm OS 3.1 真实记录恢复 | 成功；返回系统启动器，重新打开 Moss 后仍显示 6 条记录及最新 `OK` 回复 |
| 真实服务 401 处理 | 成功；未保存的内存 Key 在重启后丢失时显示 HTTP 401；重新输入授权 Key 后真实调用成功 |
| 应用数据放行检查 | 通过；握手期间和证书拒绝后不提供应用写入缓冲区；本地不受信任服务端完成 TLS 握手后收到 0 个应用字节，客户端报 X509 62 |
| 证书缓存正反向检查 | 通过；相同链命中、指纹改变后重新验签，缓存存在时错误域名报 X509 56、过期时间报 X509 54 |

协议检查包含 Base URL 解析、JSON 转义、Content-Length、按字节截断的 HTTP 接收、chunked 解码、HTTP 编码拒绝、Unicode/代理对、无效 JSON 与文本容量截断。最新公开验签路径的 TLS 主机检查通过真实证书链、错误域名与过期时间三项验证，不发送 HTTP 请求或 API Key。测试源码在 `tests/`。

握手诊断：早期版本曾停在密文发送 362 字节、接收 4251 字节、引擎状态 6、网络错误 0。模拟器网络后端可能把 broken pipe 返回为零字节/无错误。主机独立实验在收到服务器握手数据后等待 1、3、5、10 秒再继续握手均成功，等待 20 秒时被关闭。Palm 诊断版本记录过密码运算 38 秒、总耗时 39 秒。将验签移到应用放行前后，握手完成，但服务器又在应用层等待期间关闭。最终通过完整证书预验证、内存验证缓存和发送 HTTP 前重连解决；两轮真实回复和记录恢复均在最终 PRC 上观察到。

## 验证边界

- 表中明确标注 mock 的问答使用 `tests/mock_api.py`；真实 DeepSeek 两轮调用使用用户授权 Key，TLS、证书校验和加解密都由 Palm 端 `MossTLS.prc` 完成，没有 TLS 终止代理。
- Palm 真实回复经过 HTTP 成功状态和 JSON 文本解析后显示；此前主机版同一核心的请求也曾取得 HTTP 200 / `OK`。主机版结果与 Palm 端结果分别记录。
- 此前完整验签版本及当前实验版已在 Palm IIIx / OS 3.1 验证；OS 4.0 的旧 mock 结果不构成最新 TLS 库已完成 4.0 真实调用验收。
- 首次完整验签仍较慢，且一次验签期间不能抢占取消；缓存仅存在于本次库打开期间，当前每次新请求可能重新预验证。
- 未验证实体掌机、串口 PPP、无线扩展或其他真实设备网络链路。
- OS 3.5 ROM 已找到，但本次未单独运行；3.1 和 4.0 的结果不构成所有设备组合的完整兼容保证。
- 中文字体/输入、长时间运行、低内存压力、自动关机、重命名/删除、长文本滚动及取消请求尚未逐项完成模拟器验收。
- 在 OS 4.0 的已保存会话上再次覆盖安装时，模拟器曾在 `EmSession::ExecuteSubroutine / DmDeleteDatabase` 内断言退出；初次安装与收发已通过。最终修订版已回到 OS 3.1 成功覆盖安装。随后通过先恢复 4.0 会话、待系统运行后从工具栏安装 PRC 成功完成最终版覆盖安装并打开 Moss，历史记录完整。CLI 立即安装的断言根因仍未定位；无需清空原会话数据。
- OS 4.0 的收发检查使用草稿隔离修正前的同版代码；最终草稿修正的 PRC 已通过 OS 3.1 与 OS 4.0 安装运行检查。
- 当前是可构建运行的第一版；产品文档中的可配置 token 上限、按最近活动排序及 P1/P2 扩展仍待实现。
