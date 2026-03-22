# 在 Arduino IDE 中使用

* [English Version](./use_with_arduino.md)

## 目录

- [在 Arduino IDE 中使用](#在-arduino-ide-中使用)
  - [目录](#目录)
  - [快速入门](#快速入门)
    - [环境准备](#环境准备)
  - [SDK 及依赖库](#sdk-及依赖库)
  - [安装库](#安装库)
  - [常见问题及解答](#常见问题及解答)
    - [Arduino 库的目录在哪儿？](#arduino-库的目录在哪儿)
    - [arduino-eps32 的安装目录以及 SDK 的目录在哪儿？](#arduino-eps32-的安装目录以及-sdk-的目录在哪儿)
    - [如何在 Arduino IDE 中安装 esp-boost？](#如何在-arduino-ide-中安装-esp-boost)
    - [在 Arduino IDE 中打开串口调试器看不到日志信息或日志信息显示不全，如何解决？](#在-arduino-ide-中打开串口调试器看不到日志信息或日志信息显示不全如何解决)

## 快速入门

### 环境准备

1. **安装 Arduino IDE**

- 从 [Arduino 官网](https://www.arduino.cc/en/software) 下载并安装 Arduino IDE，推荐使用 2.x 版本

2. **安装 ESP32 SDK**

- 打开 Arduino IDE
- 导航到 `File` > `Preferences`
- 在 `Additional boards manager URLs` 中添加:

  ```
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
  ```

- 导航到 `Tools` > `Board` > `Boards Manager`
- 搜索 `esp32` by `Espressif Systems` 并安装符合要求的版本（参阅 [SDK 及依赖库](#sdk-及依赖库)）

> [!NOTE]
> 如果遇到问题，请先查看 [常见问题及解答](#常见问题及解答) 章节。如果问题仍未解决，可以在 [GitHub Issues](https://github.com/espressif/esp-boost/issues) 提交问题。

## SDK 及依赖库

在使用本库之前，请确保已安装符合以下版本要求的 SDK 及依赖项：

|                           **SDK**                           | **版本要求**  |
| ----------------------------------------------------------- | ------------ |
| [arduino-esp32](https://github.com/espressif/arduino-esp32) | >= 3.1.0     |

> [!NOTE]
> * SDK 的安装方法请参阅 [Arduino ESP32 文档 - 安装](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)。

## 安装库

esp-boost 及依赖库已经上传到了 Arduino 库管理器，您可以按照如下步骤直接在线安装：

1. 在 Arduino IDE 中导航到 `Sketch` > `Include Library` > `Manage Libraries...`。
2. 搜索 `esp-boost` 及依赖库，点击 `Install` 按钮进行安装。

如果想要手动安装，可以通过 [Github](https://github.com/espressif/esp-boost) 或者 [Arduino Library](https://www.arduinolibraries.info/libraries/esp-boost) 下载所需版本的 `.zip` 文件，然后在 Arduino IDE 中导航到 `Sketch` > `Include Library` > `Add .ZIP Library...`，选择下载的 `.zip` 文件并点击 `Open` 按钮进行安装。

为了获取更加详细的库安装指南，请查阅 [Arduino IDE v1.x.x](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries) 或者 [Arduino IDE v2.x.x](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-installing-a-library) 文档。

## 常见问题及解答

### Arduino 库的目录在哪儿？

您可以在 Arduino IDE 的菜单栏中选择 `File` > `Preferences` > `Settings` > `Sketchbook location` 来查找和修改 Arduino 库的目录路径。

### arduino-eps32 的安装目录以及 SDK 的目录在哪儿？

arduino-esp32 的默认安装路径取决于您的操作系统：

- Windows： `C:\Users\<user name>\AppData\Local\Arduino15\packages\esp32`
- Linux： `~/.arduino15/packages/esp32`
- macOS： `~/Library/Arduino15/packages/esp32`

arduino-esp32 v3.x 版本的 SDK 位于默认安装路径下的 `tools > esp32-arduino-libs > idf-release_x` 目录中。

### 如何在 Arduino IDE 中安装 esp-boost？

请参阅 [安装库](#安装库) 。

### 在 Arduino IDE 中打开串口调试器看不到日志信息或日志信息显示不全，如何解决？

请参考以下步骤解决：

1. 检查 Arduino IDE 中 `Tools` > `Port` 是否设置正确
2. 检查 Arduino IDE 中 `Tools` > `Core Debug Level` 是否设置为期望等级，如 `Info` 或更低级别
3. 检查 Arduino IDE 中 `Tools` > `USB CDC On Boot` 是否设置正确，如 ESP32 使用 `UART` 端口连接则设置为 `Disabled`，使用 `USB` 端口连接则设置为 `Enabled`。修改设置后需要使能 `Erase All Flash Before Sketch Upload` 选项后重新烧录。
4. 检查 Arduino IDE 中串口调试器波特率是否设置正确，如 `115200`
