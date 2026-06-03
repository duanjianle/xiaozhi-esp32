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

    // 设置两轮是否镜像，是的话后续右轮信号取反，否则两轮信号相同
    int NoMirror;   // 1 代表不镜像，-1 代表镜像
    // 前后反向，如果电机装的与预期相反了，改成-1即可，无需调整走线
    int reverse;    // 1 代表正常，-1 代表电机装反了

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