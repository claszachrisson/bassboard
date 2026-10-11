# Bassboard display

Home is a landscape 480 x 320 LVGL screen, rotated 180 degrees for the board. Effect cards follow hardware order:
blue, yellow, red, green, white. Cards always show
`name_short` in white, bold text sized to fit (24, 20, or 16 px), with a 26 px
bottom status band in the slot colour. Enabled effects show ON in that band;
disabled effects and empty slots leave it blank. Status text uses whichever of
black/white has higher contrast against the band colour. Cards have square
corners, 2 px name padding, and 4 px gaps. Focus adds an outline and opens the
parameter panel.
The existing footswitch and encoder behavior is preserved.

## Adding blocks and parameters

Every derived block initializes `DSPBlock({"Name", "SHORT"})`. These strings,
and parameter names/units, must have static lifetime. Override
`parameter_count()` and `parameter(index)` to expose copied numerical values.
Parameter IDs must be stable within the block. Override `selected_parameter()`
when the block supports parameter editing. Snapshots currently support up to
eight parameters per block (`DSP_MAX_PARAMETERS`).

The control entry points `acquire_focus()`, `release_focus()`, `encoder_step()`
and `button_short()` invoke the existing virtual hooks and publish snapshots.
Enabling/disabling also publishes. If another control path changes a public
atomic parameter directly, call `block.publish()` afterwards. Publish from
serialized task/control context, not an ISR or the audio processing loop.
Metadata registration occurs before input callbacks are enabled.

`BassboardDSP::register_block_changed_cb()` registers an observer and immediately
publishes existing blocks; newly added blocks publish when registered. The
callback receives a slot and a snapshot. LCD copies it into its model before
the callback returns. Audio parameters and enable flags are atomic, and block
objects are allocated in internal RAM to use ESP-IDF's hardware atomic path.
The audio processor does not acquire a GUI mutex or wait for display SPI.

Capacity is supplied to `BassboardDSP(sample_rate, capacity)` and exposed via
`capacity()`. Home creates that many cards. Capacities above five scroll
horizontally; the physical switches and LEDs still address the original five.

## Rendering and screens

Only the display task accesses LVGL and SPI. Producers notify it only on a
clean-to-dirty model transition; further changes replace pending values.
Rendering is limited to at most 30 Hz, with the delay rounded up to FreeRTOS
ticks (25 Hz with the current 100 Hz tick configuration). Idle waits are driven
by notifications and LVGL timer deadlines, with no DSP state polling.

`ui::HBox`, `ui::VBox`, and `ui::Label` supply basic styling through `ui::Style`:
padding, margin, gap, colour, and bgcolour. Antialiased Montserrat fonts
are used, including a generated bold font for the card labels. `ui::Screen` and `ui::ScreenController` provide the extension point for
additional screens. Create/load/update screens on the UI task, and apply the
retained model snapshot when switching. Only Home is implemented today.

LVGL's custom allocator puts its objects/styles/text in PSRAM. A single
480 x 20 RGB565 draw buffer uses 19,200 bytes of internal RAM; the ILI9488
RGB666 conversion buffer uses another 28,800 bytes. SPI DMA completion is
awaited by the UI task before the driver's conversion buffer is reused.
The initial backlight is 50%, enabled after the first frame is transferred.
`LCD::set_brightness(0..255)` changes PWM brightness through the UI task.

## Wiring and reset

CS=GPIO15, clock=GPIO19, MOSI=GPIO18, DC=GPIO5, backlight=GPIO4 (PWM).
RESET is expander P1.7 and remains released by its pull-up during normal
startup. The driver performs a software reset during initialization.
`LCD::request_hardware_reset()` pulses P1.7, reinitializes the panel, restores
landscape orientation, and invalidates the screen for a complete redraw.
SPI starts at 20 MHz. Panel orientation/mirroring and RGB order must still be
confirmed on the actual board.

PSRAM support is enabled in sdkconfig and sdkconfig.defaults for WROVER-E.
The existing project configuration is retained alongside these changes.

## Verification

Build firmware with `idf.py build`. Run the host screen tests after dependencies
have been downloaded:

```sh
cmake -S tests/ui_host -B tests/ui_host/build -G Ninja
cmake --build tests/ui_host/build
cd tests/ui_host
./build/home_test
```

The test checks parameter values, focus, no redraws for unchanged state, enabled-status updates,
partial redraws, runtime capacity, and switching between screens. It writes
`home.ppm` and `editing.ppm` previews. Hardware SPI, reset, panel colours, and
actual audio timing require validation on the board.
