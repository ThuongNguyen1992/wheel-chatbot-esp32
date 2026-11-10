#pragma once

#include "display/oled_display.h"

enum class FaceMode : uint8_t {
    kIdle = 0,
    kTalking,
    kMusic,
    kHappy,
    kSad,
};

// Custom animated face display for the Wheel Chatbot.
// Renders pixel-art faces (idle/talking/music/happy/sad) on a 128×64 SSD1306 OLED.
// The top 16 rows are a compact status bar; the remaining 48 rows show the face.
// Extends OledDisplay to inherit LVGL + panel initialisation.
class WheelFaceDisplay : public OledDisplay {
public:
    WheelFaceDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                     int width, int height, bool mirror_x, bool mirror_y);
    ~WheelFaceDisplay();

    virtual void SetupUI() override;
    virtual bool SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void RemoveChatMessage(const char* role, const char* content) override;

    // Public – wheel controller and board can call this directly.
    void SetFaceMode(FaceMode mode);

private:
    FaceMode current_mode_ = FaceMode::kIdle;

    // Face container (y = 16, h = 48)
    lv_obj_t* face_area_ = nullptr;

    // Eyes
    lv_obj_t* eye_l_ = nullptr;
    lv_obj_t* eye_r_ = nullptr;

    // Mouth: arc for smile/frown, oval for talking open/close
    lv_obj_t* mouth_arc_  = nullptr;
    lv_obj_t* mouth_oval_ = nullptr;

    // Music EQ bars
    static constexpr int kNumBars = 5;
    lv_obj_t* eq_bars_[kNumBars] = {};
    int eq_heights_[kNumBars]    = {28, 20, 38, 14, 32};
    lv_obj_t* music_label_       = nullptr;

    // Scrolling chat text overlay at the bottom of the face area
    lv_obj_t* chat_label_ = nullptr;
    bool chat_active_ = false;

    // Animation
    lv_timer_t* anim_timer_     = nullptr;
    int anim_tick_              = 0;
    int blink_countdown_        = 25;
    bool is_blinking_           = false;
    int talking_state_          = 0;

    // ── Internal helpers ────────────────────────────────────────────────────
    static void AnimTimerCb(lv_timer_t* timer);

    // Called from timer – already inside LVGL task, no extra lock needed.
    void Tick();
    void AnimIdle();
    void AnimTalking();
    void AnimMusic();
    void AnimHappy();

    void SetEyeHeight(lv_coord_t h);
    void SetMouthOpen(int openness_px);
    void ShowFace(bool show);
    void ShowMusic(bool show);
    // Apply face shape for the current mode (called with LVGL lock held).
    void ApplyModeShape();
};
