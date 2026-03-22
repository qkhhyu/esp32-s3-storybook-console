# ESP-GMF-Audio

- [English](./README.md)

ESP GMF Audio 是 GMF 音频处理相关元素的集合，包括音频编码，解码和音频效果处理算法。目前已支持的 Audio 模块参见下表。

| 名称 | 功能 |  函数方法   | 输入端口 | 输出端口 |  输入阻塞时间 | 输出阻塞时间 |依赖音频信息 |
|:----:| :-----: | :----: | :----: |:----: |:----: |:----: |:---- |
|  AUDIO_DEC |音频解码：MP3,AAC,AMRNB,<br>AMRWB,FLAC,WAV,M4A,TS  | 无 |  单个 |  单个  | 可用户配置，默认是最大延迟 |可用户配置，默认是最大延迟 |否 |
|  AUDIO_ENC |音频编码：AAC,AMRNB,AMRWB,<br>ADPCM,OPUS,PCM  | 无 |  单个 |  单个  | 可用户配置，默认是最大延迟 |可用户配置，默认是最大延迟 |是 |
|  RATE_CVT|音频采样率调节  | `set_dest_rate` |  单个 |  单个  |最大延迟 |最大延迟| 是 |
|  BIT_CVT |音频比特位转换  | `set_dest_bits`| 单个 |  单个  |最大延迟 |最大延迟| 是 |
|  CH_CVT  |音频声道数转换   | `set_dest_ch`|  单个 |  单个  |最大延迟 |最大延迟| 是 |
|  ALC     |音频音量调节    | `set_gain`<br>`get_gain`| 单个 |  单个  |最大延迟 |最大延迟| 是 |
|  EQ      |音频均衡器调节  |`set_para`<br>`get_para`<br>`enable_filter`<br>`disable_filter`  |单个 |单个|最大延迟 |最大延迟|是 |
|  FADE    |音频淡入淡出效果    |`set_mode`<br>`get_mode`<br>`reset_weight` | 单个 |  单个  |最大延迟 |最大延迟 |是 |
|  SONIC   |音频变速变调效果    |`set_speed`<br>`get_speed`<br>`set_pitch`<br>`get_pitch`| 单个 | 单个 |最大延迟 |最大延迟| 是 |
|  MIXER   |音频混音效果  |`set_info`<br>`set_mode`|  多个 |  单个  | 第一路阻塞时间为0，其他路阻塞时间为最大延迟 |最大延迟| 否 |
|INTERLEAVE|数据交织    | 无 | 多个 |  单个  | 可用户配置，默认是最大延迟 |最大延迟| 是 |
|DEINTERLEAVE|数据解交织 | 无| 单个 |  多个  |最大延迟|可用户配置，默认是最大延迟 |是 |

## 示例
ESP GMF Audio 常常组合成管道使用，示例代码请参考 [test_app](../test_apps/main/elements/gmf_audio_play_el_test.c)。
