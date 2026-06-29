#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_http_server.h"


#include "cJSON.h"

/* ================= TFT ST7735 PIN CONFIG =================
   TFT 1.8 SPI 128x160 V1.1:
   SCK  -> GPIO18
   MOSI -> GPIO23
   CS   -> GPIO14
   DC   -> GPIO27
   RST  -> GPIO4
   BL   -> 3.3V ngoài
*/
#define TFT_SPI_HOST    SPI3_HOST
#define TFT_SCK_PIN     GPIO_NUM_18
#define TFT_MOSI_PIN    GPIO_NUM_23
#define TFT_CS_PIN      GPIO_NUM_14
#define TFT_DC_PIN      GPIO_NUM_27
#define TFT_RST_PIN     GPIO_NUM_4

#define TFT_WIDTH       160
#define TFT_HEIGHT      128

/* Bắc Ninh */
#define WEATHER_URL "https://api.open-meteo.com/v1/forecast?latitude=21.1861&longitude=106.0763&current=temperature_2m,relative_humidity_2m,precipitation,weather_code,wind_speed_10m&timezone=Asia%2FBangkok"

#define WIFI_SSID "PhatBinh"
#define WIFI_PASS "19752002"

#define I2C_PORT    I2C_NUM_0
#define OLED_ADDR   0x3C
#define SDA_PIN     21
#define SCL_PIN     22

#define HABIT_DEFAULT_TASK "KHONG CHOI GAME"
#define HABIT_MAX_TASK_LEN 32
#define HABIT_TARGET_DAYS 30

static const char *TAG = "CLOCK_WEATHER";

static volatile bool g_wifi_connected = false;
static char g_ip_addr[16] = "0.0.0.0";

static char g_habit_task[HABIT_MAX_TASK_LEN] = HABIT_DEFAULT_TASK;
static time_t g_habit_start_epoch = 0;
static int32_t g_habit_reset_count = 0;

/* ================= OLED BASIC ================= */

static void oled_cmd(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};
    i2c_master_write_to_device(I2C_PORT, OLED_ADDR, data, 2, pdMS_TO_TICKS(100));
}

static void oled_data(const uint8_t *data, size_t len)
{
    if (len > 128) {
        return;
    }

    uint8_t buf[129];
    buf[0] = 0x40;
    memcpy(&buf[1], data, len);

    i2c_master_write_to_device(I2C_PORT, OLED_ADDR, buf, len + 1, pdMS_TO_TICKS(100));
}

static void oled_set_cursor(uint8_t page, uint8_t col)
{
    oled_cmd(0xB0 + page);
    oled_cmd(0x00 + (col & 0x0F));
    oled_cmd(0x10 + ((col >> 4) & 0x0F));
}

static void oled_clear_area(uint8_t page, uint8_t col, uint8_t width)
{
    if (page >= 8 || col >= 128) {
        return;
    }

    if ((col + width) > 128) {
        width = 128 - col;
    }

    uint8_t zero[128];
    memset(zero, 0x00, width);

    oled_set_cursor(page, col);
    oled_data(zero, width);
}

static void oled_clear_page(uint8_t page)
{
    oled_clear_area(page, 0, 128);
}

static void oled_clear(void)
{
    for (int page = 0; page < 8; page++) {
        oled_clear_page(page);
    }
}

static void oled_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    oled_cmd(0xAE);
    oled_cmd(0x20); oled_cmd(0x00);
    oled_cmd(0xB0);
    oled_cmd(0xC8);
    oled_cmd(0x00);
    oled_cmd(0x10);
    oled_cmd(0x40);
    oled_cmd(0x81); oled_cmd(0x7F);
    oled_cmd(0xA1);
    oled_cmd(0xA6);
    oled_cmd(0xA8); oled_cmd(0x3F);
    oled_cmd(0xA4);
    oled_cmd(0xD3); oled_cmd(0x00);
    oled_cmd(0xD5); oled_cmd(0x80);
    oled_cmd(0xD9); oled_cmd(0xF1);
    oled_cmd(0xDA); oled_cmd(0x12);
    oled_cmd(0xDB); oled_cmd(0x40);
    oled_cmd(0x8D); oled_cmd(0x14);
    oled_cmd(0xAF);
}

/* ================= FONT 5x7 ================= */

static const uint8_t FONT_0[5]     = {0x3E,0x51,0x49,0x45,0x3E};
static const uint8_t FONT_1[5]     = {0x00,0x42,0x7F,0x40,0x00};
static const uint8_t FONT_2[5]     = {0x42,0x61,0x51,0x49,0x46};
static const uint8_t FONT_3[5]     = {0x21,0x41,0x45,0x4B,0x31};
static const uint8_t FONT_4[5]     = {0x18,0x14,0x12,0x7F,0x10};
static const uint8_t FONT_5[5]     = {0x27,0x45,0x45,0x45,0x39};
static const uint8_t FONT_6[5]     = {0x3C,0x4A,0x49,0x49,0x30};
static const uint8_t FONT_7[5]     = {0x01,0x71,0x09,0x05,0x03};
static const uint8_t FONT_8[5]     = {0x36,0x49,0x49,0x49,0x36};
static const uint8_t FONT_9[5]     = {0x06,0x49,0x49,0x29,0x1E};
static const uint8_t FONT_COLON[5] = {0x00,0x36,0x36,0x00,0x00};
static const uint8_t FONT_DASH[5]  = {0x08,0x08,0x08,0x08,0x08};
static const uint8_t FONT_SPACE[5] = {0x00,0x00,0x00,0x00,0x00};
static const uint8_t FONT_T[5]     = {0x01,0x01,0x7F,0x01,0x01};
static const uint8_t FONT_C[5]     = {0x3E,0x41,0x41,0x41,0x22};
static const uint8_t FONT_N[5]     = {0x7F,0x02,0x04,0x08,0x7F};

static const uint8_t FONT_A[5]     = {0x7E,0x11,0x11,0x11,0x7E};
static const uint8_t FONT_B[5]     = {0x7F,0x49,0x49,0x49,0x36};
static const uint8_t FONT_D[5]     = {0x7F,0x41,0x41,0x22,0x1C};
static const uint8_t FONT_E[5]     = {0x7F,0x49,0x49,0x49,0x41};
static const uint8_t FONT_F[5]     = {0x7F,0x09,0x09,0x09,0x01};
static const uint8_t FONT_G[5]     = {0x3E,0x41,0x49,0x49,0x7A};
static const uint8_t FONT_H[5]     = {0x7F,0x08,0x08,0x08,0x7F};
static const uint8_t FONT_I[5]     = {0x00,0x41,0x7F,0x41,0x00};
static const uint8_t FONT_K[5]     = {0x7F,0x08,0x14,0x22,0x41};
static const uint8_t FONT_L[5]     = {0x7F,0x40,0x40,0x40,0x40};
static const uint8_t FONT_M[5]     = {0x7F,0x02,0x0C,0x02,0x7F};
static const uint8_t FONT_O[5]     = {0x3E,0x41,0x41,0x41,0x3E};
static const uint8_t FONT_P[5]     = {0x7F,0x09,0x09,0x09,0x06};
static const uint8_t FONT_R[5]     = {0x7F,0x09,0x19,0x29,0x46};
static const uint8_t FONT_S[5]     = {0x46,0x49,0x49,0x49,0x31};
static const uint8_t FONT_U[5]     = {0x3F,0x40,0x40,0x40,0x3F};
static const uint8_t FONT_W[5]     = {0x7F,0x20,0x18,0x20,0x7F};
static const uint8_t FONT_J[5]     = {0x20,0x40,0x41,0x3F,0x01};
static const uint8_t FONT_Q[5]     = {0x3E,0x41,0x51,0x21,0x5E};
static const uint8_t FONT_V[5]     = {0x1F,0x20,0x40,0x20,0x1F};
static const uint8_t FONT_X[5]     = {0x63,0x14,0x08,0x14,0x63};
static const uint8_t FONT_Y[5]     = {0x07,0x08,0x70,0x08,0x07};
static const uint8_t FONT_Z[5]     = {0x61,0x51,0x49,0x45,0x43};

static const uint8_t *get_font(char c)
{
    switch (c) {
        case '0': return FONT_0;
        case '1': return FONT_1;
        case '2': return FONT_2;
        case '3': return FONT_3;
        case '4': return FONT_4;
        case '5': return FONT_5;
        case '6': return FONT_6;
        case '7': return FONT_7;
        case '8': return FONT_8;
        case '9': return FONT_9;
        case ':': return FONT_COLON;
        case '-': return FONT_DASH;
        case 'A': return FONT_A;
        case 'B': return FONT_B;
        case 'C': return FONT_C;
        case 'D': return FONT_D;
        case 'E': return FONT_E;
        case 'F': return FONT_F;
        case 'G': return FONT_G;
        case 'H': return FONT_H;
        case 'I': return FONT_I;
        case 'J': return FONT_J;
        case 'K': return FONT_K;
        case 'L': return FONT_L;
        case 'M': return FONT_M;
        case 'N': return FONT_N;
        case 'O': return FONT_O;
        case 'P': return FONT_P;
        case 'Q': return FONT_Q;
        case 'R': return FONT_R;
        case 'S': return FONT_S;
        case 'T': return FONT_T;
        case 'U': return FONT_U;
        case 'V': return FONT_V;
        case 'W': return FONT_W;
        case 'X': return FONT_X;
        case 'Y': return FONT_Y;
        case 'Z': return FONT_Z;
        default:  return FONT_SPACE;
    }
}

static void oled_draw_char_small(uint8_t page, uint8_t col, char c)
{
    const uint8_t *font = get_font(c);

    uint8_t buf[6];

    memcpy(buf, font, 5);
    buf[5] = 0x00;

    oled_set_cursor(page, col);
    oled_data(buf, sizeof(buf));
}

static void oled_print_small(uint8_t page, uint8_t col, const char *str)
{
    while (*str && col < 128) {
        oled_draw_char_small(page, col, *str);
        col += 6;
        str++;
    }
}

static void oled_print_center(uint8_t page, const char *str)
{
    size_t len = strlen(str);
    uint8_t col = 0;

    if ((len * 6) < 128) {
        col = (128 - len * 6) / 2;
    }

    oled_print_small(page, col, str);
}

static void oled_draw_progress_bar(uint8_t page, uint8_t col, uint8_t width, uint8_t percent)
{
    if (page >= 8 || col >= 128 || width < 3) {
        return;
    }

    if ((col + width) > 128) {
        width = 128 - col;
    }

    if (percent > 100) {
        percent = 100;
    }

    uint8_t buf[128];
    memset(buf, 0x00, width);

    uint8_t fill_cols = (uint8_t)(((uint16_t)(width - 2) * percent) / 100);

    for (uint8_t i = 0; i < width; i++) {
        if (i == 0 || i == (width - 1)) {
            buf[i] = 0x7E;          // border left/right
        } else if (i <= fill_cols) {
            buf[i] = 0x7E;          // filled bar
        } else {
            buf[i] = 0x42;          // empty bar border
        }
    }

    oled_set_cursor(page, col);
    oled_data(buf, width);
}

/* ================= WIFI ICON ================= */

static const uint8_t WIFI_ICON_16X8[16] = {
    0x00, 0x04, 0x02, 0x0A,
    0x19, 0x05, 0x45, 0xF5,
    0xF5, 0x45, 0x05, 0x19,
    0x0A, 0x02, 0x04, 0x00
};

static void oled_draw_wifi_icon(bool enable)
{
    if (enable) {
        oled_set_cursor(0, 0);
        oled_data(WIFI_ICON_16X8, sizeof(WIFI_ICON_16X8));
    } else {
        oled_clear_area(0, 0, 16);
    }
}

/* ================= BIG FONT X3 ================= */

static void oled_draw_char_x3(uint8_t page, uint8_t col, char c)
{
    const uint8_t *font = get_font(c);

    uint8_t p0[18];
    uint8_t p1[18];
    uint8_t p2[18];

    memset(p0, 0x00, sizeof(p0));
    memset(p1, 0x00, sizeof(p1));
    memset(p2, 0x00, sizeof(p2));

    int idx = 0;

    for (int i = 0; i < 5; i++) {
        uint32_t expanded = 0;

        for (int bit = 0; bit < 8; bit++) {
            if (font[i] & (1 << bit)) {
                expanded |= (0x07UL << (bit * 3));
            }
        }

        for (int repeat = 0; repeat < 3; repeat++) {
            p0[idx] = expanded & 0xFF;
            p1[idx] = (expanded >> 8) & 0xFF;
            p2[idx] = (expanded >> 16) & 0xFF;
            idx++;
        }
    }

    for (int space = 0; space < 3; space++) {
        p0[idx] = 0x00;
        p1[idx] = 0x00;
        p2[idx] = 0x00;
        idx++;
    }

    oled_set_cursor(page, col);
    oled_data(p0, idx);

    oled_set_cursor(page + 1, col);
    oled_data(p1, idx);

    oled_set_cursor(page + 2, col);
    oled_data(p2, idx);
}

static void oled_draw_char_big(uint8_t page, uint8_t col, char c)
{
    const uint8_t *font = get_font(c);

    uint8_t upper[12];
    uint8_t lower[12];

    memset(upper, 0x00, sizeof(upper));
    memset(lower, 0x00, sizeof(lower));

    int idx = 0;

    for (int i = 0; i < 5; i++) {
        uint16_t expanded = 0;

        for (int bit = 0; bit < 8; bit++) {
            if (font[i] & (1 << bit)) {
                expanded |= (1 << (bit * 2));
                expanded |= (1 << (bit * 2 + 1));
            }
        }

        upper[idx] = expanded & 0xFF;
        lower[idx] = (expanded >> 8) & 0xFF;
        idx++;

        upper[idx] = expanded & 0xFF;
        lower[idx] = (expanded >> 8) & 0xFF;
        idx++;
    }

    upper[idx] = 0x00;
    lower[idx] = 0x00;
    idx++;

    upper[idx] = 0x00;
    lower[idx] = 0x00;
    idx++;

    oled_set_cursor(page, col);
    oled_data(upper, idx);

    oled_set_cursor(page + 1, col);
    oled_data(lower, idx);
}

static void oled_print_big(uint8_t page, uint8_t col, const char *str)
{
    while (*str) {
        oled_draw_char_big(page, col, *str);
        col += 12;
        str++;
    }
}

/* ================= TFT ST7735 BASIC ================= */
static spi_device_handle_t tft_spi;

#define ST7735_SWRESET  0x01
#define ST7735_SLPOUT   0x11
#define ST7735_COLMOD   0x3A
#define ST7735_MADCTL   0x36
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_DISPON   0X29

#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_GRAY      0x8410
#define COLOR_ORANGE    0xFD20
#define COLOR_DARKBLUE  0x0010

static void tft_cmd(uint8_t cmd)
{
    gpio_set_level(TFT_DC_PIN, 0);

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };

    ESP_ERROR_CHECK(spi_device_transmit(tft_spi, &t));
}

static void tft_data(const uint8_t *data, int len)
{
    if(len <= 0)
    {
        return;
    }

    gpio_set_level(TFT_DC_PIN, 1);

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    
    ESP_ERROR_CHECK(spi_device_transmit(tft_spi, &t));
}

static void tft_data_u8(uint8_t data)
{
    tft_data(&data,1);
}

static void tft_set_addr_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    uint8_t data[4];

    tft_cmd(ST7735_CASET);
    data[0] = 0x00;
    data[1] = x0;
    data[2] = 0x00;
    data[3] = x1;
    tft_data(data,4);

    tft_cmd(ST7735_RASET);
    data[0] = 0x00;
    data[1] = y0;
    data[2] = 0x00;
    data[3] = y1;
    tft_data(data,4);

    tft_cmd(ST7735_RAMWR);
}

static void tft_push_color(uint16_t color, int count)
{
    uint8_t line[160*2];

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    for(int i=0; i< 160; i++)
    {
        line[i*2] = hi;
        line[i*2+1] = lo;
    }

    while(count>0){
        int n = count;
        if(n> 160) n = 160;

        tft_data(line, n*2);
        count -= n;
    }
}

static void tft_fill_screen(uint16_t color)
{
    tft_set_addr_window(0,0,TFT_WIDTH -1, TFT_HEIGHT -1);
    tft_push_color(color, TFT_WIDTH * TFT_HEIGHT);
}

static void tft_fill_rect(int x, int y,int w, int h, uint16_t color)
{
    if(x<0)
    {
        w += x;
        x=0;
    }

    if(y<0)
    {
        h += y;
        y =0;
    }

    if((x+w) > TFT_WIDTH){
        w = TFT_WIDTH - x;
    }

    if((y+h) > TFT_HEIGHT)
    {
        h = TFT_HEIGHT - y;
    }

    if(w <= 0 || h <=0){
        return;
    }

    tft_set_addr_window(x,y,x+w-1, y+h-1);
    tft_push_color(color, w*h);
}

static void tft_draw_pixel(int x, int y, uint16_t color)
{
    if(x<0 || x >= TFT_WIDTH || y <0 || y >= TFT_HEIGHT)
    {
        return;
    }

    tft_set_addr_window(x,y,x,y);

    uint8_t data[2] = {
        color >> 8,
        color & 0xFF
    };

    tft_data(data,2);
}

static void tft_draw_line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        tft_draw_pixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        int e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void tft_fill_circle(int x0, int y0, int r, uint16_t color)
{
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if ((x * x + y * y) <= (r * r)) {
                tft_draw_pixel(x0 + x, y0 + y, color);
            }
        }
    }
}

static void tft_draw_sun_icon(int x, int y)
{
    tft_fill_circle(x + 16, y + 16, 9, COLOR_YELLOW);

    tft_draw_line(x + 16, y + 1,  x + 16, y + 7,  COLOR_ORANGE);
    tft_draw_line(x + 16, y + 25, x + 16, y + 31, COLOR_ORANGE);
    tft_draw_line(x + 1,  y + 16, x + 7,  y + 16, COLOR_ORANGE);
    tft_draw_line(x + 25, y + 16, x + 31, y + 16, COLOR_ORANGE);

    tft_draw_line(x + 5,  y + 5,  x + 9,  y + 9,  COLOR_ORANGE);
    tft_draw_line(x + 23, y + 23, x + 27, y + 27, COLOR_ORANGE);
    tft_draw_line(x + 27, y + 5,  x + 23, y + 9,  COLOR_ORANGE);
    tft_draw_line(x + 9,  y + 23, x + 5,  y + 27, COLOR_ORANGE);
}

static void tft_draw_cloud_icon(int x, int y)
{
    tft_fill_circle(x + 11, y + 19, 7, COLOR_WHITE);
    tft_fill_circle(x + 20, y + 15, 9, COLOR_WHITE);
    tft_fill_circle(x + 29, y + 19, 7, COLOR_WHITE);
    tft_fill_rect(x + 8, y + 19, 25, 9, COLOR_WHITE);
}

static void tft_draw_partly_cloudy_icon(int x, int y)
{
    tft_fill_circle(x + 12, y + 12, 8, COLOR_YELLOW);
    tft_draw_line(x + 12, y + 0,  x + 12, y + 5,  COLOR_ORANGE);
    tft_draw_line(x + 0,  y + 12, x + 5,  y + 12, COLOR_ORANGE);
    tft_draw_line(x + 3,  y + 3,  x + 7,  y + 7,  COLOR_ORANGE);

    tft_draw_cloud_icon(x + 5, y + 8);
}

static void tft_draw_rain_icon(int x, int y)
{
    tft_draw_cloud_icon(x, y);

    tft_draw_line(x + 12, y + 31, x + 9,  y + 36, COLOR_CYAN);
    tft_draw_line(x + 21, y + 31, x + 18, y + 36, COLOR_CYAN);
    tft_draw_line(x + 30, y + 31, x + 27, y + 36, COLOR_CYAN);
}

static void tft_draw_storm_icon(int x, int y)
{
    tft_draw_cloud_icon(x, y);

    tft_draw_line(x + 22, y + 28, x + 17, y + 38, COLOR_YELLOW);
    tft_draw_line(x + 17, y + 38, x + 24, y + 36, COLOR_YELLOW);
    tft_draw_line(x + 24, y + 36, x + 18, y + 45, COLOR_YELLOW);
}

static void tft_draw_fog_icon(int x, int y)
{
    tft_draw_cloud_icon(x, y);

    tft_draw_line(x + 5,  y + 32, x + 34, y + 32, COLOR_GRAY);
    tft_draw_line(x + 1,  y + 37, x + 30, y + 37, COLOR_GRAY);
    tft_draw_line(x + 9,  y + 42, x + 38, y + 42, COLOR_GRAY);
}

static void tft_draw_weather_icon(int code, int x, int y)
{
    if (code == 0) {
        tft_draw_sun_icon(x, y);
    } else if (code == 1 || code == 2 || code == 3) {
        tft_draw_partly_cloudy_icon(x, y);
    } else if (code == 45 || code == 48) {
        tft_draw_fog_icon(x, y);
    } else if ((code >= 51 && code <= 67) ||
               (code >= 80 && code <= 82)) {
        tft_draw_rain_icon(x, y);
    } else if (code == 95 || code == 96 || code == 99) {
        tft_draw_storm_icon(x, y);
    } else {
        tft_draw_cloud_icon(x, y);
    }
}

/* Font 5x7 ASCII, lấy từ font public-domain phổ biến */
static const uint8_t TFT_FONT_5X7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x7F,0x20,0x18,0x20,0x7F},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x10,0x08,0x08,0x10,0x08}, {0x00,0x00,0x00,0x00,0x00}
};

static void tft_draw_char(int x, int y, char c, uint16_t color, uint16_t bg, uint8_t scale)
{
    if(c < 32 || c>127) c = ' ';

    const uint8_t *bitmap = TFT_FONT_5X7[c - 32];

    for(int col =0; col <5; col ++)
    {
        uint8_t line = bitmap[col];

        for(int row = 0; row <8; row ++)
        {
            uint16_t pixel_color = (line & 0x01) ? color : bg;

            if(scale == 1)
            {
                tft_draw_pixel(x + col, y + row, pixel_color);
            }
            else
            {
                tft_fill_rect(x + col * scale, y + row * scale, scale, scale, pixel_color);
            }

            line >>= 1;
        }
    }

    if(scale ==1)
    {
        tft_fill_rect(x + 5, y, 1, 8, bg);
    }
    else
    {
        tft_fill_rect(x + 5 * scale, y, scale, 8* scale, bg);
    }
}

static void tft_draw_text(int x, int y, const char *text, uint16_t color, uint16_t bg, uint8_t scale)
{
    while(*text)
    {
        if(*text == '\n')
        {
            y += 8 * scale +2 ;
            x =0;
        }
        else
        {
            tft_draw_char(x,y,*text, color, bg, scale);
            x += 6 *scale;
        }

        text ++;
    }
}

static void tft_reset(void)
{
    gpio_set_level(TFT_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(TFT_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void tft_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TFT_DC_PIN) | (1ULL << TFT_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    spi_bus_config_t buscfg = {
        .mosi_io_num = TFT_MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = TFT_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = TFT_CS_PIN,
        .queue_size = 7,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(TFT_SPI_HOST, &devcfg, &tft_spi));

    tft_reset();

    tft_cmd(ST7735_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    tft_cmd(ST7735_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(150));

    tft_cmd(ST7735_COLMOD);
    tft_data_u8(0x05); // 16-bit color RGB565
    vTaskDelay(pdMS_TO_TICKS(10));

    tft_cmd(ST7735_MADCTL);
    tft_data_u8(0xA8); // Nếu màn ngược/màu sai, thử đổi 0xC8 thành 0x08, 0x60, 0xA0

    tft_cmd(ST7735_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));

    tft_fill_screen(COLOR_BLACK);
    tft_draw_text(12, 20, "TFT READY", COLOR_GREEN, COLOR_BLACK, 2);
}

/* ================= WIFI + NTP ================= */

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_connected = false;
        strcpy(g_ip_addr, "0.0.0.0");
        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        g_wifi_connected = true;
        snprintf(g_ip_addr,
                 sizeof(g_ip_addr),
                 IPSTR,
                 IP2STR(&event->ip_info.ip));

        ESP_LOGI(TAG, "Da ket noi WiFi, IP: %s", g_ip_addr);
    }
}

static void wifi_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL
    ));

    wifi_config_t wifi_config = {0};

    strncpy((char *)wifi_config.sta.ssid,
            WIFI_SSID,
            sizeof(wifi_config.sta.ssid) - 1);

    strncpy((char *)wifi_config.sta.password,
            WIFI_PASS,
            sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Dang ket noi WiFi...");
}

static bool ntp_sync_once(void)
{
    static bool sntp_started = false;

    setenv("TZ", "ICT-7", 1);
    tzset();

    if (!sntp_started) {
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "time.google.com");
        sntp_setservername(1, "pool.ntp.org");
        sntp_init();

        sntp_started = true;
    }

    ESP_LOGI(TAG, "Dang lay gio tu Internet...");

    time_t now = 0;
    struct tm timeinfo = {0};

    for (int retry = 0; retry < 10; retry++) {
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year >= (2024 - 1900)) {
            ESP_LOGI(TAG, "Da lay duoc gio NTP");
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGW(TAG, "Chua lay duoc gio NTP");
    return false;
}

static const char *get_weekday_str(int tm_wday)
{
    switch (tm_wday) {
        case 0: return "CN"; // Sunday
        case 1: return "T2"; // Monday
        case 2: return "T3"; // Tuesday
        case 3: return "T4"; // Wednesday
        case 4: return "T5"; // Thursday
        case 5: return "T6"; // Friday
        case 6: return "T7"; // Saturday
        default: return "--";
    }
}

/* ================= HABIT / STREAK WEB ================= */

static bool app_time_is_valid(time_t now)
{
    struct tm timeinfo = {0};
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_year >= (2024 - 1900);
}

static time_t get_midnight_epoch(time_t t)
{
    struct tm tm_info = {0};
    localtime_r(&t, &tm_info);

    tm_info.tm_hour = 0;
    tm_info.tm_min = 0;
    tm_info.tm_sec = 0;

    return mktime(&tm_info);
}

static void habit_sanitize_task(const char *input, char *output, size_t output_size)
{
    size_t out_idx = 0;

    if (output_size == 0) {
        return;
    }

    for (size_t i = 0; input[i] != '\0' && out_idx < output_size - 1; i++) {
        unsigned char ch = (unsigned char)input[i];

        if (ch >= 'a' && ch <= 'z') {
            ch = ch - 'a' + 'A';
        }

        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == ' ' ||
            ch == '-') {
            output[out_idx++] = (char)ch;
        }
    }

    output[out_idx] = '\0';

    while (out_idx > 0 && output[out_idx - 1] == ' ') {
        output[out_idx - 1] = '\0';
        out_idx--;
    }

    if (output[0] == '\0') {
        strncpy(output, HABIT_DEFAULT_TASK, output_size - 1);
        output[output_size - 1] = '\0';
    }
}

static int hex_char_to_int(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

static void url_decode_inplace(char *str)
{
    char *src = str;
    char *dst = str;

    while (*src != '\0') {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' &&
                   isxdigit((unsigned char)src[1]) &&
                   isxdigit((unsigned char)src[2])) {
            int hi = hex_char_to_int(src[1]);
            int lo = hex_char_to_int(src[2]);

            if (hi >= 0 && lo >= 0) {
                *dst++ = (char)((hi << 4) | lo);
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
}

static void habit_load_from_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("habit", NVS_READWRITE, &nvs);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Khong mo duoc NVS habit: %s", esp_err_to_name(err));
        return;
    }

    size_t task_len = sizeof(g_habit_task);
    err = nvs_get_str(nvs, "task", g_habit_task, &task_len);

    if (err != ESP_OK) {
        strncpy(g_habit_task, HABIT_DEFAULT_TASK, sizeof(g_habit_task) - 1);
        g_habit_task[sizeof(g_habit_task) - 1] = '\0';
    }

    int64_t start_epoch = 0;
    err = nvs_get_i64(nvs, "start", &start_epoch);

    if (err == ESP_OK) {
        g_habit_start_epoch = (time_t)start_epoch;
    }

    int32_t reset_count = 0;
    err = nvs_get_i32(nvs, "reset", &reset_count);

    if (err == ESP_OK) {
        g_habit_reset_count = reset_count;
    }

    nvs_close(nvs);
}

static void habit_save_to_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("habit", NVS_READWRITE, &nvs);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Khong mo duoc NVS habit de luu: %s", esp_err_to_name(err));
        return;
    }

    nvs_set_str(nvs, "task", g_habit_task);
    nvs_set_i64(nvs, "start", (int64_t)g_habit_start_epoch);
    nvs_set_i32(nvs, "reset", g_habit_reset_count);
    nvs_commit(nvs);
    nvs_close(nvs);
}

static void habit_ensure_start_date(time_t now)
{
    if (!app_time_is_valid(now)) {
        return;
    }

    if (g_habit_start_epoch <= 0) {
        g_habit_start_epoch = get_midnight_epoch(now);
        habit_save_to_nvs();
    }
}

static int habit_get_day(time_t now)
{
    if (!app_time_is_valid(now)) {
        return 0;
    }

    habit_ensure_start_date(now);

    time_t today_midnight = get_midnight_epoch(now);
    time_t start_midnight = get_midnight_epoch(g_habit_start_epoch);

    int day = (int)((today_midnight - start_midnight) / 86400) + 1;

    if (day < 1) {
        day = 1;
    }

    return day;
}

static void habit_get_start_date_str(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) {
        return;
    }

    if (g_habit_start_epoch <= 0) {
        snprintf(buf, buf_size, "-- --");
        return;
    }

    struct tm start_info = {0};
    localtime_r(&g_habit_start_epoch, &start_info);

    snprintf(buf,
             buf_size,
             "%02d-%02d",
             start_info.tm_mday,
             start_info.tm_mon + 1);
}

static void habit_reset_today(time_t now)
{
    if (!app_time_is_valid(now)) {
        return;
    }

    g_habit_start_epoch = get_midnight_epoch(now);
    g_habit_reset_count++;
    habit_save_to_nvs();
}

static void habit_set_task_and_reset(const char *new_task, time_t now)
{
    char clean_task[HABIT_MAX_TASK_LEN];

    habit_sanitize_task(new_task, clean_task, sizeof(clean_task));

    strncpy(g_habit_task, clean_task, sizeof(g_habit_task) - 1);
    g_habit_task[sizeof(g_habit_task) - 1] = '\0';

    if (app_time_is_valid(now)) {
        g_habit_start_epoch = get_midnight_epoch(now);
    } else {
        g_habit_start_epoch = 0;
    }

    g_habit_reset_count = 0;
    habit_save_to_nvs();
}

static void habit_make_task_line(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }

    strncpy(out, g_habit_task, out_size - 1);
    out[out_size - 1] = '\0';

    if (strlen(out) > 20) {
        out[20] = '\0';
    }
}

static void oled_show_habit(int day, bool wifi_ok)
{
    char line[32];
    char task_line[24];
    char start_date[8];

    oled_clear();
    oled_draw_wifi_icon(wifi_ok);

    habit_make_task_line(task_line, sizeof(task_line));
    habit_get_start_date_str(start_date, sizeof(start_date));

    oled_print_small(0, wifi_ok ? 86 : 80, wifi_ok ? "ONLINE" : "OFFLINE");
    oled_print_center(1, task_line);
    oled_print_center(2, "STREAK DAY");

    if (day <= 0) {
        snprintf(line, sizeof(line), "---");
    } else if (day > 999) {
        snprintf(line, sizeof(line), "999");
    } else {
        snprintf(line, sizeof(line), "%03d", day);
    }

    uint8_t day_col = (128 - strlen(line) * 12) / 2;
    oled_print_big(3, day_col, line);

    uint8_t progress = 0;
    if (day > 0) {
        progress = (uint8_t)((day * 100) / HABIT_TARGET_DAYS);
        if (progress > 100) {
            progress = 100;
        }
    }

    oled_draw_progress_bar(5, 14, 100, progress);

    snprintf(line, sizeof(line), "START %s", start_date);
    oled_print_small(6, 4, line);

    snprintf(line, sizeof(line), "RESET %ld", (long)g_habit_reset_count);
    oled_print_small(7, 4, line);
}

static esp_err_t habit_redirect_home(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t habit_root_get_handler(httpd_req_t *req)
{
    time_t now;
    char start_date[8];
    static char html[5200];

    time(&now);

    int day = habit_get_day(now);
    habit_get_start_date_str(start_date, sizeof(start_date));

    int progress = 0;
    if (day > 0) {
        progress = (day * 100) / HABIT_TARGET_DAYS;
        if (progress > 100) {
            progress = 100;
        }
    }

    snprintf(html,
             sizeof(html),
             "<!doctype html>"
             "<html><head>"
             "<meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>ESP32 Habit</title>"
             "<style>"
             ":root{color-scheme:dark;--bg:#0b1020;--card:#151b2e;--line:#26314d;--text:#f8fafc;--muted:#94a3b8;--yellow:#facc15;--red:#ef4444;--green:#22c55e;}"
             "*{box-sizing:border-box}body{margin:0;min-height:100vh;font-family:Inter,Arial,sans-serif;background:radial-gradient(circle at top,#25345f 0,#0b1020 42rem);color:var(--text);padding:24px;}"
             ".wrap{max-width:520px;margin:0 auto}.top{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}.brand{font-size:13px;color:var(--muted);letter-spacing:.12em;text-transform:uppercase}.pill{font-size:12px;background:#0f172a;border:1px solid var(--line);color:#cbd5e1;padding:7px 10px;border-radius:999px}"
             ".card{background:linear-gradient(180deg,#1b243b,#12182a);border:1px solid var(--line);border-radius:24px;padding:22px;margin:14px 0;box-shadow:0 18px 45px #0008}.task{font-size:26px;font-weight:800;line-height:1.15;margin:4px 0 12px}.label{font-size:13px;color:var(--muted);text-transform:uppercase;letter-spacing:.1em}.day{font-size:72px;line-height:.95;font-weight:900;color:var(--yellow);letter-spacing:-.05em;margin:8px 0 12px}"
             ".bar{height:12px;background:#0f172a;border:1px solid var(--line);border-radius:999px;overflow:hidden;margin:16px 0 14px}.bar>i{display:block;height:100%%;width:%d%%;background:linear-gradient(90deg,#facc15,#fb923c)}"
             ".grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.mini{background:#0f172a;border:1px solid var(--line);border-radius:16px;padding:12px}.mini b{display:block;font-size:17px;margin-top:4px}.muted{color:var(--muted)}"
             "button,input{width:100%%;border:0;border-radius:16px;font-size:16px}button{padding:16px 18px;font-weight:800;color:white;cursor:pointer}.danger{background:linear-gradient(135deg,#fb7185,#ef4444)}.save{background:linear-gradient(135deg,#34d399,#16a34a);margin-top:12px}input{margin-top:10px;padding:15px 16px;background:#f8fafc;color:#0f172a;outline:none}label{font-size:15px;color:#cbd5e1}.hint{font-size:13px;color:var(--muted);line-height:1.45;margin-top:10px}"
             "</style></head><body><div class='wrap'>"
             "<div class='top'><div class='brand'>ESP32 Habit</div><div class='pill'>%s</div></div>"
             "<section class='card'>"
             "<div class='label'>Current mission</div>"
             "<div class='task'>%s</div>"
             "<div class='label'>Clean streak</div>"
             "<div class='day'>Day %03d</div>"
             "<div class='bar'><i></i></div>"
             "<div class='grid'>"
             "<div class='mini'><span class='muted'>Start</span><b>%s</b></div>"
             "<div class='mini'><span class='muted'>Reset</span><b>%ld</b></div>"
             "<div class='mini'><span class='muted'>Goal</span><b>%dD</b></div>"
             "</div></section>"
             "<section class='card'><form action='/reset' method='get' onsubmit=\"return confirm('Reset streak ve ngay 1?')\"><button class='danger' type='submit'>Hom nay toi da choi game - Reset</button></form><div class='hint'>Bam nut nay khi bi pha streak. OLED se cap nhat lai trong vai giay.</div></section>"
             "<section class='card'><form action='/set' method='get'><label>Doi task hien thi tren OLED</label><input name='task' maxlength='31' value='%s' placeholder='VD: KHONG CHOI GAME'><button class='save' type='submit'>Luu task va dem lai tu ngay 1</button></form></section>"
             "</div></body></html>",
             progress,
             g_ip_addr,
             g_habit_task,
             day,
             start_date,
             (long)g_habit_reset_count,
             HABIT_TARGET_DAYS,
             g_habit_task);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t habit_reset_get_handler(httpd_req_t *req)
{
    time_t now;
    time(&now);

    habit_reset_today(now);
    return habit_redirect_home(req);
}

static esp_err_t habit_set_get_handler(httpd_req_t *req)
{
    char query[160] = {0};
    char task_value[HABIT_MAX_TASK_LEN * 3] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query,
                                  "task",
                                  task_value,
                                  sizeof(task_value)) == ESP_OK) {
            time_t now;
            time(&now);

            url_decode_inplace(task_value);
            habit_set_task_and_reset(task_value, now);
        }
    }

    return habit_redirect_home(req);
}

static esp_err_t habit_api_get_handler(httpd_req_t *req)
{
    time_t now;
    char start_date[8];
    static char json[256];

    time(&now);

    int day = habit_get_day(now);
    habit_get_start_date_str(start_date, sizeof(start_date));

    snprintf(json,
             sizeof(json),
             "{"
             "\"ok\":true,"
             "\"task\":\"%s\","
             "\"day\":%d,"
             "\"start\":\"%s\","
             "\"reset\":%ld"
             "}",
             g_habit_task,
             day,
             start_date,
             (long)g_habit_reset_count);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static void habit_web_server_start(void)
{
    static httpd_handle_t server = NULL;

    if (server != NULL) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Khong start duoc habit web server");
        return;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = habit_root_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t reset_uri = {
        .uri = "/reset",
        .method = HTTP_GET,
        .handler = habit_reset_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t set_uri = {
        .uri = "/set",
        .method = HTTP_GET,
        .handler = habit_set_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t api_uri = {
        .uri = "/api",
        .method = HTTP_GET,
        .handler = habit_api_get_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &reset_uri);
    httpd_register_uri_handler(server, &set_uri);
    httpd_register_uri_handler(server, &api_uri);

    ESP_LOGI(TAG, "Habit web server da san sang");
}

/* ================= WEATHER API ================= */

typedef struct {
    float temperature;
    int humidity;
    float precipitation;
    float wind_speed;
    int weather_code;
    char time[32];
} weather_data_t;

typedef struct {
    char *buffer;
    int buffer_size;
    int data_len;
} http_resp_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_buf_t *resp = (http_resp_buf_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (resp == NULL || resp->buffer == NULL) {
            return ESP_FAIL;
        }

        if ((resp->data_len + evt->data_len) >= resp->buffer_size) {
            ESP_LOGE(TAG, "HTTP buffer khong du");
            return ESP_FAIL;
        }

        memcpy(resp->buffer + resp->data_len, evt->data, evt->data_len);
        resp->data_len += evt->data_len;
        resp->buffer[resp->data_len] = '\0';
    }

    return ESP_OK;
}

static const char *weather_code_to_text(int code)
{
    switch (code) {
        case 0:
            return "Troi quang";

        case 1:
        case 2:
        case 3:
            return "Nhieu may";

        case 45:
        case 48:
            return "Suong mu";

        case 51:
        case 53:
        case 55:
            return "Mua phun";

        case 61:
        case 63:
        case 65:
        case 66:
        case 67:
            return "Co mua";

        case 80:
        case 81:
        case 82:
            return "Mua rao";

        case 95:
        case 96:
        case 99:
            return "Giong set";

        default:
            return "Khong ro";
    }
}

static bool weather_fetch(weather_data_t *out)
{
    if (out == NULL) {
        return false;
    }

    static char response[4096];
    memset(response, 0, sizeof(response));

    http_resp_buf_t resp = {
        .buffer = response,
        .buffer_size = sizeof(response),
        .data_len = 0,
    };

    esp_http_client_config_t config = {
        .url = WEATHER_URL,
        .event_handler = http_event_handler,
        .user_data = &resp,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP status code: %d", status_code);
        return false;
    }

    ESP_LOGI(TAG, "Weather JSON: %s", response);

    cJSON *root = cJSON_Parse(response);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse failed");
        return false;
    }

    cJSON *current = cJSON_GetObjectItem(root, "current");
    if (current == NULL) {
        ESP_LOGE(TAG, "JSON khong co current");
        cJSON_Delete(root);
        return false;
    }

    cJSON *time_item = cJSON_GetObjectItem(current, "time");
    cJSON *temp_item = cJSON_GetObjectItem(current, "temperature_2m");
    cJSON *hum_item  = cJSON_GetObjectItem(current, "relative_humidity_2m");
    cJSON *rain_item = cJSON_GetObjectItem(current, "precipitation");
    cJSON *code_item = cJSON_GetObjectItem(current, "weather_code");
    cJSON *wind_item = cJSON_GetObjectItem(current, "wind_speed_10m");

    if (!cJSON_IsString(time_item) ||
        !cJSON_IsNumber(temp_item) ||
        !cJSON_IsNumber(hum_item)  ||
        !cJSON_IsNumber(rain_item) ||
        !cJSON_IsNumber(code_item) ||
        !cJSON_IsNumber(wind_item)) {
        ESP_LOGE(TAG, "JSON thieu field can thiet");
        cJSON_Delete(root);
        return false;
    }

    memset(out, 0, sizeof(*out));

    strncpy(out->time, time_item->valuestring, sizeof(out->time) - 1);
    out->temperature = (float)temp_item->valuedouble;
    out->humidity = hum_item->valueint;
    out->precipitation = (float)rain_item->valuedouble;
    out->weather_code = code_item->valueint;
    out->wind_speed = (float)wind_item->valuedouble;

    cJSON_Delete(root);
    return true;
}

#define TFT_TIME_X       6
#define TFT_TIME_Y       4
#define TFT_TIME_SCALE   2
#define TFT_TIME_CHAR_W  (6 * TFT_TIME_SCALE)
#define TFT_TIME_CHAR_H  (8 * TFT_TIME_SCALE)

static char s_old_tft_time[9] = "        ";

static void tft_update_time_smart(const char *time_str)
{
    for (int i = 0; i < 8; i++) {
        if (s_old_tft_time[i] != time_str[i]) {
            int x = TFT_TIME_X + i * TFT_TIME_CHAR_W;

            tft_fill_rect(x,
                          TFT_TIME_Y,
                          TFT_TIME_CHAR_W,
                          TFT_TIME_CHAR_H,
                          COLOR_BLACK);

            tft_draw_char(x,
                          TFT_TIME_Y,
                          time_str[i],
                          COLOR_YELLOW,
                          COLOR_BLACK,
                          TFT_TIME_SCALE);

            s_old_tft_time[i] = time_str[i];
        }
    }
}

static void tft_force_update_time(const char *time_str)
{
    memset(s_old_tft_time, 0, sizeof(s_old_tft_time));
    tft_update_time_smart(time_str);
}

static void tft_show_weather(const weather_data_t *w, const char *time_str)
{
    char line[40];

    tft_fill_screen(COLOR_BLACK);

    tft_force_update_time(time_str);
    tft_draw_text(106, 8, "BAC NINH", COLOR_WHITE, COLOR_BLACK, 1);

    tft_draw_text(6, 28, "THOI TIET", COLOR_CYAN, COLOR_BLACK, 1);

    snprintf(line, sizeof(line), "%.1f C", w->temperature);
    tft_draw_text(6, 42, line, COLOR_YELLOW, COLOR_BLACK, 3);

    tft_fill_rect(112, 32, 44, 48, COLOR_BLACK);
    tft_draw_weather_icon(w->weather_code, 114, 34);

    snprintf(line, sizeof(line), "%s", weather_code_to_text(w->weather_code));
    tft_draw_text(6, 72, line, COLOR_GREEN, COLOR_BLACK, 1);

    snprintf(line, sizeof(line), "Do am: %d%%", w->humidity);
    tft_draw_text(6, 88, line, COLOR_WHITE, COLOR_BLACK, 1);

    snprintf(line, sizeof(line), "Gio  : %.1f km/h", w->wind_speed);
    tft_draw_text(6, 102, line, COLOR_WHITE, COLOR_BLACK, 1);

    snprintf(line, sizeof(line), "Mua  : %.1f mm", w->precipitation);
    tft_draw_text(6, 116, line, COLOR_WHITE, COLOR_BLACK, 1);

}

static void tft_show_message(const char *line1, const char *line2)
{
    tft_fill_screen(COLOR_BLACK);
    tft_draw_text(20, 38, line1, COLOR_WHITE, COLOR_BLACK, 2);
    tft_draw_text(28, 72, line2, COLOR_GRAY, COLOR_BLACK, 1);
}




/* ================= APP MAIN ================= */

void app_main(void)
{
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));

    oled_init();
    oled_clear();

    tft_init();
    tft_show_message("DANG DOI", "Ket noi WiFi");

    wifi_init();
    habit_load_from_nvs();
    habit_web_server_start();

    bool old_wifi_state = false;
    bool ntp_synced = false;

    time_t old_tft_second = 0;
    char old_oled_key[96] = "";

    bool weather_loaded_once = false;

    TickType_t last_weather_tick = 0;
    const TickType_t weather_update_interval = pdMS_TO_TICKS(5 * 60 * 1000); // 5 phút

    while (1) {
        bool wifi_now = g_wifi_connected;

        if (wifi_now != old_wifi_state) {
            old_wifi_state = wifi_now;

            if (!wifi_now && !weather_loaded_once) {
                tft_show_message("MAT WIFI", "Dang doi...");
            }
        }

        if (wifi_now && !ntp_synced) {
            ntp_synced = ntp_sync_once();
        }

        time_t now;
        struct tm timeinfo;

        time(&now);
        localtime_r(&now, &timeinfo);

        char time_str[16];

        if (timeinfo.tm_year < (2024 - 1900)) {
            strcpy(time_str, "--:--:--");
        } else {
            snprintf(time_str,
                    sizeof(time_str),
                    "%02d:%02d:%02d",
                    timeinfo.tm_hour,
                    timeinfo.tm_min,
                    timeinfo.tm_sec);
        }

        if (weather_loaded_once &&
            timeinfo.tm_year >= (2024 - 1900) &&
            now != old_tft_second) {
            old_tft_second = now;
            tft_update_time_smart(time_str);
        }

        TickType_t now_tick = xTaskGetTickCount();

        if (wifi_now &&
            (!weather_loaded_once ||
             (now_tick - last_weather_tick) >= weather_update_interval)) {
            weather_data_t weather;

            tft_show_message("THOI TIET", "Dang cap nhat");

            if (weather_fetch(&weather)) {
                tft_show_weather(&weather, time_str);
                old_tft_second = now;

                weather_loaded_once = true;
                last_weather_tick = now_tick;
            } else {
                ESP_LOGW(TAG, "Lay thoi tiet that bai");

                if (!weather_loaded_once) {
                    tft_show_message("LOI API", "Thu lai sau");
                }
            }
        }

        if (timeinfo.tm_year >= (2024 - 1900)) {
            habit_ensure_start_date(now);
        }

        int habit_day = habit_get_day(now);

        char habit_start_date[8];
        habit_get_start_date_str(habit_start_date, sizeof(habit_start_date));

        char oled_key[128];
        snprintf(oled_key,
                 sizeof(oled_key),
                 "%s|%d|%d|%s|%ld",
                 g_habit_task,
                 habit_day,
                 wifi_now ? 1 : 0,
                 habit_start_date,
                 (long)g_habit_reset_count);

        if (strcmp(oled_key, old_oled_key) != 0) {
            oled_show_habit(habit_day, wifi_now);
            strcpy(old_oled_key, oled_key);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
