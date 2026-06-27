#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

#define WIFI_SSID "MODERN SYSTEM"
#define WIFI_PASS "Modern2025@1"

#define I2C_PORT    I2C_NUM_0
#define OLED_ADDR   0x3C
#define SDA_PIN     21
#define SCL_PIN     22

static const char *TAG = "CLOCK_OLED";

static volatile bool g_wifi_connected = false;

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
        case 'T': return FONT_T;
        case 'C': return FONT_C;
        case 'N': return FONT_N;
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
        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        g_wifi_connected = true;
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

    wifi_init();

    bool old_wifi_state = false;
    bool ntp_synced = false;

    char old_time_str[16] = "";
    char old_date_str[40] = "";

    while (1) {
        bool wifi_now = g_wifi_connected;

        if (wifi_now != old_wifi_state) {
            oled_draw_wifi_icon(wifi_now);
            old_wifi_state = wifi_now;
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

        if (strcmp(time_str, old_time_str) != 0) {
            oled_clear_area(2, 16, 96);
            oled_clear_area(3, 16, 96);

            oled_print_big(2, 16, time_str);

            strcpy(old_time_str, time_str);
        }

        char date_str[40];

        if (timeinfo.tm_year < (2024 - 1900)) {
            strcpy(date_str, "-- -- -- ----");
        } else {
            snprintf(date_str,
                    sizeof(date_str),
                    "%s %02d-%02d-%04d",
                    get_weekday_str(timeinfo.tm_wday),
                    timeinfo.tm_mday,
                    timeinfo.tm_mon + 1,
                    timeinfo.tm_year + 1900);
        }

        if (strcmp(date_str, old_date_str) != 0) {
            oled_clear_area(5, 0, 128);

            uint8_t date_col = (128 - strlen(date_str) * 6) / 2;
            oled_print_small(5, date_col, date_str);

            strcpy(old_date_str, date_str);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}