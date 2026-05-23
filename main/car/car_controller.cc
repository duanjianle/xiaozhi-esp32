#include "car_controller.h"
#include "motor_driver.h"
#include "esp_log.h"

static const char* TAG = "CarController";

CarController::CarController() {}

void CarController::Initialize() {
    ESP_LOGI(TAG, "Initializing Car Controller logic layer...");
    
    // 确保底层硬件驱动被优先初始化
    MotorDriver::GetInstance().Initialize();
    
    // 初始化完成后，强制刹车，防止开机电机乱窜
    Stop();
    
    ESP_LOGI(TAG, "Car Controller initialized perfectly.");
}

void CarController::MoveForward(int speed) {
    ESP_LOGI(TAG, "Command: Move Forward (Speed: %d)", speed);
    // 前进：左右轮同速正转
    MotorDriver::GetInstance().SetSpeed(speed, speed);
}

void CarController::MoveBackward(int speed) {
    ESP_LOGI(TAG, "Command: Move Backward (Speed: %d)", speed);
    // 后退：左右轮同速反转
    MotorDriver::GetInstance().SetSpeed(-speed, -speed);
}

void CarController::TurnLeft(int speed) {
    ESP_LOGI(TAG, "Command: Turn Left (Speed: %d)", speed);
    // 左转（坦克掉头模式）：左轮反转，右轮正转
    MotorDriver::GetInstance().SetSpeed(-speed, speed);
}

void CarController::TurnRight(int speed) {
    ESP_LOGI(TAG, "Command: Turn Right (Speed: %d)", speed);
    // 右转（坦克掉头模式）：左轮正转，右轮反转
    MotorDriver::GetInstance().SetSpeed(speed, -speed);
}

void CarController::Stop() {
    ESP_LOGI(TAG, "Command: STOP");
    // 停车：左右轮速度强制归零
    MotorDriver::GetInstance().SetSpeed(0, 0);
}