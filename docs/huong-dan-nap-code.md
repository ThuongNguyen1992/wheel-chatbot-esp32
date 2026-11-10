# Hướng Dẫn Nạp Code Vào ESP32-S3

> Áp dụng cho board **Wheel Chatbot Robot** (ESP32-S3-WROOM-1 N8R2)

---

## Yêu cầu

| Phần mềm | Phiên bản | Tải về |
|----------|-----------|--------|
| ESP-IDF | v6.0.1 trở lên (khuyến nghị v6.1) | https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started |
| Python | 3.8+ | Thường đã có sẵn trên macOS/Linux |
| Git | Bất kỳ | https://git-scm.com |
| Driver USB-UART | CH340 hoặc CP2102 | Xem mục bên dưới |

### Cài driver USB (nếu máy không nhận ESP32)

**macOS:** Thường không cần cài thêm. Nếu không thấy cổng:
- CH340: https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html
- CP2102: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

**Windows:** Tải driver CH340 hoặc CP2102 tương ứng với chip USB trên board.

---

## Bước 1 — Cài đặt ESP-IDF

```bash
# Tạo thư mục và clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone -b release/v6.1 --recursive https://github.com/espressif/esp-idf.git

# Cài đặt tools
cd esp-idf
./install.sh esp32s3

# Kiểm tra
source ~/esp/esp-idf/export.sh
idf.py --version
# Phải hiện: ESP-IDF v6.x.x
```

> **Lưu ý:** Mỗi lần mở terminal mới đều phải chạy `source ~/esp/esp-idf/export.sh` trước khi dùng `idf.py`.

---

## Bước 2 — Tải source code

```bash
cd ~/Documents
git clone https://github.com/ThuongNguyen1992/wheel-chatbot-esp32.git
cd wheel-chatbot-esp32
```

---

## Bước 3 — Kết nối ESP32-S3

1. Cắm cáp **USB-C** vào cổng **USB** trên ESP32-S3-DevKitC-1
2. Mở terminal, kiểm tra cổng:

```bash
# macOS / Linux
ls /dev/cu.*
ls /dev/tty.usb*

# Thường thấy một trong các tên:
# /dev/cu.usbmodem1234
# /dev/cu.wchusbserial1234
# /dev/cu.SLAB_USBtoUART
```

```bash
# Windows (dùng Git Bash hoặc PowerShell)
# Xem trong Device Manager > Ports (COM & LPT)
# Ví dụ: COM3, COM4
```

> Nếu không thấy cổng nào → xem mục **Xử lý lỗi** ở cuối file.

---

## Bước 4 — Build firmware

```bash
cd ~/Documents/wheel-chatbot-esp32

# Nạp môi trường ESP-IDF
source ~/esp/esp-idf/export.sh

# Build board wheel-chatbot
python3 scripts/build.py wheel-chatbot --name WheelChatbot
```

Lần đầu build sẽ mất **5–15 phút** tùy máy. Các lần sau nhanh hơn.

Build thành công sẽ thấy:
```
[100%] Built target app
...
Project build complete.
```

---

## Bước 5 — Nạp firmware (Flash)

```bash
# Thay /dev/cu.usbmodem* bằng cổng thật của bạn
idf.py -p /dev/cu.usbmodem* flash
```

**Windows:**
```bash
idf.py -p COM3 flash
```

Nạp thành công sẽ thấy:
```
Wrote 1234567 bytes... Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

---

## Bước 6 — Xem log (Monitor)

```bash
idf.py -p /dev/cu.usbmodem* monitor
```

Nhấn **Ctrl + ]** để thoát.

Log bình thường sau khi khởi động:
```
I (xxx) WheelChatbot: WheelChatbot board ready
I (xxx) wifi: Connecting to WiFi...
I (xxx) app: Device state: Starting
```

---

## Bước 7 — Cấu hình WiFi (lần đầu)

1. Nhấn nút **BOOT** (GPIO0) khi robot đang ở trạng thái **Starting**
2. Điện thoại/máy tính tìm WiFi tên `xiaozhi-xxxxxx` → kết nối
3. Mở trình duyệt vào `192.168.4.1`
4. Nhập tên WiFi nhà và mật khẩu → Lưu
5. Robot tự khởi động lại và kết nối

---

## Build + Flash một lệnh

```bash
source ~/esp/esp-idf/export.sh
python3 scripts/build.py wheel-chatbot --name WheelChatbot && \
idf.py -p /dev/cu.usbmodem* flash monitor
```

---

## Xử lý lỗi thường gặp

### ❌ Không tìm thấy cổng COM / /dev/cu.*

**Nguyên nhân:** Chưa cài driver hoặc cáp USB chỉ sạc (không có dây data).

**Cách sửa:**
- Thử cáp USB khác (cáp data, không phải cáp sạc nhanh)
- Cài driver CH340 hoặc CP2102 (xem Bước 0)
- macOS: Vào System Settings → Privacy & Security → cho phép extension kernel

---

### ❌ Lỗi khi flash: "Failed to connect to ESP32-S3"

**Cách sửa:** Nhấn **giữ nút BOOT** → cắm USB → thả BOOT → chạy lại lệnh flash.

Hoặc giữ BOOT → nhấn RST → thả RST → thả BOOT → flash lại.

---

### ❌ Lỗi: "Permission denied: /dev/ttyUSB0" (Linux)

```bash
sudo usermod -a -G dialout $USER
# Đăng xuất và đăng nhập lại
```

---

### ❌ Build lỗi: "IDF_PATH is not set"

```bash
source ~/esp/esp-idf/export.sh
# Chạy lại lệnh build
```

---

### ❌ Màn hình OLED không hiện gì

- Kiểm tra dây I2C: SDA=GPIO41, SCL=GPIO42
- Kiểm tra nguồn 3.3V cho OLED
- Xem log monitor, tìm dòng: `I2C init failed` hoặc `WheelFaceDisplay initialized`

---

### ❌ Robot không kết nối WiFi

- Chắc chắn WiFi nhà là băng tần **2.4 GHz** (ESP32 không hỗ trợ 5 GHz)
- Thử cấu hình lại WiFi: nhấn BOOT khi đang ở trạng thái `Starting`
- Xem log: `idf.py monitor` để thấy lý do kết nối thất bại

---

## Tóm tắt lệnh hay dùng

```bash
# Nạp môi trường (bắt buộc mỗi lần mở terminal)
source ~/esp/esp-idf/export.sh

# Build
python3 scripts/build.py wheel-chatbot --name WheelChatbot

# Flash
idf.py -p /dev/cu.usbmodem* flash

# Xem log
idf.py -p /dev/cu.usbmodem* monitor

# Flash + xem log cùng lúc
idf.py -p /dev/cu.usbmodem* flash monitor

# Xóa toàn bộ flash (factory reset)
idf.py -p /dev/cu.usbmodem* erase-flash
```

---

## Liên hệ / Hỗ trợ

- GitHub Issues: https://github.com/ThuongNguyen1992/wheel-chatbot-esp32/issues
- Tài liệu ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/
- Tài liệu phần cứng robot: `main/boards/wheel-chatbot/HARDWARE.md`
