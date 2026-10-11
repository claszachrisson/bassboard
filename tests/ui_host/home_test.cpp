#include "ui.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
static unsigned flushes = 0;
static std::vector<uint16_t> framebuffer(480 * 320);
static uint32_t tick = 0;
static void flush(lv_display_t* d, const lv_area_t* area, uint8_t* data)
{
    ++flushes;
    auto* pixels = reinterpret_cast<uint16_t*>(data);
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x)
            framebuffer[y * 480 + x] = *pixels++;
    lv_display_flush_ready(d);
}
static unsigned matching(lv_obj_t* obj, const char* text)
{
    unsigned n = 0;
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), text)) ++n;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) n += matching(lv_obj_get_child(obj, i), text);
    return n;
}
static void ppm(const char* file)
{
    FILE* f = fopen(file, "wb"); assert(f);
    fprintf(f, "P6\n480 320\n255\n");
    for (auto v : framebuffer) {
        unsigned char rgb[] = {static_cast<unsigned char>(((v >> 11) & 31) * 255 / 31),
            static_cast<unsigned char>(((v >> 5) & 63) * 255 / 63),
            static_cast<unsigned char>((v & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
int main()
{
    lv_init(); lv_tick_set_cb([] { return tick; });
    auto* d = lv_display_create(480, 320);
    std::vector<uint16_t> draw(480 * 20);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, draw.data(), nullptr, draw.size()*2, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    lv_timer_pause(lv_display_get_refr_timer(d));
    {
        ui::HomeScreen home(5); ui::ScreenController screens; screens.show(home);
        std::vector<DSPBlockSnapshot> state(5);
        state[0].info = {"Overdrive", "OVDR"}; state[0].present = true;
        state[0].parameter_count = 2;
        state[0].parameters[0] = {0, "Input gain", "", 5.0f, 2};
        state[0].parameters[1] = {1, "Output gain", "", 0.2f, 2};
        state[1].info = {"Tremolo", "TRMLO"}; state[1].present = true;
        screens.update(state); lv_refr_now(d); ppm("home.ppm");
        assert(matching(home.root(), "Bassboard") == 0);
        assert(matching(home.root(), "OVDR") == 1);
        assert(matching(home.root(), "TRMLO") == 1);
        auto* cards = lv_obj_get_child(home.root(), 0);
        auto* card = lv_obj_get_child(cards, 0);
        auto* card_label = lv_obj_get_child(lv_obj_get_child(card, 0), 0);
        auto* band = lv_obj_get_child(card, 1);
        auto* status_label = lv_obj_get_child(band, 0);
        assert(lv_obj_get_style_radius(card, 0) == 0);
        assert(lv_obj_get_height(band) == 26);
        assert(lv_obj_get_width(band) == lv_obj_get_width(card));
        assert(lv_color_eq(lv_obj_get_style_text_color(status_label, 0), lv_color_hex(0x000000)));
        assert(matching(home.root(), "ON") == 0);
        assert(lv_color_eq(lv_obj_get_style_text_color(card_label, 0), lv_color_hex(0xffffff)));
        const auto* font = lv_obj_get_style_text_font(card_label, 0);
        assert(font->line_height >= 24);
        for (const char* name : {"OVDR", "TREM", "COMP", "FLAN", "PHAS", "CHOR", "DELA"})
            assert(lv_text_get_width(name, strlen(name), font, 0) <= lv_obj_get_content_width(card));
        const auto slot_colour = lv_obj_get_style_bg_color(band, 0);
        auto* tremolo_label = lv_obj_get_child(lv_obj_get_child(lv_obj_get_child(cards, 1), 0), 0);
        const auto* tremolo_font = lv_obj_get_style_text_font(tremolo_label, 0);
        assert(lv_text_get_width("TRMLO", 5, tremolo_font, 0) <= lv_obj_get_content_width(tremolo_label));
        assert(lv_obj_get_height(tremolo_label) == tremolo_font->line_height);
        assert(tremolo_font->line_height < font->line_height);
        const unsigned first = flushes;
        screens.update(state); lv_refr_now(d); assert(flushes == first);
        // Enabled changes update the status band with a partial redraw.
        state[0].enabled = true; screens.update(state); lv_refr_now(d);
        assert(matching(home.root(), "ON") == 1);
        assert(flushes > first && flushes - first < 16);
        const unsigned enabled = flushes;
        screens.update(state); lv_refr_now(d); assert(flushes == enabled);
        state[0].enabled = false; screens.update(state); lv_refr_now(d);
        assert(matching(home.root(), "ON") == 0);
        assert(lv_color_eq(lv_obj_get_style_bg_color(band, 0), slot_colour));
        state[2].enabled = true; // Empty slots never show ON.
        screens.update(state); lv_refr_now(d); assert(matching(home.root(), "ON") == 0);
        state[0].enabled = true; screens.update(state); lv_refr_now(d);
        state[0].focused = true; state[0].selected_parameter = 0;
        screens.update(state); lv_refr_now(d);
        assert(matching(home.root(), "Input gain") == 1);
        assert(matching(home.root(), "5.00") == 1);
        assert(lv_color_eq(lv_obj_get_style_bg_color(band, 0), slot_colour));
        assert(lv_obj_get_style_outline_opa(card, 0) == LV_OPA_COVER);
        const unsigned focused = flushes;
        state[0].parameters[0].value = 5.1f;
        screens.update(state); lv_refr_now(d);
        assert(matching(home.root(), "5.10") == 1);
        assert(flushes > focused);
        // A single value update fits in less than a full frame of twenty-line chunks.
        assert(flushes - focused < 16);
        ppm("editing.ppm");
        const unsigned edited = flushes;
        screens.update(state); lv_refr_now(d); assert(flushes == edited);
        state[0].focused = false; screens.update(state); lv_refr_now(d);
        assert(lv_obj_has_flag(lv_obj_get_child(home.root(), 1), LV_OBJ_FLAG_HIDDEN));
        // Runtime capacity beyond the physical five slots remains safe.
        ui::HomeScreen larger(8); screens.show(larger);
        larger.update(std::vector<DSPBlockSnapshot>(8)); lv_refr_now(d);
        // A separate screen can be loaded, then Home restored with its state intact.
        screens.show(home); screens.update(state); lv_refr_now(d);
    }
    lv_display_delete(d); lv_deinit();
    puts("PASS: metadata labels, focus, values, ON status, unchanged-state redraws, partial refresh, runtime capacity, screen switching");
}
