#include "wheel_face_display.h"

#include <algorithm>
#include <cstring>
#include <string>

#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_random.h>
#include <lvgl.h>

#include "lvgl_theme.h"

#define TAG "WheelFace"

// ── Position constants (all relative to face_area_ which is 128 × 48) ────────
static constexpr int kEyeLX   = 26;   // left eye top-left x
static constexpr int kEyeRX   = 86;   // right eye top-left x
static constexpr int kEyeY    =  5;   // eye top-left y
static constexpr int kEyeSize = 16;   // eye width & initial height (circle)

static constexpr int kMouthX  = 44;   // arc object left-x (arc size 40)
static constexpr int kMouthY  = 20;   // arc object top-y
static constexpr int kMouthSz = 40;   // arc object square size

static constexpr int kOvalX   = 52;   // oval mouth left-x
static constexpr int kOvalW   = 24;   // oval mouth width

// EQ bar layout
static const int kBarXs[WheelFaceDisplay::kNumBars] = {10, 30, 50, 70, 90};
static constexpr int kBarW       = 18;
static constexpr int kFaceBottom = 46;  // y of bar bottom within face_area_

// ── Constructor / Destructor ──────────────────────────────────────────────────

WheelFaceDisplay::WheelFaceDisplay(esp_lcd_panel_io_handle_t panel_io,
                                   esp_lcd_panel_handle_t panel, int width, int height,
                                   bool mirror_x, bool mirror_y)
    : OledDisplay(panel_io, panel, width, height, mirror_x, mirror_y) {}

WheelFaceDisplay::~WheelFaceDisplay() {
    // Delete the animation timer before the base class tears LVGL down.
    if (anim_timer_ != nullptr) {
        DisplayLockGuard lock(this);
        lv_timer_delete(anim_timer_);
        anim_timer_ = nullptr;
    }
    // Null out base-class label pointers – these objects live inside the screen
    // which lvgl_port_deinit() (called from OledDisplay::~OledDisplay) will free.
    // LvglDisplay::~LvglDisplay only explicitly deletes battery_label_ and
    // low_battery_popup_, so the others are safe to clear here.
    network_label_      = nullptr;
    status_label_       = nullptr;
    notification_label_ = nullptr;
    mute_label_         = nullptr;
}

// ── SetupUI ───────────────────────────────────────────────────────────────────

void WheelFaceDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI already called, skipping");
        return;
    }
    // Mark base class flag – do NOT call OledDisplay::SetupUI() (that creates a
    // different layout we don't want).
    Display::SetupUI();

    DisplayLockGuard lock(this);

    auto* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    const lv_font_t* text_font = lvgl_theme->text_font()->font();
    const lv_font_t* icon_font = lvgl_theme->icon_font()->font();

    auto* screen = lv_screen_active();
    // White bg in LVGL = dark pixels on SSD1306 (monochrome mapping).
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // ── Status bar (y=0, h=16) ────────────────────────────────────────────────
    lv_obj_t* top_bar = lv_obj_create(screen);
    lv_obj_set_size(top_bar, LV_HOR_RES, 16);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(top_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_text_color(top_bar, lv_color_black(), 0);

    // WiFi icon (left) – base class UpdateStatusBar() writes here.
    network_label_ = lv_label_create(top_bar);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_black(), 0);

    // Status text (centre, scrolling) – base class SetStatus() writes here.
    status_label_ = lv_label_create(top_bar);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(status_label_, text_font, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_black(), 0);
    lv_label_set_text(status_label_, "...");
    // Scrolling animation
    static lv_anim_t sa;
    lv_anim_init(&sa);
    lv_anim_set_delay(&sa, 1000);
    lv_anim_set_repeat_count(&sa, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(status_label_, &sa, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(status_label_,
                                   lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    // Mute icon (right) – base class UpdateStatusBar() writes here.
    mute_label_ = lv_label_create(top_bar);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, lv_color_black(), 0);

    // Notification overlay (absolute, hidden) – base class ShowNotification() uses it.
    notification_label_ = lv_label_create(screen);
    lv_obj_set_size(notification_label_, LV_HOR_RES, 16);
    lv_obj_set_pos(notification_label_, 0, 0);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(notification_label_, text_font, 0);
    lv_obj_set_style_text_color(notification_label_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(notification_label_, LV_OPA_TRANSP, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    // ── Face area (y=16, h=48) ────────────────────────────────────────────────
    face_area_ = lv_obj_create(screen);
    lv_obj_set_size(face_area_, LV_HOR_RES, 48);
    lv_obj_set_pos(face_area_, 0, 16);
    lv_obj_set_style_bg_opa(face_area_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(face_area_, 0, 0);
    lv_obj_set_style_pad_all(face_area_, 0, 0);
    lv_obj_set_scrollbar_mode(face_area_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_layout(face_area_, LV_LAYOUT_NONE, 0);  // absolute children

    // Eyes ────────────────────────────────────────────────────────────────────
    auto make_eye = [&](int x) -> lv_obj_t* {
        lv_obj_t* e = lv_obj_create(face_area_);
        lv_obj_set_size(e, kEyeSize, kEyeSize);
        lv_obj_set_pos(e, x, kEyeY);
        lv_obj_set_style_radius(e, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(e, lv_color_black(), 0);  // black = lit on OLED
        lv_obj_set_style_border_width(e, 0, 0);
        lv_obj_set_scrollbar_mode(e, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_all(e, 0, 0);
        return e;
    };
    eye_l_ = make_eye(kEyeLX);
    eye_r_ = make_eye(kEyeRX);

    // Mouth – arc for smile / frown ───────────────────────────────────────────
    mouth_arc_ = lv_arc_create(face_area_);
    lv_obj_set_size(mouth_arc_, kMouthSz, kMouthSz);
    lv_obj_set_pos(mouth_arc_, kMouthX, kMouthY);
    lv_obj_clear_flag(mouth_arc_, LV_OBJ_FLAG_CLICKABLE);
    // Background arc (the visible track) – bottom semicircle = smile
    lv_arc_set_bg_start_angle(mouth_arc_, 0);
    lv_arc_set_bg_end_angle(mouth_arc_, 180);
    // Indicator: zero range so nothing shows
    lv_arc_set_value(mouth_arc_, 0);
    lv_arc_set_range(mouth_arc_, 0, 0);
    // Style
    lv_obj_set_style_arc_color(mouth_arc_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(mouth_arc_, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mouth_arc_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(mouth_arc_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(mouth_arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(mouth_arc_, 0, LV_PART_KNOB);

    // Mouth – oval for talking (initially hidden) ──────────────────────────────
    mouth_oval_ = lv_obj_create(face_area_);
    lv_obj_set_size(mouth_oval_, kOvalW, 10);
    lv_obj_set_pos(mouth_oval_, kOvalX, 32);
    lv_obj_set_style_radius(mouth_oval_, 5, 0);
    lv_obj_set_style_bg_color(mouth_oval_, lv_color_black(), 0);
    lv_obj_set_style_border_width(mouth_oval_, 0, 0);
    lv_obj_set_scrollbar_mode(mouth_oval_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(mouth_oval_, 0, 0);
    lv_obj_add_flag(mouth_oval_, LV_OBJ_FLAG_HIDDEN);

    // EQ bars (music mode, initially hidden) ──────────────────────────────────
    for (int i = 0; i < kNumBars; i++) {
        eq_bars_[i] = lv_obj_create(face_area_);
        lv_obj_set_style_radius(eq_bars_[i], 2, 0);
        lv_obj_set_style_bg_color(eq_bars_[i], lv_color_black(), 0);
        lv_obj_set_style_border_width(eq_bars_[i], 0, 0);
        lv_obj_set_scrollbar_mode(eq_bars_[i], LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_all(eq_bars_[i], 0, 0);
        int h = eq_heights_[i];
        lv_obj_set_size(eq_bars_[i], kBarW, h);
        lv_obj_set_pos(eq_bars_[i], kBarXs[i], kFaceBottom - h);
        lv_obj_add_flag(eq_bars_[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Music label (above EQ bars) ─────────────────────────────────────────────
    music_label_ = lv_label_create(face_area_);
    lv_label_set_text(music_label_, "Phát nhạc ~");
    lv_obj_set_width(music_label_, LV_HOR_RES);
    lv_obj_set_pos(music_label_, 0, 1);
    lv_obj_set_style_text_align(music_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(music_label_, text_font, 0);
    lv_obj_set_style_text_color(music_label_, lv_color_black(), 0);
    lv_obj_add_flag(music_label_, LV_OBJ_FLAG_HIDDEN);

    // Chat text overlay (bottom of face area, scrolling) ─────────────────────
    chat_label_ = lv_label_create(face_area_);
    lv_label_set_text(chat_label_, "");
    lv_obj_set_width(chat_label_, LV_HOR_RES);
    lv_obj_set_pos(chat_label_, 0, 38);
    lv_label_set_long_mode(chat_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(chat_label_, text_font, 0);
    lv_obj_set_style_text_color(chat_label_, lv_color_black(), 0);
    static lv_anim_t ca;
    lv_anim_init(&ca);
    lv_anim_set_delay(&ca, 800);
    lv_anim_set_repeat_count(&ca, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_label_, &ca, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_label_,
                                   lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
    lv_obj_add_flag(chat_label_, LV_OBJ_FLAG_HIDDEN);

    // Start 10 fps animation timer ────────────────────────────────────────────
    anim_timer_ = lv_timer_create(AnimTimerCb, 100, this);

    ESP_LOGI(TAG, "WheelFaceDisplay UI ready (%d×%d)", width_, height_);
}

// ── Public API ────────────────────────────────────────────────────────────────

bool WheelFaceDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr || face_area_ == nullptr) return false;
    std::string e = emotion;
    if (e == "happy" || e == "laugh" || e == "excited") {
        SetFaceMode(FaceMode::kHappy);
    } else if (e == "sad" || e == "cry" || e == "angry" || e == "disappointed") {
        SetFaceMode(FaceMode::kSad);
    } else if (e == "music" || e == "singing" || e == "song") {
        SetFaceMode(FaceMode::kMusic);
    } else {
        // neutral, robot_2, confused, etc. → idle face
        if (current_mode_ != FaceMode::kTalking) {
            SetFaceMode(FaceMode::kIdle);
        }
    }
    return true;
}

void WheelFaceDisplay::SetChatMessage(const char* role, const char* content) {
    if (chat_label_ == nullptr) return;
    DisplayLockGuard lock(this);

    bool empty = (content == nullptr || content[0] == '\0');
    if (empty) {
        lv_label_set_text(chat_label_, "");
        lv_obj_add_flag(chat_label_, LV_OBJ_FLAG_HIDDEN);
        chat_active_ = false;
        // AI finished speaking → return to idle
        if (current_mode_ == FaceMode::kTalking) {
            current_mode_ = FaceMode::kIdle;
            ApplyModeShape();
        }
        return;
    }

    std::string msg = content;
    std::replace(msg.begin(), msg.end(), '\n', ' ');
    lv_label_set_text(chat_label_, msg.c_str());
    lv_obj_remove_flag(chat_label_, LV_OBJ_FLAG_HIDDEN);
    chat_active_ = true;

    // Show talking animation for assistant messages (not system/user)
    if (strcmp(role, "assistant") == 0 && current_mode_ != FaceMode::kMusic) {
        current_mode_ = FaceMode::kTalking;
        ApplyModeShape();
    }
}

void WheelFaceDisplay::RemoveChatMessage(const char* role, const char* content) {
    if (chat_label_ == nullptr || content == nullptr) return;
    DisplayLockGuard lock(this);

    std::string msg = content;
    std::replace(msg.begin(), msg.end(), '\n', ' ');
    const char* current = lv_label_get_text(chat_label_);
    if (current == nullptr || msg != current) return;  // different message – ignore

    lv_label_set_text(chat_label_, "");
    lv_obj_add_flag(chat_label_, LV_OBJ_FLAG_HIDDEN);
    chat_active_ = false;
    if (current_mode_ == FaceMode::kTalking) {
        current_mode_ = FaceMode::kIdle;
        ApplyModeShape();
    }
}

void WheelFaceDisplay::SetFaceMode(FaceMode mode) {
    if (face_area_ == nullptr) {
        current_mode_ = mode;
        return;
    }
    DisplayLockGuard lock(this);
    if (current_mode_ == mode) return;
    current_mode_ = mode;
    ApplyModeShape();
}

// ── Internal helpers (may be called with or without LVGL lock) ────────────────

void WheelFaceDisplay::ApplyModeShape() {
    // Assumes LVGL lock is already held by the caller.
    if (current_mode_ == FaceMode::kMusic) {
        ShowFace(false);
        ShowMusic(true);
        return;
    }
    ShowMusic(false);
    ShowFace(true);
    // Reset mouth to arc (hide oval)
    lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_oval_, LV_OBJ_FLAG_HIDDEN);
    if (current_mode_ == FaceMode::kSad) {
        // Frown: top arc (goes 180° → 0° passing through 270° = top)
        lv_arc_set_bg_start_angle(mouth_arc_, 180);
        lv_arc_set_bg_end_angle(mouth_arc_, 360);
    } else {
        // Smile: bottom arc (goes 0° → 180° passing through 90° = bottom)
        lv_arc_set_bg_start_angle(mouth_arc_, 0);
        lv_arc_set_bg_end_angle(mouth_arc_, 180);
    }
    // For happy mode: widen smile (use bigger arc object)
    if (current_mode_ == FaceMode::kHappy) {
        lv_obj_set_size(mouth_arc_, 50, 50);
        lv_obj_set_pos(mouth_arc_, kMouthX - 5, kMouthY - 2);
    } else {
        lv_obj_set_size(mouth_arc_, kMouthSz, kMouthSz);
        lv_obj_set_pos(mouth_arc_, kMouthX, kMouthY);
    }
}

void WheelFaceDisplay::ShowFace(bool show) {
    auto flag_fn = show ? lv_obj_remove_flag : lv_obj_add_flag;
    flag_fn(eye_l_, LV_OBJ_FLAG_HIDDEN);
    flag_fn(eye_r_, LV_OBJ_FLAG_HIDDEN);
    flag_fn(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
    if (!show) lv_obj_add_flag(mouth_oval_, LV_OBJ_FLAG_HIDDEN);
}

void WheelFaceDisplay::ShowMusic(bool show) {
    auto flag_fn = show ? lv_obj_remove_flag : lv_obj_add_flag;
    for (int i = 0; i < kNumBars; i++) flag_fn(eq_bars_[i], LV_OBJ_FLAG_HIDDEN);
    flag_fn(music_label_, LV_OBJ_FLAG_HIDDEN);
}

void WheelFaceDisplay::SetEyeHeight(lv_coord_t h) {
    if (!eye_l_ || !eye_r_) return;
    lv_obj_set_height(eye_l_, h);
    lv_obj_set_height(eye_r_, h);
    lv_coord_t r = std::min((int)h / 2, kEyeSize / 2);
    lv_obj_set_style_radius(eye_l_, r, 0);
    lv_obj_set_style_radius(eye_r_, r, 0);
}

void WheelFaceDisplay::SetMouthOpen(int openness_px) {
    if (!mouth_arc_ || !mouth_oval_) return;
    if (openness_px <= 2) {
        lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_oval_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(mouth_oval_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(mouth_oval_, openness_px);
        // Keep oval vertically centred around y=37 of face_area_
        int y = 37 - openness_px / 2;
        lv_obj_set_pos(mouth_oval_, kOvalX, y);
    }
}

// ── Animation (called from LVGL timer – lock already held by LVGL task) ───────

void WheelFaceDisplay::AnimTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<WheelFaceDisplay*>(lv_timer_get_user_data(timer));
    if (!self || !self->face_area_) return;
    self->Tick();
}

void WheelFaceDisplay::Tick() {
    anim_tick_++;
    switch (current_mode_) {
        case FaceMode::kIdle:    AnimIdle();    break;
        case FaceMode::kTalking: AnimTalking(); break;
        case FaceMode::kMusic:   AnimMusic();   break;
        case FaceMode::kHappy:   AnimHappy();   break;
        case FaceMode::kSad:
            // Sad uses the same blink as idle; mouth was set to frown in ApplyModeShape().
            AnimIdle();
            break;
    }
}

void WheelFaceDisplay::AnimIdle() {
    if (!is_blinking_) {
        blink_countdown_--;
        if (blink_countdown_ <= 0) {
            is_blinking_   = true;
            blink_countdown_ = 2;
            SetEyeHeight(3);
        }
    } else {
        blink_countdown_--;
        if (blink_countdown_ <= 0) {
            is_blinking_     = false;
            blink_countdown_ = 22 + (int)(esp_random() % 28);  // 2.2–5.0 s
            SetEyeHeight(kEyeSize);
        }
    }
}

void WheelFaceDisplay::AnimTalking() {
    // Mouth cycles: 0=closed → 1=small → 2=open → 3=big → 2=open → 1=small
    static const int kOpenSizes[] = {0, 6, 12, 16, 12, 6};
    static constexpr int kStates = sizeof(kOpenSizes) / sizeof(kOpenSizes[0]);
    talking_state_ = (talking_state_ + 1) % kStates;
    SetMouthOpen(kOpenSizes[talking_state_]);

    // Occasional slow blink
    if ((anim_tick_ % 20) == 0) SetEyeHeight(3);
    if ((anim_tick_ % 20) == 2) SetEyeHeight(kEyeSize);
}

void WheelFaceDisplay::AnimMusic() {
    // Random-walk EQ bar heights
    for (int i = 0; i < kNumBars; i++) {
        int delta = (int)(esp_random() % 15) - 7;
        eq_heights_[i] = std::max(8, std::min(42, eq_heights_[i] + delta));
        int h = eq_heights_[i];
        lv_obj_set_size(eq_bars_[i], kBarW, h);
        lv_obj_set_pos(eq_bars_[i], kBarXs[i], kFaceBottom - h);
    }
}

void WheelFaceDisplay::AnimHappy() {
    // Eyes bounce up and down (2 px range, 8-frame period)
    int offset = (anim_tick_ % 8 < 4) ? -2 : 0;
    lv_obj_set_pos(eye_l_, kEyeLX, kEyeY + offset);
    lv_obj_set_pos(eye_r_, kEyeRX, kEyeY + offset);

    // Quick wink every 15 frames
    if ((anim_tick_ % 15) == 0)  SetEyeHeight(4);
    if ((anim_tick_ % 15) == 1)  SetEyeHeight(kEyeSize);
}
