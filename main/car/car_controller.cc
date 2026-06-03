#include "car_controller.h"
#include "motor_driver.h"
#include "esp_log.h"

static const char* TAG = "CarController";

CarController::CarController() {}

void CarController::Initialize() {
    ESP_LOGI(TAG, "Initializing Car Controller logic layer...");
    
    // 通过NoMirror参数设置两轮是否镜像，carcontroller.h文件中定义
    // 镜像的话后续右轮信号取反，否则两轮信号相同
    NoMirror = -1;

    // 前后反向参数，默认没装反
    // 如果电机装的与预期相反了，改成-1即可，无需调整走线
    NoReverse = 1;

    // 确保底层硬件驱动被优先初始化
    MotorDriver::GetInstance().Initialize();

    // 初始化完成后，强制刹车，防止开机电机乱窜
    Stop();
    
    ESP_LOGI(TAG, "Car Controller initialized perfectly.");
}

void CarController::MoveForward(int speed) {
    ESP_LOGI(TAG, "Command: Move Forward (Speed: %d)", speed);
    // 前进：左右轮同速正转
    // 如果NoMirror为-1，代表电机是镜像安装的，两轮信号相反
    MotorDriver::GetInstance().SetSpeed(NoReverse * speed, NoReverse * NoMirror * speed);
}

void CarController::MoveBackward(int speed) {
    ESP_LOGI(TAG, "Command: Move Backward (Speed: %d)", speed);
    // 后退：左右轮同速反转
    // 如果NoMirror为-1，代表电机是镜像安装的，两轮信号相反
    MotorDriver::GetInstance().SetSpeed(NoReverse * -speed, NoReverse * NoMirror * -speed);
}

void CarController::TurnLeft(int speed) {
    ESP_LOGI(TAG, "Command: Turn Left (Speed: %d)", speed);
    // 左转（坦克掉头模式）：左轮反转，右轮正转
    // 如果NoMirror为-1，代表电机是镜像安装的，两轮信号相反
    MotorDriver::GetInstance().SetSpeed(NoReverse * -speed, NoReverse * NoMirror * speed);
}

void CarController::TurnRight(int speed) {
    ESP_LOGI(TAG, "Command: Turn Right (Speed: %d)", speed);
    // 右转（坦克掉头模式）：左轮正转，右轮反转
    // 如果NoMirror为-1，代表电机是镜像安装的，两轮信号相反
    MotorDriver::GetInstance().SetSpeed(NoReverse * speed, NoReverse * NoMirror * -speed);
}

void CarController::Stop() {
    ESP_LOGI(TAG, "Command: STOP");
    // 停车：左右轮速度强制归零
    MotorDriver::GetInstance().SetSpeed(0, 0);
}