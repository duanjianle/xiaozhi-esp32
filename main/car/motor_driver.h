#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include "driver/mcpwm_prelude.h" // 乐鑫官方最新高精度电机专用 MCPWM 驱动

class MotorDriver {
private:
    // 硬件引脚定义（需要根据您大板的实际走线修改这些引脚号）
    static constexpr int GPIO_LEFT_PWM  = 9;
    static constexpr int GPIO_LEFT_DIR  = 10;
    static constexpr int GPIO_RIGHT_PWM = 11;
    static constexpr int GPIO_RIGHT_DIR = 12;

    // MCPWM 资源句柄
    mcpwm_cmpr_handle_t left_comparator_;
    mcpwm_cmpr_handle_t right_comparator_;

    // 私有构造函数（单例模式，防外部乱new）
    MotorDriver();

public:
    // 获取单例句柄
    static MotorDriver& GetInstance() {
        static MotorDriver instance;
        return instance;
    }

    // 禁止拷贝
    MotorDriver(const MotorDriver&) = delete;
    MotorDriver& operator=(const MotorDriver&) = delete;

    // 核心接口：配置底层硬件引脚和频率
    void Initialize();

    // 核心接口：控制左右轮转速，范围 -100 (全速倒车) 到 100 (全速前进)
    void SetSpeed(int left_speed, int right_speed);
};

#endif // MOTOR_DRIVER_H