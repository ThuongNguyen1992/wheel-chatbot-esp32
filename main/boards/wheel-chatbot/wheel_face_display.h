#pragma once

#include "display/oled_display.h"

enum class FaceMode : uint8_t {
    kIdle = 0,
    kTalking,
    kMusic,
    kHappy,
    kSad,
    kSleepy,     // auto after 10 s idle: half-closed eyes + ZZZ
    kSurprised,  // wide eyes + big open mouth
    kAngry,      // squint eyes + eyebrows + tight frown
    kShy,        // normal eyes + blush dots + small smile
};

// Custom animated face display for the Wheel Chatbot.
// Renders pixel-art faces on a 128×64 SSD1306 OLED.
// Top 16 rows = status bar; bottom 48 rows = face area.
class WheelFaceDisplay : public OledDisplay {
public:
    WheelFaceDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                     int width, int height, bool mirror_x, bool mirror_y);
    ~WheelFaceDisplay();

    virtual void SetupUI() override;
    virtual bool SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void RemoveChatMessage(const char* role, const char* content) override;

    void SetFaceMode(FaceMode mode);

private:
    FaceMode current_mode_ = FaceMode::kIdle;

    // Face container (y = 16, h = 48)
    lv_obj_t* face_area_ = nullptr;

    // Eyes
    lv_obj_t* eye_l_ = nullptr;
    lv_obj_t* eye_r_ = nullptr;

    // Mouth: arc for smile/frown, oval for talking/surprised
    lv_obj_t* mouth_arc_  = nullptr;
    lv_obj_t* mouth_oval_ = nullptr;

    // Angry eyebrows (thin rects above each eye)
    lv_obj_t* brow_l_ = nullptr;
    lv_obj_t* brow_r_ = nullptr;

    // Shy blush (small ovals on cheeks)
    lv_obj_t* blush_l_ = nullptr;
    lv_obj_t* blush_r_ = nullptr;

    // Sleepy ZZZ label
    lv_obj_t* zzz_label_ = nullptr;

    // Music EQ bars
    static constexpr int kNumBars = 5;
    lv_obj_t* eq_bars_[kNumBars] = {};
    int eq_heights_[kNumBars]    = {28, 20, 38, 14, 32};
    lv_obj_t* music_label_       = nullptr;

    // Scrolling chat text overlay
    lv_obj_t* chat_label_ = nullptr;
    bool chat_active_ = false;

    // Animation
    lv_timer_t* anim_timer_  = nullptr;
    int anim_tick_           = 0;
    int blink_countdown_     = 25;
    bool is_blinking_        = false;
    int talking_state_       = 0;
    int idle_ticks_          = 0;  // counts 100 ms ticks while in kIdle

    static constexpr int kAutoSleepTicks = 100;  // 10 s before auto-sleep

    static void AnimTimerCb(lv_timer_t* timer);
    void Tick();
    void AnimIdle();
    void AnimTalking();
    void AnimMusic();
    void AnimHappy();
    void AnimSleepy();
    void AnimSurprised();
    void AnimAngry();
    void AnimShy();

    void SetEyeHeight(lv_coord_t h);
    void SetMouthOpen(int openness_px);
    void ShowFace(bool show);
    void ShowMusic(bool show);
    void ShowBrows(bool show);
    void ShowBlush(bool show);
    void ApplyModeShape();
};
