#include "wifi_board.h"

#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "power_save_timer.h"
#include "../ai-wali-0.96oled-s3/power_manager.h"

#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include "i2c_device.h"
#include <esp_io_expander_tca9554.h>

#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include "mcp_server.h"
#include "car/car_controller.h"


#if CONFIG_USE_DEVICE_AEC
#include "codecs/box_audio_codec.h"
#include "codecs/dummy_audio_codec.h"
#include "codecs/es8311_audio_codec.h"

#else
#include "codecs/no_audio_codec.h"
#endif

#define TAG "AI_WALI_0_96OLED_S3"

// 编译期字符串哈希函数（可以让程序实现switch- case，识别 字符串情况，hash将字符串转为int）
constexpr unsigned int string_hash(const char* str, int h = 0) {
    return !str[h] ? 5381 : (string_hash(str, h + 1) * 33) ^ str[h];
}

 

class AI_WALI_0_96OLED_S3 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    Display* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    void InitializePowerManager() {
        power_manager_ = new PowerManager(GPIO_NUM_38);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        rtc_gpio_init(GPIO_NUM_21);
        rtc_gpio_set_direction(GPIO_NUM_21, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level(GPIO_NUM_21, 1);

        // 根据当前电池电量设置关机时间，电量越低关机时间越短
        uint8_t current_battery = power_manager_->GetBatteryLevel(); 
        int seconds_to_shutdown;
        if (current_battery >= 100) { // 如果电池是满电，关机时间设置成20分钟
            seconds_to_shutdown = 20 * 60;
        } else if (current_battery >= 86) { // 如果电池电量剩余较多，关机时间设置成10分钟
            seconds_to_shutdown = 10 * 60;
        } else { // 如果电池电量剩余较少，关机时间设置成5分钟
            seconds_to_shutdown = 5 * 60;
        }

        power_save_timer_ = new PowerSaveTimer(-1, 60, seconds_to_shutdown);  // (int cpu_max_freq, int seconds_to_sleep, int seconds_to_shutdown)
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            rtc_gpio_set_level(GPIO_NUM_21, 0);
            // 启用保持功能，确保睡眠期间电平不变
            rtc_gpio_hold_en(GPIO_NUM_21);
            esp_lcd_panel_disp_on_off(panel_, false); //关闭显示
            esp_deep_sleep_start();
        });
        power_save_timer_->SetEnabled(true);
    }

    // i2c只需要初始化一次，除非有个多个i2c
    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            // 修改处，用空保护隔离开，防止引发后面的硬件挂死
            panel_ = nullptr;
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
#endif
    }

    void InitializeGpio() {
        ESP_LOGI("I2C", "准备初始化PA_EN_PIN"); 
#ifdef AUDIO_CODEC_PA_PIN
#if AUDIO_CODEC_PA_PIN != -1
        // 设置ES8311
        // 1. 设置引脚方向为“输出”模式
        ESP_LOGI("I2C", "开始初始化PA_EN_PIN"); 

        gpio_reset_pin(AUDIO_CODEC_PA_PIN); // 先复位引脚，清除可能存在的其他外设复用
        gpio_set_direction(AUDIO_CODEC_PA_PIN, GPIO_MODE_OUTPUT);

        // 3. 【必须】将引脚拉高（通常 1 是使能，如果是低电平有效则写 0）
        gpio_set_level(AUDIO_CODEC_PA_PIN, 1); 

        // 4. 【极重要】给硬件芯片一个“睡醒”和电压稳定的缓冲时间
        vTaskDelay(pdMS_TO_TICKS(50));        

        ESP_LOGI("I2C", "初始化PA_EN_PIN完成"); 
#endif
#endif
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }



    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128+16; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200));
                if (ret == ESP_OK) {
                    printf("%02x ", address);
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }
    }


    // 🛠️ 把小车的 AI 控制工具注册进系统
    void InitializeCarTools(){
        // 🔗 注册小车移动工具

        // 初始化小车控制器，确保它在接收 AI 命令前已经准备就绪
        CarController::GetInstance().Initialize();

        // 这其实一个工具函数，传入参数包括：工具名称、工具描述、参数定义（JSON格式字符串）和回调函数（lambda表达式）
        // 执行是通过回调函数实现的，回调函数也会返回字符串结果给大模型，告诉它执行结果是成功还是失败，以及一些提示信息
        McpServer::GetInstance().AddTool(
            // 1. 工具的唯一名称 (大模型认这个)
            "car_move",          
            // 2. 描述：用于让大模型理解什么时候该调用它                         
            "控制智能小车的移动方向，例如前进、后退、停止、向左转、向右转、转圈",      
            // 3. 参数定义 (JSON 属性结构)
            PropertyList({
                Property("direction", kPropertyTypeString, "移动方向，可选值: forward, backward, stop, turn_left, turn_right, turn_around")
            }),          
            // 4. C++ 真实的回调函数逻辑
            [this](const PropertyList& properties) -> ReturnValue {

                // 以下是回调函数，即实际定义

                // 4.1 解析大模型传回的参数（使用标准 JSON 库）
                std::string direction_str = "";
                try {
                    direction_str = properties["direction"].value<std::string>();
                } catch (...) {
                    return "{\"result\": \"failed\", \"message\": \"JSON 参数解析失败\"}";
                }

                // 4.2 执行命令

                // 在回调函数内部解析出 direction_str 后，使用编译期 Hash 闪现匹配动作
                switch (string_hash(direction_str.c_str())) {
                    case string_hash("forward"):
                        CarController::GetInstance().MoveForward(50);
                        ESP_LOGI("CAR", "收到 AI 命令：小车正在前进！");
                        return "{\"result\": \"success\", \"message\": \"小车已开始向前行驶\"}";

                    case string_hash("backward"):
                        CarController::GetInstance().MoveBackward(50);
                        ESP_LOGI("CAR", "收到 AI 命令：小车正在后退！"); // 👈 修正：对齐日志
                        return "{\"result\": \"success\", \"message\": \"小车已开始向后倒车\"}"; 

                    case string_hash("stop"):
                        CarController::GetInstance().Stop();
                        ESP_LOGI("CAR", "收到 AI 命令：小车紧急停止！");
                        return "{\"result\": \"success\", \"message\": \"小车已安全停止\"}"; 

                    case string_hash("turn_left"):
                        CarController::GetInstance().TurnLeft(50);
                        ESP_LOGI("CAR", "收到 AI 命令：小车已左转！");
                        return "{\"result\": \"success\", \"message\": \"小车已左转\"}"; 

                    case string_hash("turn_right"):
                        CarController::GetInstance().TurnRight(50);
                        ESP_LOGI("CAR", "收到 AI 命令：小车已右转！");
                        return "{\"result\": \"success\", \"message\": \"小车已右转\"}"; 

                    case string_hash("turn_around"):
                        CarController::GetInstance().TurnLeft(50*4); // 模拟转圈，实际项目中可以根据需要调整转圈的方式和时间
                        ESP_LOGI("CAR", "收到 AI 命令：小车已转圈！");
                        return "{\"result\": \"success\", \"message\": \"小车已转圈\"}"; 

                    default:
                        // 大模型传错了参数，或者超出了 forward/backward/stop 的范围
                        ESP_LOGW("CAR", "大模型传回了未在列表中定义的错误方向: %s", direction_str.c_str());
                        break;
                }

                // 未知命令，返回失败
                return "{\"result\": \"failed\", \"message\": \"未知的移动方向\"}";
            }
        );
    }

public:
    AI_WALI_0_96OLED_S3() :
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializePowerManager();
        InitializePowerSaveTimer();
        //InitializeGpio();
        InitializeI2c();
        //I2cDetect();  // 这里audio还没初始化，探测不到相应端口
        //InitializeDisplayI2c();
        //InitializeSsd1306Display();
        display_ = new NoDisplay();
        InitializeButtons();
        ESP_LOGI("MAIN", "BOARD初始化完成");

        // 获取并打印内部与外部可用内存
        size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI("MEM", "内部 SRAM 剩余: %u bytes", free_internal);
        ESP_LOGI("MEM", "外部 PSRAM 剩余: %u bytes", free_psram);
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#if CONFIG_USE_DEVICE_AEC
        //InitializeGpio();
        // 1. 检测到音频芯片是否存在，并缓存 AudioCodec 实例指针，避免重复探测和实例化
        static AudioCodec* cached_codec = nullptr;
        if (cached_codec == nullptr) {
            //注意，此处传入的I2C地址，与项目使用的不一致，此处需要用7位，项目已经处理成8位
            bool es8311_found = (i2c_master_probe(i2c_bus_, AUDIO_CODEC_ES8311_ADDR >> 1, 100) == ESP_OK); 
            bool es7210_found = (i2c_master_probe(i2c_bus_, AUDIO_CODEC_ES7210_ADDR>>1, 100) == ESP_OK); 

        // 2. 🧬 动态决策
            if (es8311_found && es7210_found) {
                ESP_LOGI("BOARD", "🎉 [AEC模式] 音频芯片在线。");
                static BoxAudioCodec audio_codec(
                    i2c_bus_, 
                    AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                    AUDIO_I2S_SPK_GPIO_MCLK, AUDIO_I2S_SPK_GPIO_BCLK, 
                    AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_SPK_GPIO_DOUT, 
                    AUDIO_I2S_MIC_GPIO_DIN, AUDIO_CODEC_PA_PIN,
                    AUDIO_CODEC_ES8311_ADDR, 
                    AUDIO_CODEC_ES7210_ADDR,
                    AUDIO_INPUT_REFERENCE); 
                cached_codec = &audio_codec; // 存入缓存
                ESP_LOGI("BOARD", "🎉 [AEC模式] 启动硬件环回 AEC。");
            } 
            else if (es8311_found && !es7210_found) {
                ESP_LOGW("BOARD", "⚠️ [降级模式] 仅发现 ES8311，将降级为单麦克风、无 AEC 模式。");
                static BoxAudioCodec audio_codec(
                    i2c_bus_, 
                    AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                    AUDIO_I2S_SPK_GPIO_MCLK, AUDIO_I2S_SPK_GPIO_BCLK, 
                    AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, 
                    AUDIO_I2S_MIC_GPIO_DIN, AUDIO_CODEC_PA_PIN,
                    AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, 
                    false); // 硬件不全，关闭 AEC 参考通道
                cached_codec = &audio_codec; // 存入缓存
            } 
            else {
                ESP_LOGE("BOARD", "❌ [虚拟模式] 未发现音频芯片！未开启AEC，切换为 Dummy 影子驱动防止卡死。");
                static DummyAudioCodec dummy_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE);
                cached_codec = &dummy_codec; // 存入缓存
            }
        I2cDetect();
        }
        return cached_codec; // 返回缓存的实例
#else
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, 
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
#endif

    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(AI_WALI_0_96OLED_S3);
