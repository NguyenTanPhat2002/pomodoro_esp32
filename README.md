# ESP32 Smart Pomodoro Desk Clock

Dự án đồng hồ để bàn thông minh sử dụng **ESP32 DevKit V1**, kết hợp **OLED 0.96 inch I2C** và **TFT SPI 1.8 inch 128x160**.

Thiết bị được thiết kế để hỗ trợ học tập/làm việc với các chức năng chính như:

* Hiển thị thời gian thực
* Hiển thị thứ, ngày, tháng, năm
* Hiển thị trạng thái WiFi
* Hiển thị thời tiết lấy từ Internet
* Chế độ Pomodoro
* Timer
* Stopwatch
* Alarm
* Thống kê thời gian tập trung
* Cài đặt thiết bị
* Cấu hình lại WiFi bằng Web Config Portal

---

## 1. Tổng quan hệ thống

Dự án sử dụng 2 màn hình với vai trò riêng biệt:

### OLED 0.96 inch

OLED là màn hình phụ, luôn hiển thị các thông tin cơ bản:

* Giờ hiện tại
* Thứ, ngày, tháng, năm
* Icon trạng thái WiFi
* Có thể mở rộng thêm trạng thái Pomodoro như `FOCUS`, `BREAK`, `IDLE`

Ví dụ hiển thị:

```text
15:42:08
T3 16-06-2026
```

---

### TFT 1.8 inch SPI 128x160

TFT là màn hình chính, dùng để hiển thị menu và các chức năng tương tác:

* Weather Mode
* Pomodoro Mode
* Timer Mode
* Stopwatch Mode
* Alarm Mode
* Statistics Mode
* Settings Mode
* WiFi Config Mode

Ví dụ menu chính:

```text
MAIN MENU

> Weather
  Pomodoro
  Timer
  Stopwatch
  Alarm
  Statistics
  Settings
```

---

## 2. Chức năng của dự án

### 2.1. Clock Display

Chức năng hiển thị thời gian thực trên OLED.

Thông tin hiển thị:

* Giờ: phút: giây
* Thứ trong tuần
* Ngày, tháng, năm
* Trạng thái WiFi

Dữ liệu thời gian được lấy từ Internet thông qua NTP sau khi ESP32 kết nối WiFi.

Ví dụ:

```text
16:30:25
T4 17-06-2026
```

---

### 2.2. Weather Mode

Chức năng hiển thị thời tiết trên màn TFT.

Thông tin dự kiến hiển thị:

* Tên khu vực/thành phố
* Nhiệt độ hiện tại
* Trạng thái thời tiết
* Độ ẩm
* Tốc độ gió
* Lượng mưa
* Thời điểm cập nhật cuối

Ví dụ giao diện:

```text
WEATHER

Ha Noi

31.2 C
Nhieu may

Humidity: 70%
Wind: 8.4 km/h
Rain: 0.0 mm

Update: 16:30
```

Chức năng xử lý:

* ESP32 gửi HTTP request tới Weather API
* Nhận dữ liệu dạng JSON
* Parse các thông tin cần thiết
* Hiển thị dữ liệu lên TFT
* Tự động cập nhật theo chu kỳ
* Nếu mất WiFi thì hiển thị `WiFi Lost`
* Nếu lấy dữ liệu thất bại thì hiển thị `Update Fail`

---

### 2.3. Pomodoro Mode

Chế độ hỗ trợ tập trung học tập/làm việc.

Các trạng thái chính:

```text
IDLE   : chưa chạy
FOCUS  : đang tập trung
BREAK  : đang nghỉ
PAUSE  : tạm dừng
FINISH : hoàn thành phiên
```

Thông tin hiển thị trên TFT:

```text
POMODORO

FOCUS
24:32

Session: 1/4

Press: Pause
Hold : Reset
```

Chức năng:

* Cài đặt thời gian tập trung
* Cài đặt thời gian nghỉ ngắn
* Cài đặt thời gian nghỉ dài
* Start / Pause / Resume / Reset
* Tự động chuyển từ Focus sang Break
* Tự động chuyển từ Break sang phiên Focus tiếp theo
* Buzzer báo khi hết phiên
* Hiển thị số phiên đã hoàn thành
* Hiển thị progress bar cho phiên hiện tại

---

### 2.4. Timer Mode

Chế độ đếm ngược tự do.

Ví dụ giao diện:

```text
TIMER

Set: 10 min
09:42

Press: Start
Hold : Reset
```

Chức năng:

* Cài đặt thời gian đếm ngược
* Start / Pause / Resume / Reset
* Buzzer báo khi hết giờ
* Có thể dùng cho học tập, nghỉ ngơi, nấu ăn hoặc các công việc cần hẹn giờ

---

### 2.5. Stopwatch Mode

Chế độ bấm giờ.

Ví dụ giao diện:

```text
STOPWATCH

00:03:28

Press: Start/Pause
Hold : Reset
```

Chức năng:

* Bắt đầu bấm giờ
* Tạm dừng
* Tiếp tục
* Reset thời gian

---

### 2.6. Alarm Mode

Chế độ báo thức.

Ví dụ giao diện:

```text
ALARM

07:00
Status: ON

Press to edit
```

Chức năng:

* Cài đặt giờ báo thức
* Bật/tắt báo thức
* Buzzer báo khi đến giờ
* Hiển thị trạng thái báo thức trên TFT

---

### 2.7. Statistics Mode

Chức năng thống kê thời gian tập trung.

Ví dụ giao diện:

```text
TODAY

Focus: 3 sessions
Time : 75 min
Break: 20 min
```

Thông tin có thể thống kê:

* Số phiên Pomodoro đã hoàn thành trong ngày
* Tổng thời gian tập trung
* Tổng thời gian nghỉ
* Số ngày sử dụng liên tiếp
* Tổng số phiên đã hoàn thành

Dữ liệu có thể được lưu vào NVS Flash của ESP32.

---

### 2.8. Settings Mode

Chế độ cài đặt thiết bị.

Các mục cài đặt dự kiến:

```text
SETTINGS

1. Focus time
2. Break time
3. Long break
4. Buzzer
5. Brightness
6. City
7. WiFi
```

Các thông số có thể lưu:

* Thời gian Focus
* Thời gian Break
* Thời gian Long Break
* Số phiên trước khi nghỉ dài
* Bật/tắt buzzer
* Độ sáng màn TFT
* Thành phố lấy thời tiết
* Chu kỳ cập nhật thời tiết
* Thông tin WiFi

Các thông số cấu hình nên được lưu vào NVS để sau khi tắt nguồn bật lại, thiết bị vẫn giữ cấu hình cũ.

---

### 2.9. WiFi Config Mode

Chế độ cấu hình lại WiFi trực tiếp trên thiết bị mà không cần nạp lại firmware.

Khi người dùng chọn:

```text
Settings -> WiFi Config -> Change WiFi
```

ESP32 sẽ chuyển từ Station mode sang Access Point mode và tạo một WiFi nội bộ để người dùng kết nối.

Ví dụ thông tin hiển thị trên TFT:

```text
WIFI CONFIG

AP STARTED

Connect to:
PomodoroClock_AP

Open:
192.168.4.1
```

Người dùng dùng điện thoại hoặc laptop kết nối vào WiFi do ESP32 phát ra, sau đó mở trình duyệt và truy cập:

```text
http://192.168.4.1
```

Trang web cấu hình cho phép nhập:

* SSID mới
* Password mới

Sau khi người dùng bấm `Save`, ESP32 sẽ thử kết nối tới WiFi mới.

Luồng xử lý:

```text
1. Người dùng chọn Change WiFi trên TFT.
2. ESP32 chuyển sang AP mode.
3. ESP32 tạo WiFi cấu hình, ví dụ: PomodoroClock_AP.
4. TFT hiển thị tên WiFi AP và địa chỉ web 192.168.4.1.
5. Người dùng kết nối điện thoại/laptop vào WiFi của ESP32.
6. Người dùng truy cập 192.168.4.1.
7. Người dùng nhập SSID/PASSWORD mới.
8. ESP32 nhận thông tin WiFi mới.
9. ESP32 chuyển lại Station mode.
10. ESP32 thử kết nối WiFi mới.
11. Nếu kết nối thành công, ESP32 lưu SSID/PASSWORD mới vào NVS.
12. Nếu kết nối thất bại, ESP32 quay lại AP mode để người dùng nhập lại.
```

Nguyên tắc an toàn:

* Không lưu đè WiFi mới ngay lập tức
* Chỉ lưu SSID/PASSWORD mới vào NVS sau khi đã kết nối thành công
* Nếu WiFi mới sai, thiết bị quay lại AP mode để cấu hình lại
* Trạng thái cấu hình được hiển thị trên TFT để người dùng dễ thao tác

---

## 3. Phần cứng sử dụng

| Thiết bị                 | Chức năng                                            |
| ------------------------ | ---------------------------------------------------- |
| ESP32 DevKit V1          | Vi điều khiển chính                                  |
| OLED 0.96 inch I2C       | Hiển thị giờ, ngày, trạng thái WiFi                  |
| TFT 1.8 inch SPI 128x160 | Màn hình chính để hiển thị menu, thời tiết, Pomodoro |
| Rotary Encoder           | Điều khiển menu, chọn chức năng, tăng/giảm giá trị   |
| Buzzer                   | Báo âm khi hết Pomodoro, Timer hoặc Alarm            |
| Nguồn 5V ngoài           | Cấp nguồn chính cho hệ thống                         |
| Mạch hạ áp 3.3V          | Cấp nguồn riêng cho OLED, TFT, Encoder               |
| Dây jumper               | Kết nối module                                       |
| Breadboard hoặc PCB      | Lắp mạch thử nghiệm hoặc hoàn thiện                  |

---

## 4. Sơ đồ kết nối chân

### 4.1. OLED 0.96 inch I2C

| OLED | ESP32 DevKit V1 |
| ---- | --------------- |
| VCC  | 3.3V ngoài      |
| GND  | GND chung       |
| SDA  | GPIO21          |
| SCL  | GPIO22          |

---

### 4.2. TFT 1.8 inch SPI 128x160

Màn TFT thường có các chân như:

```text
VCC
GND
SCL / SCK / CLK
SDA / MOSI / DIN
RES / RST
DC / A0
CS
BL / LED
```

Bảng kết nối đề xuất:

| TFT              | ESP32 DevKit V1        | Ghi chú                 |
| ---------------- | ---------------------- | ----------------------- |
| VCC              | 3.3V ngoài             | Nguồn cho màn TFT       |
| GND              | GND chung              | Bắt buộc nối chung mass |
| SCL / SCK / CLK  | GPIO18                 | SPI Clock               |
| SDA / MOSI / DIN | GPIO23                 | SPI MOSI                |
| CS               | GPIO14                 | Chip Select             |
| DC / A0          | GPIO16                 | Data/Command            |
| RES / RST        | GPIO17                 | Reset màn hình          |
| BL / LED         | 3.3V ngoài hoặc GPIO27 | Đèn nền màn hình        |

Ghi chú:

* Nếu muốn màn hình luôn sáng, nối `BL/LED` vào 3.3V ngoài.
* Nếu muốn điều chỉnh độ sáng màn hình, dùng GPIO27 điều khiển qua transistor hoặc MOSFET.
* Không nên cấp dòng đèn nền TFT trực tiếp từ GPIO nếu chưa kiểm tra module đã có điện trở hạn dòng hay chưa.

---

### 4.3. Rotary Encoder

| Encoder | ESP32 DevKit V1 | Ghi chú          |
| ------- | --------------- | ---------------- |
| CLK / A | GPIO32          | Tín hiệu xoay    |
| DT / B  | GPIO33          | Tín hiệu xoay    |
| SW      | GPIO25          | Nút nhấn encoder |
| VCC / + | 3.3V ngoài      | Nguồn encoder    |
| GND     | GND chung       | Mass chung       |

Ghi chú:

* Các chân `CLK`, `DT`, `SW` nên bật Pull-up trong code.
* Encoder dùng để di chuyển menu, chọn chức năng và thay đổi giá trị cài đặt.

---

### 4.4. Buzzer

Nếu dùng buzzer module 3 chân:

| Buzzer Module | ESP32 DevKit V1         |
| ------------- | ----------------------- |
| VCC           | 5V hoặc 3.3V tùy module |
| GND           | GND chung               |
| SIG / I/O     | GPIO26                  |

Nếu dùng buzzer 2 chân:

* Nên điều khiển qua transistor
* Không nên cấp trực tiếp buzzer dòng lớn từ GPIO của ESP32

Sơ đồ nguyên lý đơn giản với transistor NPN:

```text
GPIO26 ---- R 1k ---- B transistor
C transistor -------- chân âm buzzer
chân dương buzzer --- 5V
E transistor -------- GND
```

---

## 5. Bảng GPIO tổng hợp

```text
OLED I2C:
SDA  -> GPIO21
SCL  -> GPIO22

TFT SPI:
SCK  -> GPIO18
MOSI -> GPIO23
CS   -> GPIO14
DC   -> GPIO16
RST  -> GPIO17
BL   -> GPIO27 hoặc 3.3V ngoài

Rotary Encoder:
CLK  -> GPIO32
DT   -> GPIO33
SW   -> GPIO25

Buzzer:
SIG  -> GPIO26
```

---

## 6. Sơ đồ nguồn khuyến nghị

Không nên cấp toàn bộ OLED, TFT, Encoder và Buzzer từ chân `3V3` của ESP32 DevKit V1.

Sơ đồ nguồn khuyến nghị:

```text
Adapter 5V 1A/2A
│
├── VIN/5V của ESP32
│
├── Buzzer module nếu dùng loại 5V
│
└── Mạch hạ áp 3.3V riêng
    │
    ├── OLED VCC
    ├── TFT VCC
    ├── Encoder VCC
    └── LED nhỏ nếu có
```

Tất cả GND phải nối chung:

```text
GND nguồn ngoài
│
├── GND ESP32
├── GND OLED
├── GND TFT
├── GND Encoder
└── GND Buzzer
```

Lưu ý:

* ESP32 dùng WiFi nên dòng tiêu thụ có thể tăng cao trong lúc truyền nhận dữ liệu.
* TFT có đèn nền nên tiêu thụ dòng lớn hơn OLED.
* Buzzer có thể gây sụt áp nếu cấp nguồn không đủ.
* Nên dùng nguồn 5V ngoài đủ dòng để hệ thống ổn định.

---

## 7. Các GPIO nên tránh trên ESP32 DevKit V1

| GPIO            | Lý do nên tránh                                  |
| --------------- | ------------------------------------------------ |
| GPIO6 - GPIO11  | Đang dùng cho SPI Flash                          |
| GPIO34 - GPIO39 | Chỉ input, không output, không có pull-up nội bộ |
| GPIO0           | Chân boot, dùng sai có thể không nạp được code   |
| GPIO2           | Chân boot, nên tránh nếu chưa quen               |
| GPIO12          | Ảnh hưởng boot voltage, nên tránh                |
| GPIO15          | Chân boot, nên tránh nếu chưa cần                |

---

## 8. Macro cấu hình chân đề xuất

```c
#define OLED_I2C_PORT      I2C_NUM_0
#define OLED_SDA_PIN       GPIO_NUM_21
#define OLED_SCL_PIN       GPIO_NUM_22
#define OLED_ADDR          0x3C

#define TFT_SPI_HOST       SPI2_HOST
#define TFT_SCK_PIN        GPIO_NUM_18
#define TFT_MOSI_PIN       GPIO_NUM_23
#define TFT_CS_PIN         GPIO_NUM_14
#define TFT_DC_PIN         GPIO_NUM_16
#define TFT_RST_PIN        GPIO_NUM_17
#define TFT_BL_PIN         GPIO_NUM_27

#define ENCODER_CLK_PIN    GPIO_NUM_32
#define ENCODER_DT_PIN     GPIO_NUM_33
#define ENCODER_SW_PIN     GPIO_NUM_25

#define BUZZER_PIN         GPIO_NUM_26
```

---

## 9. Phần mềm và công nghệ sử dụng

### Môi trường phát triển

| Phần mềm          | Chức năng                                 |
| ----------------- | ----------------------------------------- |
| ESP-IDF           | Framework lập trình ESP32                 |
| VS Code           | IDE lập trình                             |
| ESP-IDF Extension | Hỗ trợ build, flash, monitor trên VS Code |
| Git               | Quản lý source code                       |
| GitHub            | Lưu trữ và quản lý project                |

---

### Thành phần phần mềm trong firmware

| Thành phần        | Chức năng                                  |
| ----------------- | ------------------------------------------ |
| FreeRTOS          | Quản lý task, timer, queue                 |
| WiFi Station      | Kết nối ESP32 vào mạng WiFi                |
| WiFi Access Point | Phát WiFi nội bộ để cấu hình lại WiFi      |
| SNTP/NTP          | Lấy thời gian thực từ Internet             |
| HTTP Client       | Gửi request lấy dữ liệu thời tiết          |
| HTTP Server       | Tạo trang web cấu hình WiFi                |
| JSON Parser       | Parse dữ liệu thời tiết                    |
| NVS Flash         | Lưu cấu hình WiFi và các cài đặt           |
| I2C Driver        | Giao tiếp OLED                             |
| SPI Driver        | Giao tiếp TFT                              |
| GPIO Driver       | Đọc Encoder, điều khiển Buzzer             |
| LEDC PWM          | Điều khiển buzzer hoặc độ sáng TFT nếu cần |

---

## 10. Cấu trúc source code đề xuất

```text
main/
├── app_main.c
├── oled_clock.c
├── oled_clock.h
├── tft_st7735.c
├── tft_st7735.h
├── weather_api.c
├── weather_api.h
├── pomodoro.c
├── pomodoro.h
├── timer_mode.c
├── timer_mode.h
├── stopwatch.c
├── stopwatch.h
├── alarm.c
├── alarm.h
├── statistics.c
├── statistics.h
├── ui_menu.c
├── ui_menu.h
├── encoder.c
├── encoder.h
├── buzzer.c
├── buzzer.h
├── wifi_manager.c
├── wifi_manager.h
├── wifi_config_portal.c
├── wifi_config_portal.h
├── app_config.c
├── app_config.h
```

Vai trò từng module:

| File                      | Chức năng                               |
| ------------------------- | --------------------------------------- |
| `app_main.c`              | Khởi tạo hệ thống, quản lý luồng chính  |
| `oled_clock.c/.h`         | Hiển thị giờ, ngày, icon WiFi trên OLED |
| `tft_st7735.c/.h`         | Driver màn TFT SPI                      |
| `weather_api.c/.h`        | Lấy và xử lý dữ liệu thời tiết          |
| `pomodoro.c/.h`           | Xử lý logic Pomodoro                    |
| `timer_mode.c/.h`         | Xử lý chế độ Timer                      |
| `stopwatch.c/.h`          | Xử lý chế độ Stopwatch                  |
| `alarm.c/.h`              | Xử lý báo thức                          |
| `statistics.c/.h`         | Lưu và hiển thị thống kê tập trung      |
| `ui_menu.c/.h`            | Quản lý menu trên TFT                   |
| `encoder.c/.h`            | Đọc Rotary Encoder                      |
| `buzzer.c/.h`             | Điều khiển buzzer                       |
| `wifi_manager.c/.h`       | Quản lý WiFi Station/AP                 |
| `wifi_config_portal.c/.h` | Web cấu hình WiFi                       |
| `app_config.c/.h`         | Lưu/đọc cấu hình từ NVS                 |

---

## 11. Giao diện menu dự kiến

### Main Menu

```text
MAIN MENU

> Weather
  Pomodoro
  Timer
  Stopwatch
  Alarm
  Statistics
  Settings
```

---

### Settings Menu

```text
SETTINGS

> Focus time
  Break time
  Long break
  Buzzer
  Brightness
  City
  WiFi
```

---

### WiFi Config Menu

```text
WIFI CONFIG

> Current WiFi
  Change WiFi
  Back
```

Trong đó:

```text
Current WiFi:
- Hiển thị SSID hiện tại
- Hiển thị trạng thái Connected/Disconnected
- Hiển thị IP nếu đã kết nối

Change WiFi:
- Chuyển ESP32 sang AP mode
- Mở web cấu hình tại 192.168.4.1

Back:
- Quay lại Settings
```

---

## 12. Nguyên tắc hoạt động

### Khi khởi động

```text
1. ESP32 khởi tạo NVS.
2. ESP32 đọc WiFi SSID/PASSWORD đã lưu.
3. ESP32 kết nối WiFi ở Station mode.
4. Nếu kết nối thành công, ESP32 lấy giờ NTP.
5. OLED hiển thị giờ, ngày, trạng thái WiFi.
6. TFT hiển thị menu chính hoặc mode mặc định.
```

---

### Khi mất WiFi

```text
1. OLED tắt hoặc đổi trạng thái icon WiFi.
2. TFT có thể hiển thị WiFi Lost.
3. ESP32 thử kết nối lại WiFi.
4. Các chức năng offline như Pomodoro, Timer, Stopwatch vẫn hoạt động.
5. Weather Mode tạm dừng cập nhật cho đến khi có WiFi lại.
```

---

### Khi đổi WiFi

```text
1. Người dùng vào Settings -> WiFi -> Change WiFi.
2. ESP32 chuyển sang AP mode.
3. TFT hiển thị tên AP và địa chỉ 192.168.4.1.
4. Người dùng kết nối điện thoại/laptop vào AP của ESP32.
5. Người dùng nhập SSID/PASSWORD mới trên web.
6. ESP32 thử kết nối WiFi mới.
7. Nếu thành công, ESP32 lưu WiFi mới vào NVS.
8. Nếu thất bại, ESP32 quay lại AP mode để nhập lại.
```

---

## 13. Ghi chú kỹ thuật

* OLED dùng giao tiếp I2C.
* TFT dùng giao tiếp SPI.
* Rotary Encoder dùng GPIO input có pull-up.
* Buzzer nên được điều khiển qua transistor nếu dùng loại tiêu thụ dòng lớn.
* Không nên cấp toàn bộ module từ chân 3V3 của ESP32.
* Tất cả module phải nối chung GND.
* Các cấu hình quan trọng nên lưu vào NVS.
* Weather Mode cần WiFi.
* Pomodoro, Timer, Stopwatch có thể hoạt động không cần WiFi.
* Alarm phụ thuộc vào thời gian hệ thống; nếu không có RTC, thiết bị cần lấy giờ NTP sau khi khởi động.
