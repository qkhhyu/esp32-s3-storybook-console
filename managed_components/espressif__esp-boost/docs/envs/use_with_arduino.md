# Using with Arduino IDE

* [中文版](./use_with_arduino_cn.md)

## Table of Contents

- [Using with Arduino IDE](#using-with-arduino-ide)
  - [Table of Contents](#table-of-contents)
  - [Quick Start](#quick-start)
    - [Environment Setup](#environment-setup)
  - [SDK \& Dependencies](#sdk--dependencies)
  - [Installing Libraries](#installing-libraries)
  - [FAQ](#faq)
    - [Where is the Arduino library directory?](#where-is-the-arduino-library-directory)
    - [Where are the arduino-esp32 installation directory and SDK directory?](#where-are-the-arduino-esp32-installation-directory-and-sdk-directory)
    - [How to install esp-boost in Arduino IDE?](#how-to-install-esp-boost-in-arduino-ide)
    - [Can't see log messages or messages are incomplete in Arduino IDE's Serial Monitor, how to fix?](#cant-see-log-messages-or-messages-are-incomplete-in-arduino-ides-serial-monitor-how-to-fix)

## Quick Start

### Environment Setup

1. **Install Arduino IDE**

- Download and install Arduino IDE from [Arduino's official website](https://www.arduino.cc/en/software), version 2.x is recommended

2. **Install ESP32 SDK**

- Open Arduino IDE
- Navigate to `File` > `Preferences`
- Add to `Additional boards manager URLs`:

  ```
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
  ```

- Navigate to `Tools` > `Board` > `Boards Manager`
- Search for `esp32` by `Espressif Systems` and install the required version (see [SDK & Dependencies](#sdk--dependencies))

> [!NOTE]
> If you encounter any issues, please check the [FAQ](#faq) section first. If the problem persists, you can submit an issue on [GitHub](https://github.com/espressif/esp-boost/issues).

## SDK & Dependencies

Before using this library, ensure you have installed SDK and dependencies that meet the following version requirements:

|                           **SDK**                           | **Required Version** |
| ----------------------------------------------------------- | -------------------- |
| [arduino-esp32](https://github.com/espressif/arduino-esp32) | >= 3.1.0             |

> [!NOTE]
> * For SDK installation, please refer to [Arduino ESP32 Documentation - Installing](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).

## Installing Libraries

esp-boost and its dependencies have been uploaded to the Arduino Library Manager. You can install them directly online following these steps:

1. In Arduino IDE, navigate to `Sketch` > `Include Library` > `Manage Libraries...`.
2. Search for `esp-boost` and its dependencies, click the `Install` button to install.

For manual installation, you can download the required version's `.zip` file from [Github](https://github.com/espressif/esp-boost) or [Arduino Library](https://www.arduinolibraries.info/libraries/esp-boost), then in Arduino IDE navigate to `Sketch` > `Include Library` > `Add .ZIP Library...`, select the downloaded `.zip` file and click `Open` to install.

For more detailed library installation guides, please refer to [Arduino IDE v1.x.x](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries) or [Arduino IDE v2.x.x](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-installing-a-library) documentation.

## FAQ

### Where is the Arduino library directory?

You can find and modify the Arduino library directory path in Arduino IDE by selecting `File` > `Preferences` > `Settings` > `Sketchbook location`.

### Where are the arduino-esp32 installation directory and SDK directory?

The default installation path for arduino-esp32 depends on your operating system:

- Windows: `C:\You\<user name>\AppData\Local\Arduino15\packages\esp32`
- Linux: `~/.arduino15/packages/esp32`
- macOS: `~/Library/Arduino15/packages/esp32`

For arduino-esp32 v3.x version, the SDK is located in the `tools > esp32-arduino-libs > idf-release_x` directory under the default installation path.

### How to install esp-boost in Arduino IDE?

Please refer to [Installing Libraries](#installing-libraries).

### Can't see log messages or messages are incomplete in Arduino IDE's Serial Monitor, how to fix?

Please follow these steps to resolve:

1. Check if `Tools` > `Port` is set correctly in Arduino IDE
2. Check if `Tools` > `Core Debug Level` is set to the desired level, such as `Info` or lower
3. Check if `Tools` > `USB CDC On Boot` is set correctly in Arduino IDE - set to `Disabled` when ESP32 is connected via `UART` port, or `Enabled` when connected via `USB` port. After changing this setting, enable the `Erase All Flash Before Sketch Upload` option and reflash.
4. Check if the baud rate in Arduino IDE's Serial Monitor is set correctly, such as `115200`
