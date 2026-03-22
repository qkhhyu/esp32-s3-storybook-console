# Using with ESP-IDF

* [中文版](./use_with_idf_cn.md)

## Table of Contents

- [Using with ESP-IDF](#using-with-esp-idf)
  - [Table of Contents](#table-of-contents)
  - [SDK \& Dependencies](#sdk--dependencies)
  - [Adding to Project](#adding-to-project)

## SDK & Dependencies

Before using this library, please ensure you have installed the SDK that meets the following version requirements:

|                     **SDK**                     | **Version Required** |
| ----------------------------------------------- | ------------------- |
| [esp-idf](https://github.com/espressif/esp-idf) | >= 5.1              |

> [!NOTE]
> * For SDK installation, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## Adding to Project

esp-boost has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

    Run the following command in your project directory:

    ```bash
    idf.py add-dependency "espressif/esp-boost==0.1.*"
    ```

2. **Modifying Configuration File**

    Create or modify the *idf_component.yml* file in your project directory:

    ```yaml
    dependencies:
      espressif/esp-boost: "0.1.*"
    ```

For detailed information, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
