# ESP32 Smart Pomodoro Desk Clock

## 1. Sơ đồ kết nối chân

### 1.1. OLED 0.96 inch I2C

| OLED | ESP32 DevKit V1 |
| ---- | --------------- |
| VCC  | 3.3V ngoài      |
| GND  | GND chung       |
| SDA  | GPIO21          |
| SCL  | GPIO22          |

---

### 1.2. TFT 1.8 inch SPI 128x160

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
| DC / A0          | GPIO27                 | Data/Command            |
| RES / RST        | GPIO4                 | Reset màn hình          |
| BL / LED         | 3.3V ngoài hoặc GPIO27 | Đèn nền màn hình        |

Ghi chú:

* Nếu muốn màn hình luôn sáng, nối `BL/LED` vào 3.3V ngoài.
* Nếu muốn điều chỉnh độ sáng màn hình, dùng GPIO27 điều khiển qua transistor hoặc MOSFET.
* Không nên cấp dòng đèn nền TFT trực tiếp từ GPIO nếu chưa kiểm tra module đã có điện trở hạn dòng hay chưa.

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

## 2. Các GPIO nên tránh trên ESP32 DevKit V1

| GPIO            | Lý do nên tránh                                  |
| --------------- | ------------------------------------------------ |
| GPIO6 - GPIO11  | Đang dùng cho SPI Flash                          |
| GPIO34 - GPIO39 | Chỉ input, không output, không có pull-up nội bộ |
| GPIO0           | Chân boot, dùng sai có thể không nạp được code   |
| GPIO2           | Chân boot, nên tránh nếu chưa quen               |
| GPIO12          | Ảnh hưởng boot voltage, nên tránh                |
| GPIO15          | Chân boot, nên tránh nếu chưa cần                |

---

## 3. Macro cấu hình chân đề xuất

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




