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

// ── Position constants (relative to face_area_: 128 × 48) ───────────────────
static constexpr int kEyeLX   = 26;
static constexpr int kEyeRX   = 86;
static constexpr int kEyeY    =  5;
static constexpr int kEyeSize = 16;

static constexpr int kMouthX  = 44;
static constexpr int kMouthY  = 20;
static constexpr int kMouthSz = 40;

static constexpr int kOvalX   = 52;
static constexpr int kOvalW   = 24;

// EQ bar layout
static const int kBarXs[WheelFaceDisplay::kNumBars] = {10, 30, 50, 70, 90};
static constexpr int kBarW       = 18;
static constexpr int kFaceBottom = 46;

// ── Constructor / Destructor ─────────────────────────────────────────────────

WheelFaceDisplay::WheelFaceDisplay(esp_lcd_panel_io_handle_t panel_io,
                                   esp_lcd_panel_handle_t panel, int width, int height,
                                   bool mirror_x, bool mirror_y)
    : OledDisplay(panel_io, panel, width, height, mirror_x, mirror_y) {}

WheelFaceDisplay::~WheelFaceDisplay() {
    if (anim_timer_ != nullptr) {
        DisplayLockGuard lock(this);
        lv_timer_delete(anim_timer_);
        anim_timer_ = nullptr;
    }
    network_label_      = nullptr;
    status_label_       = nullptr;
    notification_label_ = nullptr;
    mute_label_         = nullptr;
}

// ── SetupUI ──────────────────────────────────────────────────────────────────

void WheelFaceDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI already called, skipping");
        return;
    }
    Display::SetupUI();

    DisplayLockGuard lock(this);

    auto* lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    const lv_font_t* text_font = lvgl_theme->text_font()->font();
    const lv_font_t* icon_font = lvgl_theme->icon_font()->font();

    auto* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // ── Status bar (y=0, h=16) ───────────────────────────────────────────────
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

    network_label_ = lv_label_create(top_bar);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_black(), 0);

    status_label_ = lv_label_create(top_bar);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(status_label_, text_font, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_black(), 0);
    lv_label_set_text(status_label_, "...");
    static lv_anim_t sa;
    lv_anim_init(&sa);
    lv_anim_set_delay(&sa, 1000);
    lv_anim_set_repeat_count(&sa, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(status_label_, &sa, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(status_label_,
                                   lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    mute_label_ = lv_label_create(top_bar);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, lv_color_black(), 0);

    notification_label_ = lv_label_create(screen);
    lv_obj_set_size(notification_label_, LV_HOR_RES, 16);
    lv_obj_set_pos(notification_label_, 0, 0);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(notification_label_, text_font, 0);
    lv_obj_set_style_text_color(notification_label_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(notification_label_, LV_OPA_TRANSP, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    // ── Face area (y=16, h=48) ───────────────────────────────────────────────
    face_area_ = lv_obj_create(screen);
    lv_obj_set_size(face_area_, LV_HOR_RES, 48);
    lv_obj_set_pos(face_area_, 0, 16);
    lv_obj_set_style_bg_opa(face_area_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(face_area_, 0, 0);
    lv_obj_set_style_pad_all(face_area_, 0, 0);
    lv_obj_set_scrollbar_mode(face_area_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_layout(face_area_, LV_LAYOUT_NONE, 0);

    // Eyes ───────────────────────────────────────────────────────────────────
    auto make_eye = [&](int x) -> lv_obj_t* {
        lv_obj_t* e = lv_obj_create(face_area_);
        lv_obj_set_size(e, kEyeSize, kEyeSize);
        lv_obj_set_pos(e, x, kEyeY);
        lv_obj_set_style_radius(e, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(e, lv_color_black(), 0);
        lv_obj_set_style_border_width(e, 0, 0);
        lv_obj_set_scrollbar_mode(e, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_all(e, 0, 0);
        return e;
    };
    eye_l_ = make_eye(kEyeLX);
    eye_r_ = make_eye(kEyeRX);

    // Mouth arc (smile / frown) ──────────────────────────────────────────────
    mouth_arc_ = lv_arc_create(face_area_);
    lv_obj_set_size(mouth_arc_, kMouthSz, kMouthSz);
    lv_obj_set_pos(mouth_arc_, kMouthX, kMouthY);
    lv_obj_clear_flag(mouth_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_start_angle(mouth_arc_, 0);
    lv_arc_set_bg_end_angle(mouth_arc_, 180);
    lv_arc_set_value(mouth_arc_, 0);
    lv_arc_set_range(mouth_arc_, 0, 0);
    lv_obj_set_style_arc_color(mouth_arc_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(mouth_arc_, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mouth_arc_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(mouth_arc_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(mouth_arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(mouth_arc_, 0, LV_PART_KNOB);

    // Mouth oval (talking / surprised) ───────────────────────────────────────
    mouth_oval_ = lv_obj_create(face_area_);
    lv_obj_set_size(mouth_oval_, kOvalW, 10);
    lv_obj_set_pos(mouth_oval_, kOvalX, 32);
    lv_obj_set_style_radius(mouth_oval_, 5, 0);
    lv_obj_set_style_bg_color(mouth_oval_, lv_color_black(), 0);
    lv_obj_set_style_border_width(mouth_oval_, 0, 0);
    lv_obj_set_scrollbar_mode(mouth_oval_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(mouth_oval_, 0, 0);
    lv_obj_add_flag(mouth_oval_, LV_OBJ_FLAG_HIDDEN);

    // Angry eyebrows (thin rects above eyes) ─────────────────────────────────
    auto make_brow = [&](int x) -> lv_obj_t* {
        lv_obj_t* b = lv_obj_create(face_area_);
        lv_obj_set_size(b, 18, 3);
        lv_obj_set_pos(b, x, 0);
        lv_obj_set_style_radius(b, 0, 0);
        lv_obj_set_style_bg_color(b, lv_color_black(), 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_scrollbar_mode(b, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
        return b;
    };
    brow_l_ = make_brow(kEyeLX);
    brow_r_ = make_brow(kEyeRX);

    // Shy blush (small ovals on cheeks) ──────────────────────────────────────
    auto make_blush = [&](int x) -> lv_obj_t* {
        lv_obj_t* bl = lv_obj_create(face_area_);
        lv_obj_set_size(bl, 14, 6);
        lv_obj_set_pos(bl, x, 26);
        lv_obj_set_style_radius(bl, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(bl, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(bl, LV_OPA_40, 0);  // lighter fill = blush dot
        lv_obj_set_style_border_width(bl, 0, 0);
        lv_obj_set_scrollbar_mode(bl, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_pad_all(bl, 0, 0);
        lv_obj_add_flag(bl, LV_OBJ_FLAG_HIDDEN);
        return bl;
    };
    blush_l_ = make_blush(10);   // left cheek
    blush_r_ = make_blush(104);  // right cheek

    // Sleepy ZZZ label (top-right of face area) ──────────────────────────────
    zzz_label_ = lv_label_create(face_area_);
    lv_label_set_text(zzz_label_, "z");
    lv_obj_set_pos(zzz_label_, 95, 2);
    lv_obj_set_style_text_font(zzz_label_, text_font, 0);
    lv_obj_set_style_text_color(zzz_label_, lv_color_black(), 0);
    lv_obj_add_flag(zzz_label_, LV_OBJ_FLAG_HIDDEN);

    // EQ bars (music mode) ───────────────────────────────────────────────────
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

    // Music label ─────────────────────────────────────────────────────────────
    music_label_ = lv_label_create(face_area_);
    lv_label_set_text(music_label_, "Phat nhac ~");
    lv_obj_set_width(music_label_, LV_HOR_RES);
    lv_obj_set_pos(music_label_, 0, 1);
    lv_obj_set_style_text_align(music_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(music_label_, text_font, 0);
    lv_obj_set_style_text_color(music_label_, lv_color_black(), 0);
    lv_obj_add_flag(music_label_, LV_OBJ_FLAG_HIDDEN);

    // Chat text overlay ───────────────────────────────────────────────────────
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

    anim_timer_ = lv_timer_create(AnimTimerCb, 100, this);

    ESP_LOGI(TAG, "WheelFaceDisplay UI ready (%d x %d)", width_, height_);
}

// ── Public API ────────────────────────────────────────────────────────────────

bool WheelFaceDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr || face_area_ == nullptr) return false;
    std::string e = emotion;

    idle_ticks_ = 0;  // any emotion call resets idle counter

    if (e == "happy" || e == "laugh" || e == "excited" || e == "joy") {
        SetFaceMode(FaceMode::kHappy);
    } else if (e == "sad" || e == "cry" || e == "disappointed" || e == "depressed") {
        SetFaceMode(FaceMode::kSad);
    } else if (e == "angry" || e == "frustrated" || e == "furious") {
        SetFaceMode(FaceMode::kAngry);
    } else if (e == "surprised" || e == "curious" || e == "wow" || e == "shocked") {
        SetFaceMode(FaceMode::kSurprised);
    } else if (e == "shy" || e == "embarrassed" || e == "blush" || e == "love") {
        SetFaceMode(FaceMode::kShy);
    } else if (e == "sleepy" || e == "tired" || e == "bored") {
        SetFaceMode(FaceMode::kSleepy);
    } else if (e == "music" || e == "singing" || e == "song") {
        SetFaceMode(FaceMode::kMusic);
    } else {
        // neutral, robot_2, confused, etc.
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
        if (current_mode_ == FaceMode::kTalking) {
            current_mode_ = FaceMode::kIdle;
            idle_ticks_   = 0;
            ApplyModeShape();
        }
        return;
    }

    std::string msg = content;
    std::replace(msg.begin(), msg.end(), '\n', ' ');
    lv_label_set_text(chat_label_, msg.c_str());
    lv_obj_remove_flag(chat_label_, LV_OBJ_FLAG_HIDDEN);
    chat_active_ = true;
    idle_ticks_  = 0;

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
    if (current == nullptr || msg != current) return;

    lv_label_set_text(chat_label_, "");
    lv_obj_add_flag(chat_label_, LV_OBJ_FLAG_HIDDEN);
    chat_active_ = false;
    if (current_mode_ == FaceMode::kTalking) {
        current_mode_ = FaceMode::kIdle;
        idle_ticks_   = 0;
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
    if (mode != FaceMode::kIdle && mode != FaceMode::kSleepy) {
        idle_ticks_ = 0;
    }
    ApplyModeShape();
}

// ── Internal helpers ─────────────────────────────────────────────────────────

void WheelFaceDisplay::ApplyModeShape() {
    if (current_mode_ == FaceMode::kMusic) {
        ShowFace(false);
        ShowMusic(true);
        ShowBrows(false);
        ShowBlush(false);
        lv_obj_add_flag(zzz_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    ShowMusic(false);
    ShowFace(true);

    // Reset mouth to default arc position/size
    lv_obj_set_size(mouth_arc_, kMouthSz, kMouthSz);
    lv_obj_set_pos(mouth_arc_, kMouthX, kMouthY);
    lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mouth_oval_, LV_OBJ_FLAG_HIDDEN);

    // Reset eye size
    lv_obj_set_size(eye_l_, kEyeSize, kEyeSize);
    lv_obj_set_size(eye_r_, kEyeSize, kEyeSize);
    lv_obj_set_pos(eye_l_, kEyeLX, kEyeY);
    lv_obj_set_pos(eye_r_, kEyeRX, kEyeY);
    lv_obj_set_style_radius(eye_l_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(eye_r_, LV_RADIUS_CIRCLE, 0);

    ShowBrows(false);
    ShowBlush(false);
    lv_obj_add_flag(zzz_label_, LV_OBJ_FLAG_HIDDEN);

    switch (current_mode_) {
        case FaceMode::kIdle:
            // Smile
            lv_arc_set_bg_start_angle(mouth_arc_, 0);
            lv_arc_set_bg_end_angle(mouth_arc_, 180);
            break;

        case FaceMode::kTalking:
            // Mouth starts closed; AnimTalking() will open it
            lv_arc_set_bg_start_angle(mouth_arc_, 0);
            lv_arc_set_bg_end_angle(mouth_arc_, 180);
            break;

        case FaceMode::kHappy:
            // Wide smile
            lv_obj_set_size(mouth_arc_, 50, 50);
            lv_obj_set_pos(mouth_arc_, kMouthX - 5, kMouthY - 2);
            lv_arc_set_bg_start_angle(mouth_arc_, 0);
            lv_arc_set_bg_end_angle(mouth_arc_, 180);
            break;

        case FaceMode::kSad:
            // Frown
            lv_arc_set_bg_start_angle(mouth_arc_, 180);
            lv_arc_set_bg_end_angle(mouth_arc_, 360);
            break;

        case FaceMode::kSleepy:
            // Half-closed eyes + tiny smile + ZZZ
            SetEyeHeight(4);
            lv_arc_set_bg_start_angle(mouth_arc_, 0);
            lv_arc_set_bg_end_angle(mouth_arc_, 180);
            lv_obj_set_size(mouth_arc_, 24, 24);
            lv_obj_set_pos(mouth_arc_, 52, 28);
            lv_obj_remove_flag(zzz_label_, LV_OBJ_FLAG_HIDDEN);
            break;

        case FaceMode::kSurprised:
            // Wide eyes + big open oval mouth
            lv_obj_set_size(eye_l_, 20, 20);
            lv_obj_set_size(eye_r_, 20, 20);
            lv_obj_set_pos(eye_l_, kEyeLX - 2, kEyeY - 2);
            lv_obj_set_pos(eye_r_, kEyeRX - 2, kEyeY - 2);
            lv_obj_add_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(mouth_oval_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(mouth_oval_, 22, 20);
            lv_obj_set_style_radius(mouth_oval_, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_pos(mouth_oval_, 53, 26);
            break;

        case FaceMode::kAngry:
            // Squint eyes + eyebrows + tight frown
            SetEyeHeight(6);
            ShowBrows(true);
            // Position brows closer to eyes (inner corner lower to show anger)
            lv_obj_set_pos(brow_l_, kEyeLX,     1);
            lv_obj_set_pos(brow_r_, kEyeRX,     1);
            lv_arc_set_bg_start_angle(mouth_arc_, 180);
            lv_arc_set_bg_end_angle(mouth_arc_, 360);
            lv_obj_set_size(mouth_arc_, 28, 28);
            lv_obj_set_pos(mouth_arc_, 50, 26);
            break;

        case FaceMode::kShy:
            // Normal eyes + small smile + blush
            ShowBlush(true);
            lv_arc_set_bg_start_angle(mouth_arc_, 0);
            lv_arc_set_bg_end_angle(mouth_arc_, 180);
            lv_obj_set_size(mouth_arc_, 28, 28);
            lv_obj_set_pos(mouth_arc_, 50, 24);
            break;

        default:
            break;
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

void WheelFaceDisplay::ShowBrows(bool show) {
    auto flag_fn = show ? lv_obj_remove_flag : lv_obj_add_flag;
    if (brow_l_) flag_fn(brow_l_, LV_OBJ_FLAG_HIDDEN);
    if (brow_r_) flag_fn(brow_r_, LV_OBJ_FLAG_HIDDEN);
}

void WheelFaceDisplay::ShowBlush(bool show) {
    auto flag_fn = show ? lv_obj_remove_flag : lv_obj_add_flag;
    if (blush_l_) flag_fn(blush_l_, LV_OBJ_FLAG_HIDDEN);
    if (blush_r_) flag_fn(blush_r_, LV_OBJ_FLAG_HIDDEN);
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
        lv_obj_set_size(mouth_oval_, kOvalW, openness_px);
        lv_obj_set_style_radius(mouth_oval_, 5, 0);
        int y = 37 - openness_px / 2;
        lv_obj_set_pos(mouth_oval_, kOvalX, y);
    }
}

// ── Animation timer ───────────────────────────────────────────────────────────

void WheelFaceDisplay::AnimTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<WheelFaceDisplay*>(lv_timer_get_user_data(timer));
    if (!self || !self->face_area_) return;
    self->Tick();
}

void WheelFaceDisplay::Tick() {
    anim_tick_++;
    switch (current_mode_) {
        case FaceMode::kIdle:       AnimIdle();      break;
        case FaceMode::kTalking:    AnimTalking();   break;
        case FaceMode::kMusic:      AnimMusic();     break;
        case FaceMode::kHappy:      AnimHappy();     break;
        case FaceMode::kSad:        AnimIdle();      break;  // frown + blink
        case FaceMode::kSleepy:     AnimSleepy();    break;
        case FaceMode::kSurprised:  AnimSurprised(); break;
        case FaceMode::kAngry:      AnimAngry();     break;
        case FaceMode::kShy:        AnimShy();       break;
    }
}

// idle: normal blink, auto-sleep after kAutoSleepTicks
void WheelFaceDisplay::AnimIdle() {
    if (!is_blinking_) {
        blink_countdown_--;
        if (blink_countdown_ <= 0) {
            is_blinking_     = true;
            blink_countdown_ = 2;
            SetEyeHeight(3);
        }
    } else {
        blink_countdown_--;
        if (blink_countdown_ <= 0) {
            is_blinking_     = false;
            blink_countdown_ = 22 + (int)(esp_random() % 28);
            SetEyeHeight(kEyeSize);
        }
    }

    idle_ticks_++;
    if (idle_ticks_ >= kAutoSleepTicks) {
        idle_ticks_   = 0;
        current_mode_ = FaceMode::kSleepy;
        ApplyModeShape();
    }
}

// talking: mouth opens and closes
void WheelFaceDisplay::AnimTalking() {
    static const int kOpenSizes[] = {0, 6, 12, 16, 12, 6};
    static constexpr int kStates = sizeof(kOpenSizes) / sizeof(kOpenSizes[0]);
    talking_state_ = (talking_state_ + 1) % kStates;
    SetMouthOpen(kOpenSizes[talking_state_]);

    if ((anim_tick_ % 20) == 0) SetEyeHeight(3);
    if ((anim_tick_ % 20) == 2) SetEyeHeight(kEyeSize);
}

// music: random EQ bars
void WheelFaceDisplay::AnimMusic() {
    for (int i = 0; i < kNumBars; i++) {
        int delta = (int)(esp_random() % 15) - 7;
        eq_heights_[i] = std::max(8, std::min(42, eq_heights_[i] + delta));
        int h = eq_heights_[i];
        lv_obj_set_size(eq_bars_[i], kBarW, h);
        lv_obj_set_pos(eq_bars_[i], kBarXs[i], kFaceBottom - h);
    }
}

// happy: eyes bounce, quick wink
void WheelFaceDisplay::AnimHappy() {
    int offset = (anim_tick_ % 8 < 4) ? -2 : 0;
    lv_obj_set_pos(eye_l_, kEyeLX, kEyeY + offset);
    lv_obj_set_pos(eye_r_, kEyeRX, kEyeY + offset);

    if ((anim_tick_ % 15) == 0) SetEyeHeight(4);
    if ((anim_tick_ % 15) == 1) SetEyeHeight(kEyeSize);
}

// sleepy: eyes droop open/close slowly, ZZZ cycles
void WheelFaceDisplay::AnimSleepy() {
    // Slowly open and close eyes (period 60 ticks = 6 s)
    int phase = anim_tick_ % 60;
    if (phase < 10) {
        SetEyeHeight(2);  // almost closed
    } else if (phase < 20) {
        SetEyeHeight(5);  // half open
    } else if (phase < 30) {
        SetEyeHeight(2);  // close again
    }
    // else eyes stay at whatever last height

    // Cycle ZZZ text every 20 ticks (~2 s)
    if (zzz_label_) {
        int zphase = anim_tick_ % 60;
        if (zphase == 0)  lv_label_set_text(zzz_label_, "z");
        if (zphase == 20) lv_label_set_text(zzz_label_, "z Z");
        if (zphase == 40) lv_label_set_text(zzz_label_, "Z z z");
    }

    // Wake up when SetFaceMode is called externally (idle_ticks_ check not needed here)
}

// surprised: eyes pulse slightly bigger/normal
void WheelFaceDisplay::AnimSurprised() {
    // Pulse eyes every 15 ticks
    int phase = anim_tick_ % 15;
    if (phase == 0) {
        lv_obj_set_size(eye_l_, 22, 22);
        lv_obj_set_size(eye_r_, 22, 22);
    } else if (phase == 5) {
        lv_obj_set_size(eye_l_, 20, 20);
        lv_obj_set_size(eye_r_, 20, 20);
    }
}

// angry: eyebrows twitch, occasional hard blink
void WheelFaceDisplay::AnimAngry() {
    // Eyebrow twitch: shift 1px down and back
    if ((anim_tick_ % 20) == 0) {
        lv_obj_set_pos(brow_l_, kEyeLX, 2);
        lv_obj_set_pos(brow_r_, kEyeRX, 2);
    }
    if ((anim_tick_ % 20) == 3) {
        lv_obj_set_pos(brow_l_, kEyeLX, 1);
        lv_obj_set_pos(brow_r_, kEyeRX, 1);
    }

    // Hard blink every 25 ticks
    if ((anim_tick_ % 25) == 0) SetEyeHeight(2);
    if ((anim_tick_ % 25) == 1) SetEyeHeight(6);
}

// shy: slow blink, blush fades in/out
void WheelFaceDisplay::AnimShy() {
    // Slow blink
    if ((anim_tick_ % 30) == 0) SetEyeHeight(3);
    if ((anim_tick_ % 30) == 2) SetEyeHeight(kEyeSize);

    // Blush pulse: alternate opacity
    if (blush_l_ && blush_r_) {
        lv_opa_t opa = ((anim_tick_ % 20) < 10) ? LV_OPA_40 : LV_OPA_60;
        lv_obj_set_style_bg_opa(blush_l_, opa, 0);
        lv_obj_set_style_bg_opa(blush_r_, opa, 0);
    }
}
