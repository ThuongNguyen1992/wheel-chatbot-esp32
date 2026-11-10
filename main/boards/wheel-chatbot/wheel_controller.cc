#include "wheel_controller.h"

#include <cstring>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board.h"
#include "mcp_server.h"
#include "wifi_manager.h"

#define TAG "WheelController"

// ─── Init ────────────────────────────────────────────────────────────────────

void WheelController::InitLedc() {
    ledc_timer_config_t timer = {
        .speed_mode = MOTOR_LEDC_SPEED_MODE,
        .duty_resolution = MOTOR_LEDC_RESOLUTION,
        .timer_num = MOTOR_LEDC_TIMER,
        .freq_hz = MOTOR_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch_l = {
        .gpio_num = MOTOR_L_ENA,
        .speed_mode = MOTOR_LEDC_SPEED_MODE,
        .channel = MOTOR_L_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = MOTOR_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_l));

    ledc_channel_config_t ch_r = {
        .gpio_num = MOTOR_R_ENB,
        .speed_mode = MOTOR_LEDC_SPEED_MODE,
        .channel = MOTOR_R_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = MOTOR_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_r));
}

void WheelController::InitGpio() {
    const gpio_num_t dir_pins[] = {MOTOR_L_IN1, MOTOR_L_IN2, MOTOR_R_IN3, MOTOR_R_IN4};
    for (gpio_num_t pin : dir_pins) {
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&cfg));
        gpio_set_level(pin, 0);
    }
}

WheelController::WheelController() {
    action_queue_ = xQueueCreate(8, sizeof(Action));
    InitLedc();
    InitGpio();
    Stop();
    RegisterMcpTools();
    ESP_LOGI(TAG, "WheelController initialized");
}

WheelController::~WheelController() {
    if (action_task_ != nullptr) {
        vTaskDelete(action_task_);
        action_task_ = nullptr;
    }
    vQueueDelete(action_queue_);
    Stop();
}

// ─── Motor control ───────────────────────────────────────────────────────────

void WheelController::ApplyMotors(int left_speed, int right_speed) {
    // Clamp to [-100, 100]
    if (left_speed > 100) left_speed = 100;
    if (left_speed < -100) left_speed = -100;
    if (right_speed > 100) right_speed = 100;
    if (right_speed < -100) right_speed = -100;

    // Left motor direction
    if (left_speed >= 0) {
        gpio_set_level(MOTOR_L_IN1, 1);
        gpio_set_level(MOTOR_L_IN2, 0);
    } else {
        gpio_set_level(MOTOR_L_IN1, 0);
        gpio_set_level(MOTOR_L_IN2, 1);
        left_speed = -left_speed;
    }

    // Right motor direction
    if (right_speed >= 0) {
        gpio_set_level(MOTOR_R_IN3, 1);
        gpio_set_level(MOTOR_R_IN4, 0);
    } else {
        gpio_set_level(MOTOR_R_IN3, 0);
        gpio_set_level(MOTOR_R_IN4, 1);
        right_speed = -right_speed;
    }

    // PWM duty: scale 0-100 → 0-255
    uint32_t duty_l = (uint32_t)left_speed * 255 / 100;
    uint32_t duty_r = (uint32_t)right_speed * 255 / 100;

    ledc_set_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_L_LEDC_CHANNEL, duty_l);
    ledc_update_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_L_LEDC_CHANNEL);
    ledc_set_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_R_LEDC_CHANNEL, duty_r);
    ledc_update_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_R_LEDC_CHANNEL);
}

void WheelController::SetMotors(int left_speed, int right_speed) {
    is_moving_ = (left_speed != 0 || right_speed != 0);
    ApplyMotors(left_speed, right_speed);
}

void WheelController::Stop() {
    ApplyMotors(0, 0);
    // Both direction pins low = brake (L298N active brake)
    gpio_set_level(MOTOR_L_IN1, 0);
    gpio_set_level(MOTOR_L_IN2, 0);
    gpio_set_level(MOTOR_R_IN3, 0);
    gpio_set_level(MOTOR_R_IN4, 0);
    is_moving_ = false;
}

// ─── Blocking moves (called from action task) ─────────────────────────────────

void WheelController::MoveForward(int speed, int duration_ms) {
    is_moving_ = true;
    ApplyMotors(speed, speed);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    Stop();
}

void WheelController::MoveBackward(int speed, int duration_ms) {
    is_moving_ = true;
    ApplyMotors(-speed, -speed);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    Stop();
}

void WheelController::TurnLeft(int speed, int duration_ms) {
    is_moving_ = true;
    ApplyMotors(speed / 2, speed);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    Stop();
}

void WheelController::TurnRight(int speed, int duration_ms) {
    is_moving_ = true;
    ApplyMotors(speed, speed / 2);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    Stop();
}

void WheelController::SpinLeft(int speed, int duration_ms) {
    is_moving_ = true;
    ApplyMotors(-speed, speed);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    Stop();
}

void WheelController::SpinRight(int speed, int duration_ms) {
    is_moving_ = true;
    ApplyMotors(speed, -speed);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    Stop();
}

void WheelController::Dance(int steps, int speed) {
    is_moving_ = true;
    for (int i = 0; i < steps; i++) {
        ApplyMotors(-speed, speed);
        vTaskDelay(pdMS_TO_TICKS(200));
        ApplyMotors(speed, -speed);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    Stop();
}

// ─── Action queue ─────────────────────────────────────────────────────────────

void WheelController::EnqueueAction(const Action& a) {
    xQueueSend(action_queue_, &a, portMAX_DELAY);
    StartTaskIfNeeded();
}

void WheelController::StartTaskIfNeeded() {
    if (action_task_ == nullptr) {
        xTaskCreate(ActionTask, "wheel_action", 2048, this, configMAX_PRIORITIES - 2,
                    &action_task_);
    }
}

void WheelController::ActionTask(void* arg) {
    WheelController* ctrl = static_cast<WheelController*>(arg);
    Action a;
    while (true) {
        if (xQueueReceive(ctrl->action_queue_, &a, pdMS_TO_TICKS(5000)) == pdTRUE) {
            switch (a.type) {
                case ACT_MOVE:
                    ctrl->is_moving_ = true;
                    ctrl->ApplyMotors(a.left_speed, a.right_speed);
                    if (a.duration_ms > 0) {
                        vTaskDelay(pdMS_TO_TICKS(a.duration_ms));
                        if (uxQueueMessagesWaiting(ctrl->action_queue_) == 0) {
                            ctrl->Stop();
                        }
                    }
                    break;
                case ACT_STOP:
                    ctrl->Stop();
                    break;
                case ACT_DANCE:
                    ctrl->Dance(a.steps, a.speed);
                    break;
            }
        }
    }
}

// ─── MCP Tools ───────────────────────────────────────────────────────────────

void WheelController::RegisterMcpTools() {
    auto& mcp = McpServer::GetInstance();

    mcp.AddTool(
        "self.wheel.move",
        "Điều khiển robot di chuyển. direction: forward/backward/left/right/spin_left/spin_right. "
        "speed: 0–100 (tốc độ, mặc định 70). duration_ms: thời gian chạy tính bằng ms (mặc định "
        "1000, 0 = chạy liên tục cho đến khi stop). "
        "Ví dụ: đi thẳng 2 giây → direction=forward, speed=70, duration_ms=2000.",
        PropertyList({
            Property("direction", kPropertyTypeString, "forward"),
            Property("speed", kPropertyTypeInteger, 70, 0, 100),
            Property("duration_ms", kPropertyTypeInteger, 1000, 0, 10000),
        }),
        [this](const PropertyList& props) -> ReturnValue {
            std::string dir = props["direction"].value<std::string>();
            int speed = props["speed"].value<int>();
            int dur = props["duration_ms"].value<int>();

            int l = 0, r = 0;
            if (dir == "forward") {
                l = speed;
                r = speed;
            } else if (dir == "backward") {
                l = -speed;
                r = -speed;
            } else if (dir == "left") {
                l = speed / 2;
                r = speed;
            } else if (dir == "right") {
                l = speed;
                r = speed / 2;
            } else if (dir == "spin_left") {
                l = -speed;
                r = speed;
            } else if (dir == "spin_right") {
                l = speed;
                r = -speed;
            } else {
                return "Lỗi: direction không hợp lệ. Dùng: forward, backward, left, right, "
                       "spin_left, spin_right";
            }

            Action a = {ACT_MOVE, l, r, dur, 0, speed};
            EnqueueAction(a);
            return true;
        });

    mcp.AddTool("self.wheel.stop", "Dừng ngay lập tức tất cả động cơ.", PropertyList(),
                [this](const PropertyList&) -> ReturnValue {
                    Action a = {ACT_STOP, 0, 0, 0, 0, 0};
                    EnqueueAction(a);
                    return true;
                });

    mcp.AddTool(
        "self.wheel.dance",
        "Lắc lắc vui vẻ — robot lắc trái phải để biểu đạt cảm xúc vui mừng. "
        "steps: số lần lắc (mặc định 4). speed: tốc độ lắc 0–100 (mặc định 65).",
        PropertyList({
            Property("steps", kPropertyTypeInteger, 4, 1, 20),
            Property("speed", kPropertyTypeInteger, 65, 10, 100),
        }),
        [this](const PropertyList& props) -> ReturnValue {
            int steps = props["steps"].value<int>();
            int speed = props["speed"].value<int>();
            Action a = {ACT_DANCE, 0, 0, 0, steps, speed};
            EnqueueAction(a);
            return true;
        });

    mcp.AddTool("self.wheel.get_status",
                "Trả về trạng thái hiện tại của robot: moving hoặc idle.", PropertyList(),
                [this](const PropertyList&) -> ReturnValue {
                    return is_moving_ ? "moving" : "idle";
                });

    mcp.AddTool("self.wheel.get_ip", "Trả về địa chỉ IP WiFi của robot.", PropertyList(),
                [](const PropertyList&) -> ReturnValue {
                    auto& wifi = WifiManager::GetInstance();
                    std::string ip = wifi.GetIpAddress();
                    if (ip.empty()) {
                        return std::string("{\"ip\":\"\",\"connected\":false}");
                    }
                    return std::string("{\"ip\":\"") + ip + "\",\"connected\":true}";
                });

    ESP_LOGI(TAG, "MCP tools registered");
}

// ─── Global init ─────────────────────────────────────────────────────────────

static WheelController* g_wheel_controller = nullptr;

void InitializeWheelController() {
    if (g_wheel_controller == nullptr) {
        g_wheel_controller = new WheelController();
    }
}
