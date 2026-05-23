#include "motor_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "MotorDriver";

MotorDriver::MotorDriver() : left_comparator_(nullptr), right_comparator_(nullptr) {}

void MotorDriver::Initialize() {
    ESP_LOGI(TAG, "Initializing MCPWM motor driver...");

    // 1. 严谨初始化方向控制引脚 (GPIO)
    gpio_config_t io_conf = {}; // 默认清零结构体，最安全的写法
    io_conf.pin_bit_mask = (1ULL << GPIO_LEFT_DIR) | (1ULL << GPIO_RIGHT_DIR);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    // 2. 初始化 MCPWM 定时器（配置频率为 20kHz，人耳听不见，电机无噪音）
    mcpwm_timer_handle_t timer = nullptr;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1MHz 分辨率 (1 tick = 1us)
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = 50,       // 1000000 / 50 = 20kHz 的 PWM 周期
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // 3. 初始化操作器 (Operator)
    mcpwm_oper_handle_t oper = nullptr;
    mcpwm_operator_config_t operator_config = { .group_id = 0 };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // 4. 初始化发生器 (Generator) 绑定到 PWM 物理引脚
    mcpwm_gen_handle_t left_gen = nullptr;
    mcpwm_generator_config_t gen_config_left = { .gen_gpio_num = GPIO_LEFT_PWM };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_config_left, &left_gen));

    mcpwm_gen_handle_t right_gen = nullptr;
    mcpwm_generator_config_t gen_config_right = { .gen_gpio_num = GPIO_RIGHT_PWM };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_config_right, &right_gen));

    // 5. 初始化比较器 (Comparator) 控制占空比
    mcpwm_comparator_config_t compare_config = { .flags = { .update_cmp_on_tez = true } };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &compare_config, &left_comparator_));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &compare_config, &right_comparator_));

    // 6. 核心动作逻辑：定时器开始时拉高，匹配到阈值时拉低（生成标准 PWM 波）
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
        left_gen, 
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)
    ));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
        left_gen, 
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, left_comparator_, MCPWM_GEN_ACTION_LOW)
    ));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
        right_gen, 
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)
    ));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
        right_gen, 
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, right_comparator_, MCPWM_GEN_ACTION_LOW)
    ));

    // 7. 启动整个电机外设
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    ESP_LOGI(TAG, "MCPWM motor driver initialized perfectly!");
}

void MotorDriver::SetSpeed(int left_speed, int right_speed) {
    // 强制把越界的值压回 -100 到 100 之间
    left_speed = std::clamp(left_speed, -100, 100);
    right_speed = std::clamp(right_speed, -100, 100);

    // 1. 驱动左轮
    if (left_speed == 0) {
        gpio_set_level((gpio_num_t)GPIO_LEFT_DIR, 0); // 刹车防抖
        mcpwm_comparator_set_compare_value(left_comparator_, 0);
    } else if (left_speed > 0) {
        gpio_set_level((gpio_num_t)GPIO_LEFT_DIR, 1); // 正转
        mcpwm_comparator_set_compare_value(left_comparator_, left_speed * 50 / 100);
    } else {
        gpio_set_level((gpio_num_t)GPIO_LEFT_DIR, 0); // 反转
        mcpwm_comparator_set_compare_value(left_comparator_, (-left_speed) * 50 / 100);
    }

    // 2. 驱动右轮
    if (right_speed == 0) {
        gpio_set_level((gpio_num_t)GPIO_RIGHT_DIR, 0); // 刹车防抖
        mcpwm_comparator_set_compare_value(right_comparator_, 0);
    } else if (right_speed > 0) {
        gpio_set_level((gpio_num_t)GPIO_RIGHT_DIR, 1); // 正转
        mcpwm_comparator_set_compare_value(right_comparator_, right_speed * 50 / 100);
    } else {
        gpio_set_level((gpio_num_t)GPIO_RIGHT_DIR, 0); // 反转
        mcpwm_comparator_set_compare_value(right_comparator_, (-right_speed) * 50 / 100);
    }
}