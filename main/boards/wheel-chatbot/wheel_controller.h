#ifndef __WHEEL_CONTROLLER_H__
#define __WHEEL_CONTROLLER_H__

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "config.h"

class WheelController {
public:
    WheelController();
    ~WheelController();

    // Drive both motors. speed: -100 (full back) to +100 (full forward).
    void SetMotors(int left_speed, int right_speed);

    // Convenience wrappers — block until duration_ms elapses, then stop.
    void MoveForward(int speed = 70, int duration_ms = 1000);
    void MoveBackward(int speed = 70, int duration_ms = 1000);
    void TurnLeft(int speed = 60, int duration_ms = 600);
    void TurnRight(int speed = 60, int duration_ms = 600);
    void SpinLeft(int speed = 70, int duration_ms = 400);
    void SpinRight(int speed = 70, int duration_ms = 400);
    void Stop();

    // "Lắc lắc vui vẻ" — oscillate left-right, steps times.
    void Dance(int steps = 4, int speed = 65);

    bool IsMoving() const { return is_moving_; }

    void RegisterMcpTools();

private:
    bool is_moving_ = false;
    TaskHandle_t action_task_ = nullptr;
    QueueHandle_t action_queue_;

    struct Action {
        int type;         // see ActionType enum
        int left_speed;
        int right_speed;
        int duration_ms;
        int steps;
        int speed;
    };

    enum ActionType {
        ACT_MOVE = 1,
        ACT_STOP = 2,
        ACT_DANCE = 3,
    };

    void InitLedc();
    void InitGpio();
    void ApplyMotors(int left_speed, int right_speed);
    void EnqueueAction(const Action& a);
    void StartTaskIfNeeded();

    static void ActionTask(void* arg);
};

void InitializeWheelController();

#endif  // __WHEEL_CONTROLLER_H__
