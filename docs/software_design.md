# ESP32-S3 Storybook Console 软件设计文档

## 1. 文档目的

本文档定义当前仓库第一阶段的正式实现目标与边界：

- 基于 `ESP-Brookesia` 做一个独立的 `Settings` App
- 首阶段功能只做 5 组能力：
  - Wi-Fi 扫描
  - Wi-Fi 连接
  - 屏幕亮度调节
  - 音量调节
  - 时间设置
  - 设备信息查询
- 所有底层实现必须严格落在官方基线之上，而不是参考历史临时工程

本文档不是历史问题复盘，而是后续代码实现的约束。

## 2. 当前文档审查结论

现有版本有几个方向是对的，但已经不适合当前第一目标。

### 2.1 合理的部分

- 明确要求优先对齐官方示例和官方 BSP
- 明确禁止 UI 直接调用底层驱动
- 明确要求先做基线验证，再做页面接入

### 2.2 需要修正的部分

1. 顶层产品划分和当前目标冲突
   - 旧文档把亮度和音量从 `Parent Care` 中拆走
   - 但当前第一目标是“先做一个设置 App”，它天然就是系统设置入口
   - 正确做法不是把功能拆成多个顶层 App，而是在一个 `Settings App` 里做分组页面或分区卡片，同时保证服务层解耦

2. 文档太强调历史问题，缺少可执行接口定义
   - 旧文档写了很多“不要做什么”
   - 但没有把 `SettingsService` 需要暴露哪些状态、哪些 API、哪些 NVS 键写清楚

3. 没有完整定义“时间设置”
   - 旧文档只提了“自动对时”
   - 当前目标里明确包含“设置时间”
   - 因此必须同时定义：
     - 自动对时
     - 手动设置时间
     - 两者的切换规则

4. 没有把 Brookesia 接入方式写到实现级别
   - 当前工程是典型的 `Phone + App Registry` 结构
   - 文档需要明确：
     - 设置页是一个独立 `App`
     - 页面生命周期由 Brookesia 管理
     - UI 状态刷新通过服务查询或定时刷新完成

## 3. 第一阶段目标

### 3.1 产品目标

第一阶段只交付一个 `Settings` App，作为系统设置入口。

首阶段功能清单：

- Wi-Fi 扫描 AP 列表
- 输入密码并连接 Wi-Fi
- 显示当前网络状态
- 调整屏幕亮度
- 调整音量
- 自动对时开关
- 手动设置日期和时间
- 查询设备基础信息与 PMU 电源状态

### 3.2 不在第一阶段范围内

以下内容不进入当前里程碑：

- OTA
- 多语言切换
- 电池页和 PMU 深度诊断页
- 家长控制业务逻辑
- 内容播放业务逻辑
- 复杂设置导航体系

## 4. 官方基线

第一阶段只允许建立在以下官方基线之上。

### 4.1 ESP-Brookesia 基线

用于：

- `Phone` 容器
- App 注册与安装
- App 生命周期
- Status Bar / Navigation Bar / Launcher 体系

项目层约束：

- 设置功能必须作为一个标准 Brookesia `App`
- 不修改 Brookesia 核心行为模型
- 不绕过 App 生命周期直接管理整页资源

### 4.2 ESP-IDF 官方基线

Wi-Fi 必须对齐：

- `examples/wifi/getting_started/station`
- `examples/wifi/scan`

时间能力必须对齐：

- `examples/protocols/sntp`
- `settimeofday()` / `localtime_r()` / `tzset()` 的标准用法

项目层约束：

- Wi-Fi 事件流必须保持官方模型
- 扫描完成后通过 `esp_wifi_scan_get_ap_records()` 取结果
- 获取 IP 才视为连接成功
- 自动对时只在网络可用后启动

### 4.3 板卡 BSP 基线

板级能力只通过 Waveshare BSP 暴露的接口接入：

- `bsp_display_brightness_set()`
- `bsp_audio_codec_speaker_init()`
- `esp_codec_dev_open()`
- `esp_codec_dev_set_out_vol()`

项目层约束：

- 亮度不重写背光驱动
- 音量不自建第二套 codec 控制层
- 板级能力只做最小封装

## 5. 目标架构

第一阶段采用三层最小架构。

### 5.1 第 0 层: 官方层

包含：

- `ESP-Brookesia`
- `ESP-IDF` 官方 Wi-Fi / SNTP 模型
- Waveshare BSP

职责：

- 提供唯一可信的底层行为模型

### 5.2 第 1 层: Settings Service

模块名：

- `components/settings_service`

职责：

- 管理 NVS 配置
- 管理亮度和音量
- 管理 Wi-Fi STA 初始化、扫描、连接、重试、状态快照
- 管理自动对时与手动设时
- 向 UI 暴露稳定、可轮询的状态接口

边界：

- 可以调用 `esp_wifi_*`
- 可以调用 BSP 和 `esp_codec_dev_*`
- 不能依赖任何 LVGL 对象
- 不能依赖任何 Brookesia 页面对象

### 5.3 第 2 层: Settings App

模块名：

- `components/brookesia_app_settings`

职责：

- 创建设置页 UI
- 绑定按钮、滑条、密码弹窗、时间弹窗
- 只通过 `SettingsService` 交互
- 不直接调用底层驱动和协议接口

边界：

- 页面只表达用户意图
- 页面不保存 Wi-Fi 状态机
- 页面不保存驱动句柄

## 6. Settings Service 设计

### 6.1 服务暴露的能力

必须提供以下最小接口：

- `begin()`
- `ensureWifiStarted()`
- `startWifiScan()`
- `connectWifi(ssid, password)`
- `getWifiState()`
- `getWifiScanResults()`
- `getConnectedSsid()`
- `getWifiSignalLevel()`
- `setBrightness(value)`
- `setVolume(value)`
- `setAutoTimeSync(enable)`
- `setManualDateTime(date, time)`
- `getCurrentDateText()`
- `getCurrentTimeText()`
- `getDeviceInfoEntries()`
- `getPowerStatus()`

### 6.2 状态模型

Wi-Fi 状态定义：

- `Idle`
- `Ready`
- `Scanning`
- `Connecting`
- `Connected`
- `Error`

Wi-Fi 错误定义：

- `None`
- `NotFound`
- `AuthFailed`
- `Timeout`
- `AssocFailed`
- `DriverError`
- `Unknown`

### 6.3 NVS 键设计

当前阶段只持久化：

- `brightness`
- `volume`
- `auto_sync`
- `wifi_ssid`
- `wifi_pass`

规则：

- Wi-Fi 凭据只在连接成功后落盘
- 手动设置的时间不持久化
- 时间有效性由系统当前 epoch 判断

## 7. Wi-Fi 设计

### 7.1 连接模型

必须保持以下顺序：

1. `esp_netif_init()`
2. `esp_event_loop_create_default()`
3. `esp_netif_create_default_wifi_sta()`
4. `esp_wifi_init()`
5. `esp_wifi_set_storage(WIFI_STORAGE_RAM)`
6. `esp_wifi_set_mode(WIFI_MODE_STA)`
7. `esp_wifi_set_config(WIFI_IF_STA, &wifi_config)`
8. `esp_wifi_start()`
9. `WIFI_EVENT_STA_START`
10. `esp_wifi_connect()`
11. `IP_EVENT_STA_GOT_IP`

### 7.2 断线与重试策略

当前阶段只允许有限重试：

- `WIFI_EVENT_STA_DISCONNECTED` 后限次重试
- 达到上限后转入 `Error`
- 不实现复杂恢复调度器
- 不做扫描和连接交错恢复的复杂逻辑

### 7.3 扫描策略

当前阶段使用最小扫描模型：

- STA 模式下发起扫描
- 异步等待 `WIFI_EVENT_SCAN_DONE`
- 读取 AP 记录
- 结果按 RSSI 排序
- 页面只显示前若干项

## 8. 时间设计

### 8.1 自动对时

规则：

- 只有在 Wi-Fi 已拿到 IP 后才启动 SNTP
- 自动对时开启时，优先以网络时间为准
- 自动对时关闭时，停止 SNTP 服务

### 8.2 手动设时

规则：

- 手动设置只在“自动对时关闭”时允许
- UI 输入格式固定为：
  - 日期 `YYYY-MM-DD`
  - 时间 `HH:MM`
- 服务层负责解析并调用 `settimeofday()`

### 8.3 时区策略

当前阶段先固定单一时区：

- `CST-8`

说明：

- 第一阶段先完成“可设置时间”
- 时区配置作为后续需求，不在本轮实现

## 9. 启动顺序

当前工程的正式启动顺序如下：

1. `bsp_display_start_with_config()`
2. `bsp_display_backlight_on()`
3. `SettingsService::begin()`
4. `SettingsService::ensureWifiStarted()`
5. Brookesia `Phone::begin()`
6. 从 App Registry 初始化并安装 App
7. 周期刷新状态栏时间和 Wi-Fi 图标

约束：

- 设置服务必须早于 Settings App 页面创建
- 页面不负责补做底层初始化

## 10. UI 设计约束

### 10.1 页面结构

首版 Settings App 使用单页纵向滚动布局，按功能卡片分区：

- Hero 区
- Device Info 卡片（含电量、充电状态、PMU 温度）
- Display 卡片
- Audio 卡片
- Time 卡片
- Wi-Fi 卡片

### 10.2 交互约束

- Wi-Fi 密码输入使用弹层和键盘
- 手动设时使用独立弹层
- 页面只读服务状态，不保存底层状态机
- 页面刷新采用定时轮询即可，不引入额外事件总线

## 11. 目录规划

第一阶段落地目录如下：

```text
main/
  main.cpp

components/
  settings_service/
  brookesia_app_settings/
  brookesia_core/
```

后续如果设置能力继续变大，再考虑拆成：

- `network_service`
- `time_service`
- `audio_service`
- `display_service`

当前阶段不提前拆过细。

## 12. 验证矩阵

### 12.1 Wi-Fi

- 冷启动后服务可正常初始化
- 点击扫描可看到 AP 列表
- 输入正确密码能拿到 IP
- 输入错误密码能进入错误状态
- AP 不存在时能进入错误状态

### 12.2 亮度

- 启动后可正常亮屏
- 滑条调亮和调暗不崩溃

### 12.3 音量

- 首次调节能完成 codec 初始化
- 音量调整不重启

### 12.4 时间

- 打开自动对时后，联网可更新时间
- 关闭自动对时后，可手动设置日期和时间
- 状态栏时钟能随系统时间变化

### 12.5 设备信息

- 设置页可直接查看芯片、固件版本、ESP-IDF 版本
- 可查看 Flash、PSRAM、MAC 等基础设备信息
- 可查看电池电量、充电状态、PMU 温度
- 设备信息查询不依赖旧的 07 工程页面结构

## 13. 里程碑

### M0

- 重写设计文档
- 确认官方基线

### M1

- 从 0 实现 `SettingsService`
- 从 0 实现 `Settings App`

### M2

- 真机闭环 Wi-Fi / 亮度 / 音量 / 时间

### M3

- 根据真机问题再决定是否拆服务、补 PMU、电池、语言等扩展设置

---

本文档是当前仓库第一阶段设置功能开发的正式基线。若后续目标变化，必须先更新本文档，再调整实现。






