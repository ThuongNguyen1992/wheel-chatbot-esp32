#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ─── Sample rates ────────────────────────────────────────────────────────────
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// ─── I2S – Microphone (INMP441, Bus 0) ───────────────────────────────────────
// INMP441: WS=LRCLK, SCK=BCK, DIN=DATA
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6

// ─── I2S – Speaker (MAX98357A, Bus 1) ────────────────────────────────────────
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_17

// ─── OLED SSD1306 128×64 (I2C) ───────────────────────────────────────────────
#define DISPLAY_SDA_PIN GPIO_NUM_41
#define DISPLAY_SCL_PIN GPIO_NUM_42
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64

// ─── Motor Driver L298N – Left motor (right-side GPIO, upper bank) ───────────
#define MOTOR_L_ENA GPIO_NUM_19   // PWM speed (LEDC)
#define MOTOR_L_IN1 GPIO_NUM_20   // Direction A
#define MOTOR_L_IN2 GPIO_NUM_21   // Direction B

// ─── Motor Driver L298N – Right motor (right-side GPIO, lower bank) ──────────
#define MOTOR_R_ENB GPIO_NUM_35   // PWM speed (LEDC)
#define MOTOR_R_IN3 GPIO_NUM_36   // Direction A
#define MOTOR_R_IN4 GPIO_NUM_37   // Direction B

// ─── LEDC PWM config for motors ──────────────────────────────────────────────
#define MOTOR_LEDC_SPEED_MODE  LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_TIMER       LEDC_TIMER_1
#define MOTOR_L_LEDC_CHANNEL   LEDC_CHANNEL_2
#define MOTOR_R_LEDC_CHANNEL   LEDC_CHANNEL_3
#define MOTOR_LEDC_FREQ_HZ     1000
#define MOTOR_LEDC_RESOLUTION  LEDC_TIMER_8_BIT  // 0–255

// ─── Touch sensor (capacitive touch pad module) ───────────────────────────────
#define TOUCH_SIGNAL_PIN GPIO_NUM_8

// ─── Buzzer (passive, PWM-driven via LEDC) ───────────────────────────────────
#define BUZZER_PIN GPIO_NUM_7

// ─── Rotary encoder ───────────────────────────────────────────────────────────
#define ENCODER_CLK_PIN GPIO_NUM_9    // A channel
#define ENCODER_DT_PIN  GPIO_NUM_10   // B channel
#define ENCODER_SW_PIN  GPIO_NUM_11   // Push-button in encoder

// ─── LED indicators ──────────────────────────────────────────────────────────
#define LED_LEFT_PIN    GPIO_NUM_12   // Simple LED, left side (via 220 Ω)
#define LED_RIGHT_PIN   GPIO_NUM_47   // Right-side LED indicator
// GPIO48 = onboard WS2812 RGB LED on ESP32-S3-DevKitC-1 (do not reuse)

// ─── Boot / function button ──────────────────────────────────────────────────
#define BOOT_BUTTON_GPIO GPIO_NUM_0

#endif  // _BOARD_CONFIG_H_
