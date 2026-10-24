# Moss TLS Library

Palm OS 3.1–4.0 / Motorola 68000 的独立共享库，数据库名称 `Moss TLS Library`，类型 `libr`，Creator `MTLS`。其他原生应用可只包含 `MossTLS.h` 并安装 `MossTLS.prc`，不依赖 Moss 聊天客户端。

## 当前协议配置

- BearSSL 0.6（MIT，见 `vendor/bearssl-0.6/LICENSE.txt`）。
- TLS 1.2，RSA 密钥交换 / AES-128-GCM / SHA-256，SNI。
- 默认版本执行 X.509 证书链、主机名、有效期验证；当前信任库只含 DigiCert Global Root G2。
- 当前使用用户明确批准的静态 RSA 兼容模式，不提供前向保密：若服务器 RSA 私钥未来泄露，过去记录的通信可能被解密。
- 不支持 TLS 1.3、ECDSA 服务器证书、ECDHE 及通用系统根证书库。其他 HTTPS 服务需要评估证书链与密码套件后扩充配置。
- Palm Net Library 负责已连接的 TCP socket；加解密与证书校验全部在 Palm 68K 端完成。没有 TLS 代理。
- 每个会话约需 33 KB 引擎上下文（含最多 6 KB 证书链缓存）及 16 KB 专用栈；约 43 KB 引擎代码重定位到临时存储数据库，不占用同等大小的动态堆。

该移植仍处于开发验证阶段；真实 API 和各 ROM 的结果以项目 `VALIDATION.md` 为准。

## 生命周期

1. `SysLibFind(MOSS_TLS_NAME, &ref)`；不存在时 `SysLibLoad('libr', MOSS_TLS_CREATOR, &ref)`。
2. `MossTLSOpen(ref)`。
3. 调用方打开 Net Library，创建并连接 TCP socket。
4. `MossTLSCreate(ref, netRef, socket, host, &session)`；host 必须为不含端口的 DNS 名称，用于 SNI 与证书校验。
5. 反复调用 `MossTLSPump(ref, session, &state, &detail)`，直到 `state & MT_STATE_READY`。若同时有 `MT_STATE_RECONNECT`，证书已经完成首次验证；为避开服务器空闲超时，Moss 先销毁当前会话并关闭 socket，保留已打开的库，再连接并创建新会话。
6. `MossTLSWrite` / `MossTLSRead` 处理明文；每次可能只处理部分数据或返回零长度，期间持续调用 Pump。
7. 完成或取消时 `MossTLSDestroy`；关闭调用方持有的 socket / Net Library。
8. `MossTLSClose` 返回成功后可 `SysLibRemove(ref)`；返回 `mtErrBusy` 时不能移除。

库不接管 socket 所有权、不存储 API Key、不解释 HTTP。Pump 的一次 ACK 可能进行较长的密码运算；当前不支持在此期间抢占取消。`MossTLSGetStats` 提供密文累计字节及状态，`MossTLSLastX509Error` 提供 BearSSL X.509 状态（32 为校验成功）。

## 慢速设备的验证顺序

`deferred_x509.c` 在握手时缓存有界的完整 DER 证书链，并只解码叶证书公钥。TLS 握手结束后，`core.c` 调用原版 BearSSL X.509 验证器检查完整链、信任锚、域名、有效期及 RSA 密钥交换用途。在验证通过之前，核心不提供应用读写缓冲区，也不接受应用数据 ACK 或 FLUSH；失败后保持关闭状态。已禁用 TLS 重新协商，避免复用过期的验证状态。

Palm 68K 的验签时间可能超过服务器的握手/HTTP 空闲等待时间。首次验证通过后，Moss 不发送 HTTP，而是立即重连。库仅在本次打开期间的 RAM 中保留验证缓存，内容为证书链和域名的 SHA-256 指纹，以及所有证书有效期的交集。第二次连接只有在完整链和域名逐字节相同（通过指纹比较）、当前时间仍在有效区间时才命中；否则重新做完整验证。缓存不包含 API Key、会话密钥或可复用的 TLS 会话，也不写入数据库。重复发生证书变化时最多重新预验证两次，随后失败。

这会增加首次请求的连接次数和等待时间，但避免重复发送问题；预验证连接不发送 HTTP。每条实际请求仍使用新的随机状态和独立 TLS 会话。

## 时间与随机种子

证书验证使用 Palm 系统时间及系统时区偏好，首次联网前需校准日期、时间与时区。

`MossTLSEntropy.pdb` 含操作系统安全随机源生成的 32 字节种子。每台设备必须单独生成并只安装一次；库在发出 ClientHello 前持久化更新种子。不得将同一初始种子分发给多台设备，不得每次启动重新覆盖它，也不得用可预测的 Palm SysRandom 替代。

当前打包脚本仅在本地种子文件不存在时创建；给另一台设备部署时，生成新的种子文件并单独安装：

```sh
python3 scripts/provision_entropy.py --output /private/tmp/NewPalmSeed.pdb
```

此命令拒绝覆盖已存在的文件。恢复旧模拟器快照会回退随机状态，需重新提供新种子后再联网。种子文件应按私密部署材料保管。

## 构建与实现

运行项目根目录 `./build.sh`，编译均在指定 Windows 7 上执行，本机脚本只打包已经编译的资源。

Palm 包装层采用 prc-tools 的 16 位 int ABI；BearSSL 引擎使用 `-mnoshort` 的 32 位 int ABI。二者只通过 `core_abi.h` 中显式 32 位 long / pointer 参数和独立栈交互。引擎不调用 Palm 系统函数。

`dispatch.S` 提供标准共享库跳转表及栈切换入口。`mtCD` 资源为独立引擎镜像，`mtRL` 列出绝对地址重定位位置；运行时复制到临时资源数据库，原 PRC 不会被修改。

RSA 使用原版 `rsa_i15_pub.c` 的入口和缓冲区布局。`i15_modpow2.c` 对公开指数 65537 使用固定的 16 次平方与 1 次乘法；其他指数保留原版流程。此客户端构建仅使用 RSA 公钥运算，不包含私钥签名或解密。

`rsa_verify.c` 单独编译证书验签所用的 RSA 与 Montgomery 乘法，操作数全部来自公开证书和签名，因此可使用未填充的 68000 MULU 与寄存器内层循环。`core.c` 只把这一入口注册到证书/签名验证器；RSA 密钥交换仍调用原版入口及 `moss_mul15.h` 的互补 MULU 配对，保护包含会话秘密的操作数。实验文件 `rsa_public.c` 不参与当前构建。这些移植改动尚未经过独立密码安全审计。

`tests/run.sh` 包含离线 RSA 回归：以 Python 模幂为独立参考，检查 1024/2048/4096 位模数、不同指数、前导零与边界输入，分别验证两个算术入口。主机测试不替代 Palm 上的实际验证。

`tests/tls_handshake.sh` 验证真实证书、错误域名和过期时间，并以本地不受信任 TLS 服务端确认握手结束后仍无法收到任何应用字节。运行 `MOSS_TLS_TEST_SOURCE="$PWD/tests/tls_cache_handshake.c" ./tests/tls_handshake.sh` 可验证缓存命中、指纹失效后重新验签、缓存存在时的域名和过期拒绝。

Pump 遇到正常超时/暂不可用会重试；零字节且无错误按连接断开处理。若同时允许接收，会先尝试读取对端 TLS 告警，避免把断开的连接无限重试到应用总超时。


## Mac 预验证指纹

默认构建可使用 `tls/preverified_deepseek.h` 中的公开证书链指纹。运行 `python3 scripts/preverify_deepseek.py`，由本机同一 BearSSL 验证核心完整校验证书后生成指纹与证书有效期交集；不使用 API Key。Palm 仅在域名、完整证书链指纹与有效期均匹配时省去重复验签，变化时回退到完整验证。`python3 scripts/preverify_deepseek.py --check` 检查信任策略是否变化。

## 跳过验签的独立实验版

按用户选择，可构建并启动：

```sh
MOSS_TLS_INSECURE=1 ./build.sh
MOSS_TLS_PRC="$PWD/dist/MossTLS-insecure.prc" ./run.sh
```

实验库仍由 Palm 执行 TLS 1.2 加密与密钥交换，但跳过证书链、域名、有效期验证，无法确认对端身份。它不进行验证预热重连；成功回复显示 `Ready (TLS dev)`，状态位 `MT_STATE_INSECURE` 明确标记此模式，不伪报 X.509 验证成功。

默认 `./build.sh` 保持验证模式，生成 `dist/MossTLS.prc`；实验构建另存为 `dist/MossTLS-insecure.prc`。两者 Palm 数据库标识相同，安装时互相替换，不能同时加载。`run.sh` 默认安装验证库。核心每次重新编译，避免切换模式时复用旧对象。

`MOSS_TLS_TEST_SOURCE="$PWD/tests/tls_insecure_handshake.c" ./tests/tls_handshake.sh` 检查实验版加密握手与有意跳过的域名/时间校验。Palm OS 3.1 真实 DeepSeek 验收见 `VALIDATION.md`。
