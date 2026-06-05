#include "balance_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "car_controller.h"
#include "esp_log.h"

static const char* TAG = "BalanceTask";

// 这是任务的实际死循环主体（不对外暴露，所以加了 static）
static void MotorTestTask(void* pvParameters) {
    // 1. 初始化小车管家（它会自动拉起 MotorDriver 底层）
    CarController::GetInstance().Initialize();

    ESP_LOGW(TAG, "🚨 警告：2秒后开始电机测试，请确保车轮悬空！");
    vTaskDelay(pdMS_TO_TICKS(2000));

    //while (true) {  // 一直循环
    //if (true) {     // 仅执行一次
    for (int i = 0; i < 5; ++i) { // 循环i次，测试完就结束
        ESP_LOGI(TAG, ">>> 🟢 测试项 1：前进 (动力 50%%)");
        CarController::GetInstance().MoveForward(50);
        vTaskDelay(pdMS_TO_TICKS(2000)); // 跑 2 秒

        ESP_LOGI(TAG, ">>> 🛑 刹车");
        CarController::GetInstance().Stop();
        vTaskDelay(pdMS_TO_TICKS(1000)); // 停 1 秒

        ESP_LOGI(TAG, ">>> 🔵 测试项 2：后退 (动力 50%%)");
        CarController::GetInstance().MoveBackward(50);
        vTaskDelay(pdMS_TO_TICKS(2000)); // 跑 2 秒

        ESP_LOGI(TAG, ">>> 🛑 刹车");
        CarController::GetInstance().Stop();
        vTaskDelay(pdMS_TO_TICKS(1000)); // 停 1 秒

        ESP_LOGI(TAG, ">>> 🟡 测试项 3：原地左转 (动力 50%%)");
        CarController::GetInstance().TurnLeft(50);
        vTaskDelay(pdMS_TO_TICKS(2000)); // 转 2 秒

        ESP_LOGI(TAG, ">>> 🛑 刹车");
        CarController::GetInstance().Stop();
        vTaskDelay(pdMS_TO_TICKS(1000)); // 停 1 秒

        ESP_LOGI(TAG, ">>> 🟣 测试项 4：原地右转 (动力 50%%)");
        CarController::GetInstance().TurnRight(50);
        vTaskDelay(pdMS_TO_TICKS(2000)); // 转 2 秒

        ESP_LOGI(TAG, ">>> 🛑 刹车并休息 5 秒...");
        CarController::GetInstance().Stop();
        vTaskDelay(pdMS_TO_TICKS(5000));

    }
}

// 给外部调用的启动函数
void StartBalanceTask() {
    ESP_LOGI(TAG, "Starting Balance/Test Task...");
    // 启动独立线程，分配 4096 字节栈空间，优先级设为 5
    xTaskCreate(MotorTestTask, "BalanceTask", 4096, nullptr, 5, nullptr);
}