# Tài liệu Phần Cứng – Wheel Chatbot Robot

> ESP32-S3 AI Robot có thể nói chuyện, di chuyển và hiển thị cảm xúc qua màn hình OLED.

---

## 1. Danh sách linh kiện (BOM)

| Ký hiệu | Linh kiện | Mô tả |
|---------|-----------|-------|
| U1 | ESP32-S3-WROOM-1 N8R2 | Vi điều khiển chính, 8MB Flash, 2MB PSRAM |
| U2 | INMP441 | Micro MEMS I2S, thu tiếng nói |
| U3 | MAX98357A | Khuếch đại âm thanh I2S, 3W |
| U4 | SSD1306 OLED 0.96" | Màn hình OLED 128×64, I2C |
| U5 | L298N | Module điều khiển động cơ DC đôi |
| U6 | TP4056 BMS | Mạch sạc và bảo vệ pin 18650 |
| M1 | Động cơ TT DC | Bánh xe trái (với hộp giảm tốc) |
| M2 | Động cơ TT DC | Bánh xe phải (với hộp giảm tốc) |
| SP1 | Loa nhỏ 8Ω | Loa phát âm thanh (kết nối MAX98357A) |
| SW1 | Công tắc nguồn | Bật/tắt toàn bộ mạch |
| SW2 | Nút bấm | Nút BOOT (GPIO0), cũng dùng để vào WiFi |
| SW3 | Cảm biến cảm ứng | Module TOUCH điện dung |
| BZ1 | Còi thụ động | Phát âm thanh đơn giản qua PWM |
| ENC1 | Encoder quay | Núm xoay có nút bấm, điều chỉnh âm lượng |
| LED1 | LED đơn trái | Đèn báo trạng thái bên trái |
| LED2 | LED đơn phải | Đèn báo trạng thái bên phải |
| BAT | 4× 18650 | Pin lithium 3.7V, ghép 2S2P ≈ 7.4V 6000mAh |
| R1-R2 | Điện trở 220Ω | Hạn dòng cho LED1, LED2 |
| R3 | Điện trở 100Ω | Nối tiếp buzzer |

---

## 2. Sơ đồ chân GPIO

### Phía trái ESP32-S3 (từ trên xuống)

| GPIO | Chức năng | Kết nối |
|------|-----------|---------|
| GPIO4 | I2S MIC WS (LRCLK) | INMP441 chân WS |
| GPIO5 | I2S MIC SCK (BCK) | INMP441 chân SCK |
| GPIO6 | I2S MIC DIN | INMP441 chân SD |
| GPIO7 | Buzzer PWM | BZ1 (qua R3 100Ω) |
| GPIO8 | Touch sensor | SW3 chân OUT |
| GPIO9 | Encoder CLK | ENC1 chân A |
| GPIO10 | Encoder DT | ENC1 chân B |
| GPIO11 | Encoder SW | ENC1 nút bấm |
| GPIO12 | LED trái | LED1 (qua R1 220Ω) |
| GPIO15 | I2S SPK BCLK | MAX98357A chân BCLK |
| GPIO16 | I2S SPK LRCK | MAX98357A chân LRC |
| GPIO17 | I2S SPK DOUT | MAX98357A chân DIN |

### Phía phải ESP32-S3 (từ trên xuống)

| GPIO | Chức năng | Kết nối |
|------|-----------|---------|
| GPIO42 | I2C SCL | OLED chân SCL |
| GPIO41 | I2C SDA | OLED chân SDA |
| GPIO37 | Motor R IN4 | L298N chân IN4 |
| GPIO36 | Motor R IN3 | L298N chân IN3 |
| GPIO35 | Motor R ENB (PWM) | L298N chân ENB |
| GPIO0 | Nút BOOT | Nút SW2 → GND |
| GPIO48 | RGB LED onboard | WS2812 tích hợp trên kit |
| GPIO47 | LED phải | LED2 (qua R2 220Ω) |
| GPIO21 | Motor L IN2 | L298N chân IN2 |
| GPIO20 | Motor L IN1 | L298N chân IN1 |
| GPIO19 | Motor L ENA (PWM) | L298N chân ENA |

### Nguồn điện

| Kết nối | Mô tả |
|---------|-------|
| 5V (Vin) | Từ BMS/USB vào ESP32 |
| 3.3V (3V3) | Ra cho OLED, MIC, cảm biến (tối đa 500mA) |
| VCC L298N | 7.4V từ 2S pin 18650 (trực tiếp, không qua BMS) |
| GND chung | Nối tất cả GND lại với nhau |

---

## 3. Hướng dẫn đấu dây

### INMP441 (Micro) → ESP32
```
INMP441   →   ESP32
VDD       →   3.3V
GND       →   GND
L/R       →   GND  (chọn kênh trái)
WS        →   GPIO4
SCK       →   GPIO5
SD        →   GPIO6
```

### MAX98357A (Ampli) → ESP32 & Loa
```
MAX98357A →   ESP32
VIN       →   3.3V
GND       →   GND
BCLK      →   GPIO15
LRC       →   GPIO16
DIN       →   GPIO17
GAIN      →   (để hở = gain 9dB, hoặc nối 100k xuống GND = 12dB)

MAX98357A →   Loa SP1
OUT+      →   Loa (+)
OUT-      →   Loa (-)
```

### OLED SSD1306 → ESP32
```
OLED  →   ESP32
VCC   →   3.3V
GND   →   GND
SCL   →   GPIO42
SDA   →   GPIO41
```

### L298N Motor Driver → ESP32 & Nguồn & Động cơ
```
L298N  →   ESP32
ENA    →   GPIO19  (PWM tốc độ bánh trái)
IN1    →   GPIO20  (chiều bánh trái)
IN2    →   GPIO21  (chiều bánh trái)
ENB    →   GPIO35  (PWM tốc độ bánh phải)
IN3    →   GPIO36  (chiều bánh phải)
IN4    →   GPIO37  (chiều bánh phải)

L298N  →   Nguồn
VCC    →   7.4V từ pin 18650 (không qua BMS)
GND    →   GND chung

L298N  →   Động cơ
OUT1-2 →   Motor M1 (bánh trái)
OUT3-4 →   Motor M2 (bánh phải)
```

### Cảm biến cảm ứng (Touch) → ESP32
```
TOUCH  →   ESP32
VCC    →   3.3V
GND    →   GND
OUT    →   GPIO8
```

### Encoder quay → ESP32
```
ENCODER  →   ESP32
VCC      →   3.3V
GND      →   GND
CLK (A)  →   GPIO9
DT  (B)  →   GPIO10
SW       →   GPIO11 (nút bấm encoder)
```

### Còi (Buzzer) → ESP32
```
Buzzer   →   ESP32
(+)      →   GPIO7 → R3 100Ω → Buzzer → GND
(-)      →   GND
```
*Lưu ý: Đây là còi thụ động (passive buzzer), cần tín hiệu PWM.*

### LED → ESP32
```
LED trái:   GPIO12 → R1 220Ω → LED1(+) → LED1(-) → GND
LED phải:   GPIO47 → R2 220Ω → LED2(+) → LED2(-) → GND
```

### Nút bấm BOOT → ESP32
```
SW2: GPIO0 → Nút → GND
(GPIO0 có điện trở kéo lên nội bộ 10kΩ)
```

### Nguồn điện
```
4× 18650 (2S2P) → BMS TP4056 → Công tắc SW1 → Phân phối nguồn:
  → 5V: ESP32 Vin, L298N 5V logic
  → 7.4V: L298N VCC motor (trực tiếp từ pin, không qua BMS 5V)

Lưu ý: Dùng BMS 2S (7.4V) với cân bằng cell để an toàn.
        L298N cần áp ≥ 6V cho motor.
```

---

## 4. Cấu trúc code

```
main/boards/wheel-chatbot/
├── config.h                  ← Định nghĩa tất cả chân GPIO
├── wheel_bot.cc              ← Board class chính (WheelChatbot)
├── wheel_controller.h/.cc    ← Điều khiển động cơ + MCP tools
└── wheel_face_display.h/.cc  ← Màn hình OLED với 5 chế độ khuôn mặt
```

### config.h
Chứa tất cả `#define` cho GPIO và cấu hình. Thay đổi ở đây thay vì sửa trực tiếp trong code.

### wheel_bot.cc (WheelChatbot class)
- `InitializeAudioCodec()`: Khởi tạo I2S mic (GPIO4-6) và speaker (GPIO15-17)
- `InitializeDisplay()`: Khởi tạo OLED I2C (GPIO41-42), tạo WheelFaceDisplay
- `InitializeButtons()`: Nút BOOT (GPIO0) → bật/tắt chat, vào WiFi config
- `InitializeWheelController()`: Motor driver (GPIO19-21, 35-37) + MCP tools
- `RegisterDisplayMcpTools()`: Cho AI gọi `self.display.set_mode`

### wheel_controller.cc (WheelController class)
Điều khiển động cơ qua L298N:
- LEDC PWM cho ENA (GPIO19) và ENB (GPIO35)
- Digital output cho IN1-IN4 (GPIO20-21, GPIO36-37)
- FreeRTOS queue 8 slot cho các lệnh di chuyển
- **MCP Tools:**
  - `self.wheel.move`: Tiến/lùi/trái/phải với tốc độ và thời gian
  - `self.wheel.dance`: Các điệu nhảy mẫu (spin, shake, wave)
  - `self.wheel.stop`: Dừng ngay lập tức

### wheel_face_display.cc (WheelFaceDisplay class)
Màn hình OLED 128×64 với animation 10fps:
- Dải trạng thái trên (16px): WiFi, trạng thái
- Vùng mặt dưới (48px): Khuôn mặt animation

| Chế độ | Mô tả | Khi nào dùng |
|--------|-------|---------------|
| `idle` | Mặt bình thường, chớp mắt | Chờ lệnh |
| `talking` | Miệng mở/đóng | AI đang nói |
| `happy` | Mắt nhảy, mồm cười | Vui vẻ |
| `sad` | Mồm cúp xuống | Buồn/lỗi |
| `music` | Thanh EQ nhảy | Phát nhạc |

---

## 5. MCP Tools (AI điều khiển)

AI (Claude/GPT qua WebSocket) có thể gọi các công cụ sau:

| Tool | Tham số | Mô tả |
|------|---------|-------|
| `self.wheel.move` | direction, speed (0-100), duration_ms | Điều khiển di chuyển |
| `self.wheel.dance` | name (spin/shake/wave/random) | Thực hiện điệu nhảy |
| `self.wheel.stop` | — | Dừng động cơ |
| `self.display.set_mode` | mode (idle/happy/sad/music/talking) | Đổi chế độ màn hình |

---

## 6. Build và Flash

### Yêu cầu
- ESP-IDF v6.0.1+ (release/v6.1 khuyến nghị)
- Python 3.8+

### Cài đặt môi trường
```bash
# Nguồn ESP-IDF (thay đường dẫn phù hợp)
source ~/esp/esp-idf/export.sh
idf.py --version  # Phải hiện v6.x
```

### Build và Flash
```bash
# Di chuyển vào thư mục project
cd ~/Documents/Personal\ Documents/ESP/xiaozhi-esp32

# Build cho board wheel-chatbot
python3 scripts/build.py wheel-chatbot --name WheelChatbot

# Flash vào ESP32-S3
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

### Cấu hình WiFi
Sau khi flash:
1. Nhấn nút BOOT (GPIO0) khi robot đang ở trạng thái `Starting`
2. Kết nối WiFi `xiaozhi-xxxxxx` từ điện thoại
3. Truy cập `192.168.4.1` để nhập SSID và mật khẩu
4. Robot khởi động lại và kết nối WiFi

---

## 7. Lưu ý quan trọng

1. **GPIO35, 36, 37**: Chỉ có trên ESP32-S3-DevKitC-1 N8R2 (38-pin). Không có trên các kit ít chân hơn.
2. **GPIO48**: Là RGB LED onboard trên DevKit — **không dùng** cho mục đích khác.
3. **L298N VCC**: Nối thẳng vào pin 7.4V, **không** qua bộ điều chỉnh 5V. L298N drop ~2V nên motor nhận ~5.4V.
4. **GND chung**: Tất cả GND (ESP32, L298N, cảm biến, pin) phải nối chung một điểm.
5. **Dòng điện**: INMP441 cần 3.3V ổn định. Dùng tụ lọc 100nF gần VDD nếu có nhiễu.
6. **Kháng EMI**: Có thể nối tụ 100nF song song với mỗi động cơ để giảm nhiễu.
