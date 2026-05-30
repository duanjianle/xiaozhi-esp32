#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 既然用了ESP32S3-ZERO，默认是用了ES7210+ES8311+NS4150B的音频版，以下基于这个配置
// 注意：音频板的 MCK 悬空不接！音频板的 EN 焊死到 3.3V 常开！
// EN 为 NS4150B的控制线，常开以减少pin，需要连接 3.3V 或是 VCC（注意电压3.3）
// MCK为 master clock，主时钟，可悬空不接
// SCK、BCK、MBK 为bit clock位时钟；
// WS 为I2S 帧时钟 (Word Select / LRCK)，区分左右声道
// DO 为 data out； DI 为 data in
// ES8311 和 ES7210 在很多设计中可以配置为内部倍频模式，不需要外部输入 MCLK。它们可以只通过 BCLK（位时钟）和 WS（帧时钟）自己推算出主时钟。

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4  // 音频板的 WS  (麦克风与喇叭共用帧时钟)
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5  // 音频板的 BCK (麦克风与喇叭共用位时钟)

// 独立音频输入、输出
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6  // 音频板的 DI  (输入：麦克风+AEC回采信号)
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7   // 音频板的 DO  (输出：小智说话放音数据)   // 原MAX98357 的 DIN

// 复用引脚，对于音频版已经集成了的情况下，不用实际连线
#define AUDIO_I2S_SPK_GPIO_BCLK AUDIO_I2S_MIC_GPIO_SCK // 复用引脚 5 将原本独立的 SPEAKER 时钟线重定向到共用时钟上 
#define AUDIO_I2S_SPK_GPIO_LRCK AUDIO_I2S_MIC_GPIO_WS  // 复用引脚 4 将原本独立的 SPEAKER 时钟线重定向到共用时钟上

// 屏幕、音频、陀螺仪，每个pin连3根线
#define DISPLAY_SDA_PIN GPIO_NUM_1     // 屏幕 & 音频 & MPU6050 芯片共用 I2C SDA
#define DISPLAY_SCL_PIN GPIO_NUM_2     // 屏幕 & 音频 & MPU6050 芯片共用 I2C SCL

// 屏幕正常参数
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true

//
// 小车的电机在 car/motor_driver.h 中定义了
//

// 这些是模板，没用到
#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39

// 结尾
#endif // _BOARD_CONFIG_H_
