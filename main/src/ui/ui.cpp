#include "ui.h"
#include <cstdio>
#include <cstring>
#include <cmath>

LV_FONT_DECLARE(card_bold_24);
LV_FONT_DECLARE(card_bold_20);
LV_FONT_DECLARE(card_bold_16);

namespace ui {
static uint32_t contrasting_text(uint32_t colour)
{
    const auto linear = [](uint32_t channel) {
        const double value = channel / 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    const double luminance = 0.2126 * linear((colour >> 16) & 255) +
                             0.7152 * linear((colour >> 8) & 255) +
                             0.0722 * linear(colour & 255);
    const double black_contrast = (luminance + 0.05) / 0.05;
    const double white_contrast = 1.05 / (luminance + 0.05);
    return black_contrast >= white_contrast ? 0x000000 : 0xffffff;
}

static void apply(lv_obj_t* obj, const Style& style)
{
    lv_obj_set_style_pad_all(obj, style.padding, 0);
    lv_obj_set_style_margin_all(obj, style.margin, 0);
    lv_obj_set_style_pad_row(obj, style.gap, 0);
    lv_obj_set_style_pad_column(obj, style.gap, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(style.colour), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(style.bgcolour), 0);
}
static lv_obj_t* layout(lv_obj_t* parent, const Style& style, lv_flex_flow_t flow)
{
    auto* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    apply(obj, style);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj, flow);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    return obj;
}
lv_obj_t* VBox(lv_obj_t* parent, const Style& style) { return layout(parent, style, LV_FLEX_FLOW_COLUMN); }
lv_obj_t* HBox(lv_obj_t* parent, const Style& style) { return layout(parent, style, LV_FLEX_FLOW_ROW); }
lv_obj_t* Label(lv_obj_t* parent, const char* text, const Style& style)
{
    auto* obj = lv_label_create(parent);
    apply(obj, style);
    lv_label_set_text(obj, text);
    return obj;
}
Screen::~Screen() { if (root_) lv_obj_delete(root_); }
void ScreenController::show(Screen& screen) { active_ = &screen; lv_screen_load(screen.root()); }
void ScreenController::update(const std::vector<DSPBlockSnapshot>& blocks) { if (active_) active_->update(blocks); }

HomeScreen::HomeScreen(size_t capacity) : previous_(capacity)
{
    root_ = lv_obj_create(nullptr);
    lv_obj_remove_style_all(root_);
    apply(root_, Style{4, 0, 8});
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(root_, &lv_font_montserrat_20, 0);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    auto* effects = HBox(root_, Style{2, 0, 4});
    lv_obj_set_height(effects, 76);
    static constexpr uint32_t colours[] = {0x389cff, 0xffd34a, 0xff6262, 0x55d58a, 0xf0f2f5};
    for (size_t i = 0; i < capacity; ++i) {
        auto* card = VBox(effects, Style{0, 0, 0});
        // For larger capacities retain readable cards and allow horizontal scrolling.
        lv_obj_set_width(card, capacity <= 5 ? 1 : 92);
        if (capacity <= 5) lv_obj_set_flex_grow(card, 1);
        lv_obj_set_height(card, LV_PCT(100));
        lv_obj_set_style_bg_color(card, lv_color_hex(0x1d2634), 0);
        lv_obj_set_style_radius(card, 0, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_outline_width(card, 2, 0);
        lv_obj_set_style_outline_color(card, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_outline_opa(card, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        auto* name_region = VBox(card, Style{2, 0, 0, 0xffffff, 0x1d2634});
        lv_obj_set_height(name_region, 1);
        lv_obj_set_flex_grow(name_region, 1);
        lv_obj_set_flex_align(name_region, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(name_region, LV_OBJ_FLAG_SCROLLABLE);
        auto* label = Label(name_region, "--", Style{0, 0, 0, 0xffffff});
        lv_obj_set_style_text_font(label, &card_bold_24, 0);
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        const uint32_t colour = colours[i % 5];
        auto* band = VBox(card, Style{0, 0, 0, contrasting_text(colour), colour});
        lv_obj_set_height(band, 26);
        lv_obj_set_flex_align(band, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(band, LV_OBJ_FLAG_SCROLLABLE);
        auto* status = Label(band, "", Style{0, 0, 0, contrasting_text(colour)});
        lv_obj_set_style_text_font(status, &card_bold_16, 0);
        cards_.push_back({card, label, status});
    }
    panel_ = VBox(root_, Style{10, 0, 6});
    lv_obj_set_flex_grow(panel_, 1);
    lv_obj_set_style_bg_color(panel_, lv_color_hex(0x1d2634), 0);
    lv_obj_set_style_radius(panel_, 0, 0);
    heading_ = Label(panel_, "");
    for (size_t i = 0; i < DSP_MAX_PARAMETERS; ++i) {
        auto* row = HBox(panel_, Style{4, 0, 8, 0xffffff, 0x1d2634});
        lv_obj_set_style_radius(row, 0, 0);
        auto* name = Label(row, "");
        lv_obj_set_flex_grow(name, 1);
        auto* value = Label(row, "");
        rows_.push_back({row, name, value});
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
}
void HomeScreen::update(const std::vector<DSPBlockSnapshot>& blocks)
{
    lv_obj_update_layout(root_); // Resolve flex widths before measuring text.
    int focus = -1;
    for (size_t i = 0; i < cards_.size(); ++i) {
        const auto& block = blocks[i];
        const auto& old = previous_[i];
        const char* name = block.present ? block.info.name_short : "--";
        const lv_font_t* font = &card_bold_16;
        for (const auto* candidate : {&card_bold_24, &card_bold_20, &card_bold_16}) {
            if (lv_text_get_width(name, std::strlen(name), candidate, 0) <= lv_obj_get_content_width(cards_[i].label)) {
                font = candidate;
                break;
            }
        }
        if (lv_obj_get_style_text_font(cards_[i].label, 0) != font)
            lv_obj_set_style_text_font(cards_[i].label, font, 0);
        if (std::strcmp(lv_label_get_text(cards_[i].label), name)) lv_label_set_text(cards_[i].label, name);
        const char* status = block.present && block.enabled ? "ON" : "";
        if (std::strcmp(lv_label_get_text(cards_[i].status), status))
            lv_label_set_text(cards_[i].status, status);
        if (block.focused) focus = static_cast<int>(i);
        if (block.focused != old.focused)
            lv_obj_set_style_outline_opa(cards_[i].box, block.focused ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    if (focus != focused_) {
        if (focus < 0) lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    }
    if (focus >= 0) {
        const auto& block = blocks[focus];
        if (std::strcmp(lv_label_get_text(heading_), block.info.name)) lv_label_set_text(heading_, block.info.name);
        for (size_t i = 0; i < rows_.size(); ++i) {
            auto& row = rows_[i];
            if (i >= block.parameter_count) { lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue; }
            lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
            const auto& param = block.parameters[i];
            if (std::strcmp(lv_label_get_text(row.name), param.name)) lv_label_set_text(row.name, param.name);
            char text[48];
            std::snprintf(text, sizeof(text), "%.*f%s%s", param.decimals, static_cast<double>(param.value),
                          param.unit[0] ? " " : "", param.unit);
            if (std::strcmp(lv_label_get_text(row.value), text)) lv_label_set_text(row.value, text);
            const bool selected = static_cast<int>(param.id) == block.selected_parameter;
            const bool was_selected = static_cast<int>(param.id) == previous_[focus].selected_parameter;
            if (focus != focused_ || selected != was_selected)
                lv_obj_set_style_bg_color(row.box, lv_color_hex(selected ? 0x34445c : 0x1d2634), 0);
        }
    }
    focused_ = focus;
    previous_ = blocks;
}
}
