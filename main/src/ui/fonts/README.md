# Card font

`card_bold_24.c`, `card_bold_20.c`, and `card_bold_16.c` contain Montserrat Bold at 24, 20, and 16 px, with 4-bit antialiasing and
printable ASCII glyphs. The font is licensed under the accompanying OFL.txt.
The source TTF ships with the pinned LVGL component. Regenerate from the
project root using the command below; substitute 20 or 16 for both occurrences
of 24 to regenerate the smaller sizes:

```sh
npx --yes lv_font_conv@1.5.3 --size 24 --bpp 4 \
  --font managed_components/lvgl__lvgl/tests/src/test_files/fonts/Montserrat-Bold.ttf \
  --range 0x20-0x7e --format lvgl --no-compress \
  --output main/src/ui/fonts/card_bold_24.c
```

Use `#include "lvgl.h"` in the generated source for the project's include paths.
