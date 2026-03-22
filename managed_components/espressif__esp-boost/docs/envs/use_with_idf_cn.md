# 在 ESP-IDF 下使用

* [English Version](./use_with_idf.md)

## 目录

- [在 ESP-IDF 下使用](#在-esp-idf-下使用)
  - [目录](#目录)
  - [SDK 及依赖组件](#sdk-及依赖组件)
  - [添加到工程](#添加到工程)

## SDK 及依赖组件

在使用本库之前，请确保已安装符合以下版本要求的 SDK：

|                     **SDK**                     | **版本要求** |
| ----------------------------------------------- | ------------ |
| [esp-idf](https://github.com/espressif/esp-idf) | >= 5.1       |

> [!NOTE]
> * SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## 添加到工程

esp-boost 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

    在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/esp-boost==0.1.*"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/esp-boost: "0.1.*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
