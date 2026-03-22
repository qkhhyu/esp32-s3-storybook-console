# ESP-GMF-Audio

- [中文版](./README_CN.md)

ESP GMF Audio is a collection of GMF elements related to audio processing, including audio encoding, decoding, and audio effect processing algorithms. The currently supported audio modules are listed in the table below.

|Name|Function|Method|Input Port|Output Port|Input blocking time|Output blocking time|Dependent on Audio Information|
|:----:|:-----:|:----:|:----:|:----:|:----:|:----:|:----|
|  AUDIO_DEC |Audio decoder: MP3,AAC,AMRNB,<br>AMRWB,FLAC,WAV,M4A,TS|Nil|Single|Single|User configurable, default value is maximum delay|User configurable, default value is maximum delay|No|
|  AUDIO_ENC |Audio encoder: AAC,AMRNB,AMRWB,<br>ADPCM,OPUS,PCM|Nil|Single|Single|User configurable, default value is maximum delay|User configurable, default value is maximum delay|Yes|
|  RATE_CVT|Audio sampling rate adjustment|`set_dest_rate`|Single|Single|Maximum delay|Maximum delay|Yes|
|  BIT_CVT |Audio bit-depth conversion|`set_dest_bits`|Single|Single|Maximum delay|Maximum delay|Yes|
|  CH_CVT  |Audio channel conversion|`set_dest_ch`|Single|Single|Maximum delay|Maximum delay|Yes|
|  ALC     |Audio volume adjustment|`set_gain`<br>`get_gain`|Single|Single|Maximum delay|Maximum delay|Yes|
|  EQ      |Audio equalizer adjustment|`set_para`<br>`get_para`<br>`enable_filter`<br>`disable_filter`|Single|Single|Maximum delay|Maximum delay|Yes|
|  FADE    |Audio fade-in and fade-out effects|`set_mode`<br>`get_mode`<br>`reset_weight`|Single|Single|Maximum delay|Maximum delay|Yes|
|  SONIC   |Audio pitch and speed shifting effects|`set_speed`<br>`get_speed`<br>`set_pitch`<br>`get_pitch`|Single|Single|Maximum delay|Maximum delay|Yes|
|  MIXER   |Audio mixing effects|`set_info`<br>`set_mode`|Multiple|Single|The blocking time for the first channel is 0, while the blocking time for other channels is maximum delay|Maximum delay|No|
|INTERLEAVE|Data interleaving|Nil|Multiple|Single|User configurable, default value is maximum delay|Maximum delay|Yes|
|DEINTERLEAVE|Data de-interleaving|Nil|Single|Multiple|Maximum delay|User configurable, default value is maximum delay|Yes|

## Usage
The ESP GMF Audio is often used in combination to form a pipeline. For example code, please refer to [test_app](../test_apps/main/elements/gmf_audio_play_el_test.c)。
