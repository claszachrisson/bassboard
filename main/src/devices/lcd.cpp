#include "lcd.h"
#include "ui.h"
#include "io_expander.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9488.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <algorithm>

extern "C" void bassboard_ui_memory_init(void);

namespace {
constexpr int WIDTH = 480, HEIGHT = 320, DRAW_LINES = 20;
constexpr size_t PIXELS = WIDTH * DRAW_LINES;
// Round up: at the current 100 Hz tick rate this gives a 40 ms minimum.
constexpr TickType_t FRAME_TICKS = (34 * configTICK_RATE_HZ + 999) / 1000;
constexpr spi_host_device_t HOST = SPI2_HOST;
constexpr ledc_mode_t PWM_MODE = LEDC_LOW_SPEED_MODE;
constexpr ledc_channel_t PWM_CHANNEL = LEDC_CHANNEL_0;
struct Transfer { SemaphoreHandle_t complete; esp_lcd_panel_handle_t panel; };
bool transfer_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* ctx)
{
    auto* transfer = static_cast<Transfer*>(ctx);
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(transfer->complete, &wake);
    return wake == pdTRUE;
}
void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels)
{
    auto* transfer = static_cast<Transfer*>(lv_display_get_user_data(display));
    // The driver owns one RGB666 conversion buffer. Wait before reusing it.
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(transfer->panel, area->x1, area->y1,
                                             area->x2 + 1, area->y2 + 1, pixels));
    configASSERT(xSemaphoreTake(transfer->complete, pdMS_TO_TICKS(1000)) == pdTRUE);
    lv_display_flush_ready(display);
}
uint32_t tick_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void brightness(uint8_t value)
{
    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODE, PWM_CHANNEL, value));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODE, PWM_CHANNEL));
}
void orient(esp_lcd_panel_handle_t panel)
{
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, true));
}
}
LCD::LCD(int capacity) : model_(std::max(1, capacity))
{
    model_mutex_ = xSemaphoreCreateMutex();
    configASSERT(model_mutex_);
    // Creation while holding the model mutex prevents the new task observing task_ unset.
    xSemaphoreTake(model_mutex_, portMAX_DELAY);
    const auto result = xTaskCreate(task_main, "display", 6144, this, 2, &task_);
    xSemaphoreGive(model_mutex_);
    configASSERT(result == pdPASS);
}
void LCD::block_changed(int slot, const DSPBlockSnapshot& snapshot, void* ctx)
{
    auto* self = static_cast<LCD*>(ctx);
    if (slot < 0 || static_cast<size_t>(slot) >= self->model_.size()) return;
    xSemaphoreTake(self->model_mutex_, portMAX_DELAY);
    self->model_[slot] = snapshot;
    // Coalesce producers: only the clean-to-dirty transition needs a notification.
    const bool wake = !self->dirty_;
    self->dirty_ = true;
    xSemaphoreGive(self->model_mutex_);
    if (wake) xTaskNotifyGive(self->task_);
}
void LCD::set_brightness(uint8_t value)
{
    xSemaphoreTake(model_mutex_, portMAX_DELAY);
    brightness_ = value;
    xSemaphoreGive(model_mutex_);
    xTaskNotifyGive(task_);
}
void LCD::request_hardware_reset()
{
    xSemaphoreTake(model_mutex_, portMAX_DELAY);
    reset_requested_ = true;
    xSemaphoreGive(model_mutex_);
    xTaskNotifyGive(task_);
}
void LCD::task_main(void* ctx) { static_cast<LCD*>(ctx)->run(); }
void LCD::run()
{
    ledc_timer_config_t timer{};
    timer.speed_mode = PWM_MODE;
    timer.duty_resolution = LEDC_TIMER_8_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = 1000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ledc_channel_config_t channel{};
    channel.gpio_num = GPIO_NUM_4;
    channel.speed_mode = PWM_MODE;
    channel.channel = PWM_CHANNEL;
    channel.timer_sel = LEDC_TIMER_0;
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
    // P1.7 is pulled high. Leave it as an input until hardware reset is requested.
    spi_bus_config_t bus{};
    bus.mosi_io_num = GPIO_NUM_18;
    bus.miso_io_num = -1;
    bus.sclk_io_num = GPIO_NUM_19;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = PIXELS * 3;
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &bus, SPI_DMA_CH_AUTO));
    Transfer transfer{xSemaphoreCreateBinary(), nullptr};
    configASSERT(transfer.complete);
    esp_lcd_panel_io_spi_config_t io_config{};
    io_config.cs_gpio_num = GPIO_NUM_15;
    io_config.dc_gpio_num = GPIO_NUM_5;
    io_config.spi_mode = 0;
    io_config.pclk_hz = 20 * 1000 * 1000;
    io_config.trans_queue_depth = 1;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.on_color_trans_done = transfer_done;
    io_config.user_ctx = &transfer;
    esp_lcd_panel_io_handle_t io = nullptr;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(HOST, &io_config, &io));
    esp_lcd_panel_dev_config_t panel_config{};
    panel_config.reset_gpio_num = -1;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_config.bits_per_pixel = 18;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(io, &panel_config, PIXELS, &transfer.panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(transfer.panel)); // Software reset.
    ESP_ERROR_CHECK(esp_lcd_panel_init(transfer.panel));
    orient(transfer.panel);

    bassboard_ui_memory_init();
    lv_init();
    lv_tick_set_cb(tick_ms);
    auto* display = lv_display_create(WIDTH, HEIGHT);
    configASSERT(display);
    auto* draw_buffer = heap_caps_malloc(PIXELS * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    configASSERT(draw_buffer);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, nullptr, PIXELS * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(display, &transfer);
    lv_display_set_flush_cb(display, flush);
    // Refresh is explicitly serviced by our task, never by an independent render timer.
    lv_timer_pause(lv_display_get_refr_timer(display));
    ui::HomeScreen home(model_.size());
    ui::ScreenController screens;
    screens.show(home);
    std::vector<DSPBlockSnapshot> snapshot(model_.size());
    TickType_t last_frame = xTaskGetTickCount() - FRAME_TICKS;
    ESP_LOGI("LCD", "Landscape Home: %u slots, %u-byte draw buffer; internal free=%u, PSRAM free=%u",
             static_cast<unsigned>(model_.size()), static_cast<unsigned>(PIXELS * 2),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    while (true) {
        const TickType_t elapsed = xTaskGetTickCount() - last_frame;
        if (elapsed < FRAME_TICKS) vTaskDelay(FRAME_TICKS - elapsed);
        xSemaphoreTake(model_mutex_, portMAX_DELAY);
        const bool changed = dirty_;
        if (changed) snapshot = model_;
        dirty_ = false;
        const bool reset = reset_requested_;
        reset_requested_ = false;
        const uint8_t backlight = brightness_;
        xSemaphoreGive(model_mutex_);
        if (reset) {
            brightness(0);
            // Set the output latch before changing direction to avoid an unintended pulse.
            ioex_set_level(EXIO_NUM_17, 1);
            ioex_set_direction(EXIO_NUM_17, IOEX_OUTPUT);
            ioex_set_level(EXIO_NUM_17, 0);
            vTaskDelay(pdMS_TO_TICKS(20));
            ioex_set_level(EXIO_NUM_17, 1);
            vTaskDelay(pdMS_TO_TICKS(120));
            ESP_ERROR_CHECK(esp_lcd_panel_init(transfer.panel));
            orient(transfer.panel);
            lv_obj_invalidate(home.root());
        }
        if (changed) screens.update(snapshot);
        // Widget invalidation can resume this timer; keep refresh under our cap.
        lv_timer_pause(lv_display_get_refr_timer(display));
        const uint32_t next_timer = lv_timer_handler();
        lv_refr_now(display);
        brightness(backlight); // First frame is fully transferred before lighting the panel.
        last_frame = xTaskGetTickCount();
        const TickType_t wait = next_timer == LV_NO_TIMER_READY ? portMAX_DELAY :
            std::max(FRAME_TICKS, pdMS_TO_TICKS(next_timer));
        ulTaskNotifyTake(pdTRUE, wait);
    }
}
