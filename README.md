# KARI DF26 Firmware

<p align="center">
  <img src="docs/assets/app-icon-rounded.png" alt="KARI 图标 / KARI app icon" width="128" height="128">
</p>

**语言 / Language:** [简体中文](#简体中文) · [English](#english)

## 简体中文

[统一使用文档](https://github.com/xlle-er/KARI-DF26) · [首次安装](https://github.com/xlle-er/KARI-DF26/blob/main/docs/installation.md) · [部署工具](https://github.com/xlle-er/KARI-DF26-Skill)

ESP32-S3 底盘固件：BLE / Wi-Fi 控制、热点配网、设备面板、持久化接线、方向校准和双分区 OTA。当前业务版本 **2.2.2**，首次 KARI 构建。支持 N16R8、N8R8；本次重新构建与契约检查已通过，维护者确认既有真机测试通过并授权正式稳定发布。详见 [VERIFICATION.md](VERIFICATION.md)。

## 官方接线

| L298N | GPIO |
| --- | --- |
| 左 IN1 / IN2 / ENA | 16 / 15 / 17 |
| 右 IN3 / IN4 / ENB | 7 / 6 / 5 |

| HC-SR04 / HC-SR04-P | 连接 |
| --- | --- |
| Vcc | 3V3（宽压模块 3–5.5 V，不要接 ESP32 的 5 V 脚） |
| Trig | GPIO 4 |
| Echo | GPIO 18（HC-SR04-P 可直接接 3.3 V GPIO） |
| Gnd | GND（与 ESP32、L298N 共地） |

GPIO 为芯片编号，不是排针序号。ESP32 与 L298N 共地，拔掉 ENA / ENB 跳帽。首次配置前断开电机电源，未确认接线前固件不启用电机输出；驱动输入浮空时需硬件下拉或保持电机断电。

超声波模块正面针序为 Vcc / Trig / Echo / Gnd，背面丝印为 Gnd / Echo / Trig / Vcc。GPIO 4 和 GPIO 18 保留给测距，不能再配置成电机脚；默认电机脚 17 / 16 / 15 / 7 / 6 / 5 与这两脚不冲突。

前方距离小于 `OBSTACLE_STOP_DISTANCE_CM`（默认 25 cm，见 `firmware/KariWifiOta/ObstaclePolicy.h`）时，固件接管电机输出：先停车约 200 ms，再按已校准的逻辑方向右转约 400 ms（与方向测试里的右转相同：左轮为正、右轮为负）。距离回到阈值以外，或这次短右转结束后，控制交还原来的遥控。若前方仍然过近且遥控仍在驱动，会再次停车并右转，不会把前进指令直接送到电机。静止时旁边有障碍不会自己转。无回波或测距超时视为远处，避免传感器缺失时一直转向。这是安全接管，不是自主导航。

## 首次安装

推荐使用 [KARI 部署工具](https://github.com/xlle-er/KARI-DF26-Skill)。网页安装源码位于 [installer](installer)，[公开安装入口](https://xlle-er.github.io/KARI-DF26-Firmware/)；本地运行：

```sh
python3 scripts/build.py
cd installer
npm ci
npm run dev
```

使用电脑 Chrome / Edge 打开显示的 localhost 地址，匹配板型后进行安装。构建过程本身不会烧录。N16R8 / N8R8 的分段 USB 清单分别为 `kari-s3-usb.json` / `kari-s3-n8r8-usb.json`；不要混用板型。

本次使用新的配置命名空间，需要重新配网、确认接线并校准。USB 安装不自动擦除整片 Flash，也不会读取历史品牌配置。

## 配网与管理

连接实际广播的 `KARI-…` 热点，打开 `http://192.168.4.1/`；接入路由器后，使用设备当前局域网 IP。设备 ID 为 `kari-<MAC>`，mDNS 为 `_kari._tcp`，BLE 名称为 `KARI-名字` 或默认 `KARI-识别码`。

初始设备管理用户名为 `admin`，初始密码为 `12345678`，请按设备面板提供的功能修改。密码不是路由器密码。长按 BOOT 约三秒后松开可重新进入配置入口。

架空车轮，短时测试方向并保存左右交换 / 电机反向设置。保存、接管和升级会先停车；不会自动恢复旧动作。

## 固件开发（维护者）

固定工具链：Arduino ESP32 **3.3.12**、ArduinoJson **7.4.3**、NimBLE-Arduino **2.5.1**。安装 Arduino CLI 后：

```sh
./scripts/build.sh
python3 tests/contracts.py
cd installer
npm ci
npm test
npm run build
```

`build.sh` 默认构建两种板型，也可传 `--board n16r8` 或 `--board n8r8`。生成结果位于 `dist/`，包含应用包、USB 分段清单、哈希、源文件摘要和安装页数据。安装页静态产物位于 `installer/dist/`。

跨端集成验证时，将 Android、iOS、Web、Skill 仓库按项目名称放在同级目录，运行：

```sh
python3 tests/contracts.py --require-clients
```

严格模式在缺少必要客户端时失败，避免跳过跨仓库检查。单仓库模式只执行本地可用检查。硬件策略以 [hardware/boards.json](hardware/boards.json) 为源，运行 `scripts/sync-hardware.py --check` 验证生成文件。

## 更新与发布

[KARI Releases](https://github.com/xlle-er/KARI-DF26-Firmware/releases) 使用新仓库地址。OTA 包为 `kari-s3.bin` / `kari-s3-n8r8.bin`，配套 JSON 包含版本、板型、大小和 SHA-256。USB 使用分段清单，OTA 使用应用包，两者不能互换。

稳定版发布后，在线更新及部署工具默认从 `latest` 获取配套清单；安装站点从同一版本发布资产生成，并核验两种板型文件。

相关客户端：[Web](https://github.com/xlle-er/KARI-DF26-WEB) · [Android](https://github.com/xlle-er/KARI-DF26-ANDROID) · [iOS](https://github.com/xlle-er/KARI-DF26-IOS)。

## English

[Unified usage guide](https://github.com/xlle-er/KARI-DF26) · [First installation](https://github.com/xlle-er/KARI-DF26/blob/main/docs/installation.md) · [Deployment tool](https://github.com/xlle-er/KARI-DF26-Skill)

ESP32-S3 chassis firmware provides BLE/Wi-Fi control, hotspot provisioning, device management, persistent wiring, direction calibration and dual-partition OTA. Current business version is **2.2.2**, the first KARI build, for N16R8 / N8R8. Rebuild and contract checks passed; the maintainer confirmed prior physical tests and approved stable publication. See [VERIFICATION.md](VERIFICATION.md).

### Official wiring

| L298N | GPIO |
| --- | --- |
| Left IN1 / IN2 / ENA | 16 / 15 / 17 |
| Right IN3 / IN4 / ENB | 7 / 6 / 5 |

| HC-SR04 / HC-SR04-P | Connection |
| --- | --- |
| Vcc | 3V3 (wide-voltage module, 3–5.5 V; do not use the ESP32 5 V pin) |
| Trig | GPIO 4 |
| Echo | GPIO 18 (HC-SR04-P echo may connect directly to a 3.3 V GPIO) |
| Gnd | GND (common with the ESP32 and L298N) |

Use chip GPIO numbers, not header positions. Share ground and remove ENA/ENB jumpers. Disconnect motor power for initial configuration. Firmware does not enable motor output before wiring confirmation; floating inputs need hardware pull-downs or disconnected power.

The module's front pin order is Vcc / Trig / Echo / Gnd; the back legend reads Gnd / Echo / Trig / Vcc. GPIO 4 and GPIO 18 are reserved for ranging and cannot be reused as motor outputs. Default motor pins 17 / 16 / 15 / 7 / 6 / 5 do not collide with them.

When the forward distance is below `OBSTACLE_STOP_DISTANCE_CM` (default 25 cm, `firmware/KariWifiOta/ObstaclePolicy.h`), firmware overrides motor output: stop for about 200 ms, then turn right for about 400 ms in the calibrated logical frame (same signs as the direction test's right turn: left wheel positive, right wheel negative). After the distance clears, or after that short turn, normal remote control resumes. If it is still too close and a drive command is still active, the car stops and turns right again instead of passing the forward command through. A stationary car does not spin just because something is nearby. A missing echo or range timeout counts as far, so a missing sensor does not keep the chassis turning. This is a safety override, not an autonomy stack.

### First installation

Use the [KARI deployment tool](https://github.com/xlle-er/KARI-DF26-Skill) or [public web installer](https://xlle-er.github.io/KARI-DF26-Firmware/). Installer source is in [installer](installer). For local operation:

```sh
python3 scripts/build.py
cd installer
npm ci
npm run dev
```

Open the displayed localhost URL in desktop Chrome / Edge and select the matching board. Building itself does not flash. Segmented USB manifests are `kari-s3-usb.json` for N16R8 and `kari-s3-n8r8-usb.json` for N8R8; never mix them.

The new configuration namespace requires Wi-Fi setup, wiring confirmation and calibration again. USB installation does not automatically erase the entire Flash and does not read historical-brand settings.

### Network and management

Join the actual advertised `KARI-…` hotspot and open `http://192.168.4.1/`; after joining a router, use the current LAN IP. Device ID is `kari-<MAC>`, mDNS is `_kari._tcp`, and the advertised name follows the configured KARI identity.

Initial administration credentials are user `admin`, password `12345678`; change them through the device panel. This is not the router password. Hold BOOT for about three seconds and release to reopen configuration.

Lift wheels for brief direction tests, then save inversion/swap. Saving, takeover and upgrades stop first; old actions never resume automatically.

### Development

Pinned tools: Arduino ESP32 **3.3.12**, ArduinoJson **7.4.3**, NimBLE-Arduino **2.5.1**. Install Arduino CLI, then:

```sh
./scripts/build.sh
python3 tests/contracts.py
cd installer
npm ci
npm test
npm run build
```

`build.sh` builds both profiles; `--board n16r8` / `--board n8r8` selects one. `dist/` contains application images, USB segments/manifests, hashes, source digests and installer data. Static site output is `installer/dist/`.

For integrated checks, place Android/iOS/Web/Skill repositories as siblings using their project names:

```sh
python3 tests/contracts.py --require-clients
```

Strict mode fails when a required client is absent. Standalone mode runs locally available checks. Hardware policy originates from [hardware/boards.json](hardware/boards.json); validate generated consumers with `scripts/sync-hardware.py --check`.

### Updates and releases

[KARI Releases](https://github.com/xlle-er/KARI-DF26-Firmware/releases) uses the new repository. OTA images are `kari-s3.bin` / `kari-s3-n8r8.bin`; JSON records version, board, size and SHA-256. USB segments and OTA applications are not interchangeable.

Online updates and the deployment tool default to `latest` stable manifests. The installation site is generated from and verifies the same release's two board profiles.

Clients: [Web](https://github.com/xlle-er/KARI-DF26-WEB) · [Android](https://github.com/xlle-er/KARI-DF26-ANDROID) · [iOS](https://github.com/xlle-er/KARI-DF26-IOS).
