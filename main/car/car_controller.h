#ifndef CAR_CONTROLLER_H
#define CAR_CONTROLLER_H

class CarController {
private:
    // 私有构造函数（单例模式）
    CarController();

public:
    // 获取全局唯一的单例句柄
    static CarController& GetInstance() {
        static CarController instance;
        return instance;
    }

    // 禁用拷贝和赋值
    CarController(const CarController&) = delete;
    CarController& operator=(const CarController&) = delete;

    // 初始化小车控制器
    void Initialize();

    // 基础运动控制（参数 speed 范围推荐为 0 到 100）
    void MoveForward(int speed);
    void MoveBackward(int speed);
    void TurnLeft(int speed);  // 原地左转
    void TurnRight(int speed); // 原地右转
    void Stop();               // 紧急刹车
};

#endif // CAR_CONTROLLER_H