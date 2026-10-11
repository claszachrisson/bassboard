#pragma once
#include <memory>
#include <vector>
#include "lvgl.h"
#include "dsp_types.h"

namespace ui {
struct Style {
    int padding = 0;
    int margin = 0;
    int gap = 8;
    uint32_t colour = 0xffffff;
    uint32_t bgcolour = 0x111722;
};
lv_obj_t* VBox(lv_obj_t* parent, const Style& style = {});
lv_obj_t* HBox(lv_obj_t* parent, const Style& style = {});
lv_obj_t* Label(lv_obj_t* parent, const char* text, const Style& style = {});

// Screens and widgets are owned/accessed exclusively by the UI task.
// Future screens implement this interface and are loaded by ScreenController.
class Screen {
public:
    virtual ~Screen();
    lv_obj_t* root() const { return root_; }
    virtual void update(const std::vector<DSPBlockSnapshot>& blocks) = 0;
protected:
    lv_obj_t* root_ = nullptr;
};
class ScreenController {
public:
    void show(Screen& screen);
    void update(const std::vector<DSPBlockSnapshot>& blocks);
private:
    Screen* active_ = nullptr;
};
class HomeScreen final : public Screen {
public:
    explicit HomeScreen(size_t capacity);
    void update(const std::vector<DSPBlockSnapshot>& blocks) override;
private:
    struct Card { lv_obj_t* box; lv_obj_t* label; lv_obj_t* status; };
    struct ParameterRow { lv_obj_t* box; lv_obj_t* name; lv_obj_t* value; };
    std::vector<Card> cards_;
    std::vector<DSPBlockSnapshot> previous_;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* heading_ = nullptr;
    std::vector<ParameterRow> rows_;
    int focused_ = -2;
};
}
