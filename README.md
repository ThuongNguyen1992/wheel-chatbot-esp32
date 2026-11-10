# Wheel Chatbot – ESP32-S3 AI Robot

An ESP32-S3 voice AI robot that can talk, move, and show emotions on an OLED display.  
Based on [XiaoZhi](https://github.com/78/xiaozhi-esp32) firmware, requires ESP-IDF v6.0.1+ (`release/v6.1` recommended).

---

## Features

- Offline wake word detection via ESP-SR
- Voice chat over WebSocket or MQTT+UDP with LLMs (Qwen, DeepSeek, etc.)
- Two-wheel drive via L298N – move / turn / dance – controlled through MCP tools
- OLED 128×64 face animations: idle / talking / happy / sad / music
- Audio: INMP441 mic (I2S) + MAX98357A amp (I2S) + 8Ω speaker
- Wi-Fi provisioning via hotspot (`192.168.4.1`)
- 39 interface languages including Vietnamese (vi-VN)

---

## Hardware

| Part | Component | Description |
|------|-----------|-------------|
| U1 | ESP32-S3-WROOM-1 N8R2 | 8 MB Flash, 2 MB PSRAM |
| U2 | INMP441 | I2S MEMS microphone |
| U3 | MAX98357A | I2S 3 W amplifier |
| U4 | SSD1306 0.96" OLED | 128×64 I2C display |
| U5 | L298N | Dual DC motor driver |
| U6 | TP4056 BMS | 18650 charge/protection |
| M1–M2 | TT DC motors | Left + right wheels (geared) |
| BAT | 4× 18650 | 2S2P ~7.4 V 6000 mAh |

**GPIO summary**

| GPIO | Function |
|------|----------|
| 4, 5, 6 | I2S MIC – WS, SCK, DIN (INMP441) |
| 15, 16, 17 | I2S SPK – BCLK, LRCK, DOUT (MAX98357A) |
| 41, 42 | I2C – SDA, SCL (OLED) |
| 19, 20, 21 | Left motor – ENA, IN1, IN2 |
| 35, 36, 37 | Right motor – ENB, IN3, IN4 |
| 0 | BOOT button / Wi-Fi config |
| 48 | Onboard RGB LED |

Full wiring diagram: [main/boards/wheel-chatbot/HARDWARE.md](main/boards/wheel-chatbot/HARDWARE.md)

---

## Build & Flash

```bash
# Activate ESP-IDF
source ~/esp/esp-idf/export.sh

# Build
python3 scripts/build.py wheel-chatbot --name wheel-chatbot

# Flash (replace port)
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

Step-by-step flashing guide (Vietnamese): [docs/huong-dan-nap-code.md](docs/huong-dan-nap-code.md)

---

## Wi-Fi Setup (first boot)

1. Hold BOOT button while powering on
2. Connect phone to `xiaozhi-xxxxxx` hotspot
3. Open `192.168.4.1`, enter home Wi-Fi credentials
4. Robot reboots and connects

---

## MCP Tools

The AI can call these tools via the MCP protocol:

| Tool | Parameters | Description |
|------|------------|-------------|
| `self.wheel.move` | direction, speed (0–100), duration_ms | Drive wheels |
| `self.wheel.dance` | name: spin / shake / wave / random | Play dance sequence |
| `self.wheel.stop` | — | Stop motors immediately |
| `self.display.set_mode` | mode: idle / happy / sad / music / talking | Change OLED face |

---

## Project Structure

```
main/
├── application.*              # Main event loop
├── boards/
│   ├── common/                # Board base, Wi-Fi, button, etc.
│   └── wheel-chatbot/         # This board
│       ├── config.h           # GPIO definitions
│       ├── wheel_bot.cc       # Board class
│       ├── wheel_controller.* # Motor driver + MCP tools
│       └── wheel_face_display.* # OLED animations
├── audio/                     # Codec, engine, wake word
├── protocols/                 # WebSocket and MQTT/UDP
└── display/                   # OLED, LCD, LVGL
```

---

## Documentation

- [Hardware & Wiring](main/boards/wheel-chatbot/HARDWARE.md)
- [Flash Guide (Vietnamese)](docs/huong-dan-nap-code.md)
- [MCP Protocol](docs/mcp-protocol.md)
- [WebSocket Protocol](docs/websocket.md)
- [MQTT + UDP Protocol](docs/mqtt-udp.md)
- [Custom Board Guide](docs/custom-board.md)

---

## License

MIT – free to use, including commercially.
