#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/no_display.h"
#include "mcp_server.h"
#include "system_reset.h"
#include "wheel_controller.h"
#include "wheel_face_display.h"
#include "wifi_board.h"

#define TAG "WheelChatbot"

class WheelChatbot : public WifiBoard {
private:
    Button boot_button_;
    AudioCodec*       audio_codec_  = nullptr;
    Display*          display_      = nullptr;
    WheelFaceDisplay* face_display_ = nullptr;  // non-null only when OLED succeeds

    // Optional animated face OLED (SSD1306) — falls back to NoDisplay on any failure.
    void InitializeDisplay() {
        i2c_master_bus_handle_t bus;
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port             = static_cast<i2c_port_t>(0),
            .sda_io_num           = DISPLAY_SDA_PIN,
            .scl_io_num           = DISPLAY_SCL_PIN,
            .clk_source           = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt    = 7,
            .intr_priority        = 0,
            .trans_queue_depth    = 0,
            .flags                = {.enable_internal_pullup = 1},
        };
        if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
            ESP_LOGW(TAG, "I2C init failed — running without display");
            display_ = new NoDisplay();
            return;
        }

        esp_lcd_panel_io_handle_t panel_io;
        esp_lcd_panel_io_i2c_config_t io_cfg = {
            .dev_addr             = 0x3C,
            .scl_speed_hz         = 400 * 1000,
            .control_phase_bytes  = 1,
            .dc_bit_offset        = 6,
            .lcd_cmd_bits         = 8,
            .lcd_param_bits       = 8,
        };
        if (esp_lcd_new_panel_io_i2c(bus, &io_cfg, &panel_io) != ESP_OK) {
            display_ = new NoDisplay();
            return;
        }

        esp_lcd_panel_handle_t panel;
        esp_lcd_panel_dev_config_t panel_cfg = {};
        panel_cfg.reset_gpio_num  = GPIO_NUM_NC;
        panel_cfg.bits_per_pixel  = 1;
        esp_lcd_panel_ssd1306_config_t ssd_cfg = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_cfg.vendor_config = &ssd_cfg;

        if (esp_lcd_new_panel_ssd1306(panel_io, &panel_cfg, &panel) != ESP_OK ||
            esp_lcd_panel_reset(panel)  != ESP_OK ||
            esp_lcd_panel_init(panel)   != ESP_OK) {
            display_ = new NoDisplay();
            return;
        }

        esp_lcd_panel_disp_on_off(panel, true);
        face_display_ = new WheelFaceDisplay(panel_io, panel,
                                              DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                              /*mirror_x=*/true, /*mirror_y=*/true);
        display_ = face_display_;
        ESP_LOGI(TAG, "WheelFaceDisplay (SSD1306) initialized");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeAudioCodec() {
        audio_codec_ = new NoAudioCodecSimplex(
            AUDIO_INPUT_SAMPLE_RATE,    AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK,   AUDIO_I2S_SPK_GPIO_LRCK,  AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK,    AUDIO_I2S_MIC_GPIO_WS,     AUDIO_I2S_MIC_GPIO_DIN);
    }

    // MCP tool: let the AI (or the wheel controller) change the face mode.
    void RegisterDisplayMcpTools() {
        if (face_display_ == nullptr) return;
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool(
            "self.display.set_mode",
            "Thay đổi chế độ màn hình OLED. mode: idle (mặt bình thường), "
            "happy (mặt vui), sad (mặt buồn), music (thanh EQ nhạc). "
            "Ví dụ: khi phát nhạc → mode=music; khi vui → mode=happy.",
            PropertyList({Property("mode", kPropertyTypeString, "idle")}),
            [this](const PropertyList& props) -> ReturnValue {
                if (face_display_ == nullptr) return false;
                std::string mode = props["mode"].value<std::string>();
                if (mode == "happy")       face_display_->SetFaceMode(FaceMode::kHappy);
                else if (mode == "sad")    face_display_->SetFaceMode(FaceMode::kSad);
                else if (mode == "music")  face_display_->SetFaceMode(FaceMode::kMusic);
                else if (mode == "talking") face_display_->SetFaceMode(FaceMode::kTalking);
                else                       face_display_->SetFaceMode(FaceMode::kIdle);
                return true;
            });
    }

public:
    WheelChatbot() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeButtons();
        InitializeAudioCodec();
        InitializeDisplay();
        InitializeWheelController();
        RegisterDisplayMcpTools();
        ESP_LOGI(TAG, "WheelChatbot board ready");
    }

    virtual AudioCodec* GetAudioCodec() override { return audio_codec_; }
    virtual Display*    GetDisplay()    override { return display_; }
};

DECLARE_BOARD(WheelChatbot);
