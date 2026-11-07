# Device protocol v2

**语言 / Language:** [简体中文](#简体中文) · [English](#english)

## English

`commands.json` is the command allowlist used by firmware and clients. Obsolete actuator commands are rejected. JSON numbers must be finite. Motion remains bounded by the existing hard speed/duration limits.

## BLE

- Name: `KARI-<ID suffix or custom name>` (2.2.0+); older firmware uses `KARI Body`
- Service: `7f2c2b6a-2d8f-4f6b-9a59-8a7f9d7c0001`
- Write: `7f2c2b6a-2d8f-4f6b-9a59-8a7f9d7c0002`
- Notify: `7f2c2b6a-2d8f-4f6b-9a59-8a7f9d7c0003`

Commands are UTF-8 JSON with a trailing newline. Notifications may span chunks; decode incrementally and split by newline. Include `id`; replies echo it. Motion, stop, ping and existing config commands preserve their previous wire shapes. Management writes include `admin_password`. BLE does not add encrypted pairing in this version.

## HTTP

- `GET /`: built-in management panel, HTTP Basic authentication.
- `GET /api/status`: device identity, capabilities, command list, network/calibration/OTA state, `control_owner`, `control_token`, and `csrf_token`. Never returns router passwords.
- `POST /api/command`: the same JSON command dispatcher as BLE.
- `POST /update`: multipart form with one `firmware` application image.

All APIs require HTTP Basic `admin:<device password>`. Mutating requests require `X-OTA-Token` from status and, if an Origin is present, the same origin. Command requests also require a stable per-client `X-Kari-Client` value. A control source is `ble` or `http:<client ID>`.

## Operations

| Command | Fields / behavior |
| --- | --- |
| `device_status`, `ota_status`, `calibration_get` | Reads current status; does not move motors |
| `wifi_scan` | Start asynchronous scan; poll until `scanning=false`, then consume `networks` |
| `wifi_configure` | `ssid`, `password`; empty password supports open networks; otherwise 8–63 bytes |
| `wifi_forget` | Persist empty network configuration and reopen hotspot |
| `calibration_set` | Boolean `left_invert`, `right_invert`, `swap`; or `reset=true`; or shortcut `reverse_forward` / `reverse_steering` |
| `control_acquire` | Claims an idle/same-client controller and returns a new `control_token`; `takeover=true` explicitly replaces another controller |
| `control_renew` | Current `control_token`; renew lease within one second |
| `control_release` | Release this client's control |
| `calibration_test` | Current `control_token`; `direction` is forward/backward/left/right/left_forward/left_backward/right_forward/right_backward; always <=300 ms |
| `motion` | `linear`, `angular`, `duration_ms`, optional `ramp_ms`, `label`; HTTP also supplies current `control_token` |
| `stop` | Always permitted for authenticated HTTP clients; invalidates ownership and pending old tokens |
| `ota_start`, `ota_exit` | Enter locked upgrade mode / leave it when no upload is active |
| `firmware_check` | Fetch latest trusted release manifest; ESP must be connected and time synchronized |
| `firmware_install` | `version` from the checked manifest; verified HTTPS download and OTA |

Successful management replies use `type=ack`, `cmd`, `accepted=true`; failures use `type=error`, `accepted=false`, `message`. Existing `config_get` and `ping` retain `config` and `pong` responses.

Uploads include `X-Firmware-Size` and lowercase hexadecimal `X-Firmware-SHA256`. Firmware must fit the inactive 3 MB application partition. A successful update schedules reboot; reconnect and verify `firmware_version`. A failed upload can be retried or exited, and cannot resume old motor movement.

### Post-update hotspot option (2.0.1+)

`ota_start` and `firmware_install` accept optional boolean `close_ap_after_update` (default `true`). Status reports `ota_ap_shutdown_supported`, `close_ap_after_update` and `ap_suppressed_after_ota`. The selection is bound to the OTA target partition and consumed on the following boot. Failed/incomplete OTA must not suppress the original partition's hotspot; BOOT explicitly reopens it. Prior versions require manually leaving the hotspot. This option never chooses the phone's internet connection.

## Hardware configuration protocol v1 (firmware 2.1.0+)

`hardware_get` returns normal device identity/status plus `hardware_profile`, `hardware_configured`, `hardware_ready`, `hardware_error`, `hardware_protocol: 1`, `flash_mb`, `physical_flash_bytes`, `psram_bytes` and `partition`. An unconfigured profile has an empty `pins` object and `hardware_ready: false`. Board names are `n16r8` and `n8r8`.

`hardware_set` takes a complete `hardware_profile`:

```json
{"type":"hardware_set","id":"unique-request-id","hardware_profile":{"version":1,"board":"n16r8","pins":{"ena":17,"in1":16,"in2":15,"in3":7,"in4":6,"enb":5}}}
```

HTTP uses existing Basic/CSRF authentication; BLE and USB mutations require `admin_password`. Changes revoke control and stop before saving. An accepted write replies with `restarting: true`, then reboots after 1.5 seconds. Never infer the new active profile from the acknowledgement: reconnect, read it back and require `hardware_ready: true`. Failed writes preserve the previous NVS blob; active mapping never changes before reboot.

USB UART0 (115200) and native Serial/JTAG use newline-delimited JSON with string IDs up to 80 bytes and requests up to 2048 bytes. The base hardware commands are `hardware_get`, `hardware_set`, `device_status`; 2.2.0 identity/authentication extensions are documented below. Ignore boot log lines and correlate reply IDs; errors are `{type:"error",accepted:false,message:...}`. No USB motion or Wi-Fi credentials commands exist. Some UART bridges reset the board when opened; allow boot to finish before sending management requests rather than repeatedly reflashing on a readback timeout. Changing passwords still follows the existing firmware management policy.

Pin order is ENA, IN1, IN2, IN3, IN4, ENB; A is left, B is right. All must be distinct integers from the board profile's allowed subset. GPIO38/48 (board LEDs), straps, UART0, native USB and memory pins are reserved. GPIO 4 (Trig) and GPIO 18 (Echo) are reserved for the HC-SR04-P ultrasonic sensor and are rejected as motor outputs. This profile covers DevKitC-1-compatible layouts, not every board carrying an N16R8/N8R8 module.

OTA applications embed `KARI_HW_V1|BOARD|1|PARTITION|END`, checked across streaming upload chunks before activating the new application. Missing/mismatched descriptors, including 2.0.x packages, are rejected. This is compatibility metadata, not a cryptographic signature. N16R8 keeps the legacy manifest URL; N8R8 uses `kari-s3-n8r8-manifest.json`. Local-file uploads follow the same device-side policy. Clients remember the last connected board for offline downloads; connect once to an N8R8 device before preparing its first offline package.


### OTA update channels

`firmware_check` and `firmware_install` accept `channel: "stable" | "preview"`, default `stable`; installation must supply the actual selected `release.channel` returned by the preceding check. Unknown channels are rejected. Stable uses the existing latest/download manifest; preview opt-in fetches both stable and releases/download/preview/ with the same per-board manifest names, chooses the higher semantic version, and prefers stable on equal precedence. Release versions outrank their prereleases; build metadata and publication timestamps cannot make an older version win. If one feed is unavailable or invalid, the other valid feed remains usable. The rolling `preview` release is marked prerelease and contains only channel pointers to immutable version-tagged application URLs. The device always enforces profile compatibility, HTTPS, size and SHA-256 regardless of channel. Local-file OTA is an explicit selection outside this online-channel preference.

## Device identity and asynchronous OTA (2.2.0+)

BLE advertises the service UUID unchanged. The full local name is in scan response: `KARI-<last six ID characters>` by default, or `KARI-<custom name>`. `device_id` and mDNS hostname never change. Identity is a separate NVS blob; the v2 network/calibration record and hardware profile are preserved.

| Command | Request / result |
| --- | --- |
| `admin_auth` | Verify current administration credentials; returns status without credentials |
| `device_name_set` | `name`: visible UTF-8 text, 1–24 bytes; persists, ACKs `restarting: true`, then reboots |
| `admin_password_set` | `new_password`: 8–63 printable ASCII characters; becomes HTTP, BLE/USB management and hotspot password after reboot |
| `ota_start` | Optional `hotspot: true` explicitly opens AP; AP also opens when STA is disconnected |
| `firmware_check` | Returns `job_id`, `ota_phase: checking`; poll `ota_status` until `ready` or `failed` |
| `firmware_install` | Checked `version` and matching `channel`; returns job acknowledgement before downloading |

Management mutations and authentication over BLE/USB require `admin_password`. HTTP retains Basic authentication, CSRF and client identifiers. Status advertises `device_identity`, `admin_password`, `async_ota` capabilities, plus `device_name_custom` / `ap_name_custom`. Passwords are never returned. Name/password updates stop motors, deliver the ACK using the current connection, then reboot after 1.5 seconds; clients reconnect and verify identity.

OTA status reports `job_id`, `ota_phase` (`idle`, `checking`, `ready`, `downloading`, `failed`, `rebooting`), `ota_progress` (0–100), `error`, and the checked `release`. Network reads run on a worker; the Arduino loop alone owns flash writes and services status between bounded chunks. An active download cannot be replaced or exited. Failed jobs leave motors locked until explicit exit; clients may offer hotspot recovery after the job has ended. A reboot acknowledgement is not proof of completion: reconnect and verify `device_id` and `firmware_version`.

## 简体中文

### 设备协议 v2

`commands.json` 是固件和客户端共用的命令白名单。废弃的执行器命令会被拒绝；JSON 数字必须有限；运动保持既有速度和时长上限。

### BLE

- 名称：`KARI-<ID 后六位或自定义名字>`（2.2.0+）；较早的固件使用 `KARI Body`。
- 服务：`7f2c2b6a-2d8f-4f6b-9a59-8a7f9d7c0001`。
- 写特征：`7f2c2b6a-2d8f-4f6b-9a59-8a7f9d7c0002`。
- 通知特征：`7f2c2b6a-2d8f-4f6b-9a59-8a7f9d7c0003`。

命令是以换行结束的 UTF-8 JSON。通知可能分块，应增量解码并按换行分帧。请求包含 `id`，响应回显相同 ID。运动、停止、ping 与原有配置命令保持既有结构；管理写入包含 `admin_password`。本版不增加 BLE 加密配对。

### HTTP

| 接口 | 行为 |
| --- | --- |
| `GET /` | 内建设备管理面板，使用 HTTP Basic 认证 |
| `GET /api/status` | 身份、能力、命令、网络/校准/OTA 状态、control_owner、control_token、csrf_token；不返回路由器密码 |
| `POST /api/command` | 与 BLE 相同的 JSON 命令分发 |
| `POST /update` | multipart 表单中的单个 firmware 应用镜像 |

全部 API 要求 Basic `admin:<设备密码>`。修改请求携带状态返回的 `X-OTA-Token`；若提供 Origin，必须同源。命令还需要稳定的每客户端 `X-Kari-Client`，控制来源为 `ble` 或 `http:<客户端 ID>`。

### 操作

| 命令 | 字段与行为 |
| --- | --- |
| device_status / ota_status / calibration_get | 只读状态，不移动电机 |
| wifi_scan | 异步扫描；轮询至 scanning=false 后读取 networks |
| wifi_configure | ssid/password；空密码为开放网络，其他密码为 8–63 字节 |
| wifi_forget | 保存空网络配置并重新开启热点 |
| calibration_set | 布尔 left_invert/right_invert/swap；或 reset=true；或 reverse_forward/reverse_steering 快捷操作 |
| control_acquire | 获取空闲/本客户端控制权并返回新 control_token；takeover=true 明确接管其他来源 |
| control_renew | 使用当前 control_token，在一秒内续租 |
| control_release | 释放本客户端控制权 |
| calibration_test | 当前 control_token；direction 支持 forward/backward/left/right/left_forward/left_backward/right_forward/right_backward；不超过 300 ms |
| motion | linear/angular/duration_ms，可选 ramp_ms/label；HTTP 还携带当前 control_token |
| stop | 已认证 HTTP 客户端始终可停止；撤销控制权与旧 token |
| ota_start / ota_exit | 进入锁定升级模式；无上传时退出 |
| firmware_check | 获取可信最新清单；ESP 必须联网并完成时间同步 |
| firmware_install | 使用已检查清单的 version；经 HTTPS、大小、哈希验证后 OTA |

成功管理响应包含 `type=ack`、cmd、accepted=true；失败为 type=error、accepted=false、message。config_get 和 ping 仍返回 config / pong。

上传携带 `X-Firmware-Size` 与小写十六进制 `X-Firmware-SHA256`；镜像必须适合未激活的 3 MB 应用分区。成功后计划重启，重连核验 firmware_version。失败可重试或退出，不能恢复旧电机动作。

### 升级后热点选项（2.0.1+）

ota_start / firmware_install 可带布尔 close_ap_after_update，默认 true。状态返回 ota_ap_shutdown_supported、close_ap_after_update、ap_suppressed_after_ota。选项绑定 OTA 目标分区，仅下一次启动消费；失败/不完整更新不能抑制原分区热点。BOOT 可明确重开；早期版本需手动离开热点。此选项不控制手机互联网连接。

### 硬件配置协议 v1（2.1.0+）

hardware_get 返回通常状态及 hardware_profile、hardware_configured、hardware_ready、hardware_error、hardware_protocol=1、flash_mb、physical_flash_bytes、psram_bytes、partition。未配置时 pins 为空、hardware_ready=false。板型为 n16r8 或 n8r8。

hardware_set 必须提供完整配置：

```json
{"type":"hardware_set","id":"unique-request-id","hardware_profile":{"version":1,"board":"n16r8","pins":{"ena":17,"in1":16,"in2":15,"in3":7,"in4":6,"enb":5}}}
```

HTTP 沿用 Basic/CSRF，BLE/USB 修改需 admin_password。保存前撤销控制并停止；成功响应 restarting=true，约 1.5 秒后重启。ACK 不证明新配置已生效，必须重连回读并要求 hardware_ready=true。失败保留原 NVS blob，重启前不切换活动映射。

UART0（115200）及原生 Serial/JTAG 使用换行 JSON，字符串 ID 最多 80 字节，请求最多 2048 字节；忽略启动日志并关联响应 ID。基础硬件命令为 hardware_get/hardware_set/device_status；2.2.0 的身份管理扩展见下文。USB 不提供运动或 Wi-Fi 凭据操作。打开串口可能触发桥接板复位，应等待启动完成再发送管理请求，不能将回读超时直接视为需要重刷。

引脚顺序为 ENA、IN1、IN2、IN3、IN4、ENB；A 为左、B 为右。必须是板型允许集合中的互异整数；板载 LED 的 GPIO38/48、启动绑带、UART0、原生 USB 和存储器相关引脚保留。GPIO 4（Trig）和 GPIO 18（Echo）保留给 HC-SR04-P 超声波模块，不能用作电机输出。本配置针对 DevKitC-1 兼容布局，不适用于所有带 N16R8/N8R8 模组的板。

OTA 应用嵌入 `KARI_HW_V1|BOARD|1|PARTITION|END`，跨上传分块检查后才激活。缺失或不匹配描述（包括 2.0.x 镜像）拒绝；它是兼容元数据而非加密签名。N16R8 沿用默认清单，N8R8 使用 kari-s3-n8r8-manifest.json。本地文件执行相同设备策略；客户端记住上次板型以离线下载，N8R8 第一次准备离线包前应先连接一次。

### OTA 通道

firmware_check / firmware_install 接受 channel=stable 或 preview，默认 stable；安装必须使用上次检查返回的真实 release.channel，未知通道拒绝。稳定源为 latest/download；预览开启时同时请求稳定源与 releases/download/preview/ 的同名板型清单，选择更高语义版本，同优先级选择稳定版。正式版本高于其预发布；构建元数据和时间戳不会使旧版胜出。一源无效不阻止另一有效源。滚动 preview Release 标记为预发布，只提供指向不可变版本应用 URL 的通道指针。任何通道都验证板型、HTTPS、大小和 SHA-256；本地文件是独立明确选择。

### 身份与异步 OTA（2.2.0+）

服务 UUID 不变，完整本地名在扫描响应中；默认 KARI-加ID后六位，自定义为 KARI-加名字。device_id 和 mDNS 主机名稳定。身份独立存储于 NVS，不覆盖 v2 网络/校准或接线记录。

| 命令 | 请求与结果 |
| --- | --- |
| admin_auth | 校验当前管理凭据，只返回状态 |
| device_name_set | name 为 1–24 字节可见 UTF-8，保存、ACK restarting=true 后重启 |
| admin_password_set | new_password 为 8–63 个可打印 ASCII 字符，重启后用于 HTTP、BLE/USB 管理和热点 |
| ota_start | 可选 hotspot=true 明确开启 AP；STA 断开时也开启 |
| firmware_check | 先返回 job_id、ota_phase=checking；轮询 ota_status 至 ready / failed |
| firmware_install | 已检查 version 和匹配 channel；下载前即确认作业 |

BLE/USB 管理修改和认证需 admin_password；HTTP 保留 Basic、CSRF、客户端标识。状态发布 device_identity/admin_password/async_ota 能力以及 device_name_custom/ap_name_custom，从不返回密码。改名/改密码先停电机，用当前连接发 ACK，再约 1.5 秒后重启，客户端重连确认。

OTA 状态为 job_id、ota_phase（idle/checking/ready/downloading/failed/rebooting）、0–100 的 ota_progress、error 和已检查 release。工作线程读取网络，仅 Arduino 主循环写 Flash，并在有界分块间服务查询。活动下载不能替换或退出；失败保持电机锁定至明确退出，结束后客户端可提供热点恢复。重启 ACK 不是完成证明，必须重连检查 device_id 和 firmware_version。
