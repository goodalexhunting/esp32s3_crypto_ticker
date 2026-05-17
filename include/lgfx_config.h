#pragma once
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_Parallel8 _bus_instance;

public:

  LGFX(void)
  {
    // ---------------------------
    // BUS (8080 parallel LCD)
    // ---------------------------
    auto cfg = _bus_instance.config();

    cfg.port = 0;

    cfg.pin_d0  = 39;
    cfg.pin_d1  = 40;
    cfg.pin_d2  = 41;
    cfg.pin_d3  = 42;
    cfg.pin_d4  = 45;
    cfg.pin_d5  = 46;
    cfg.pin_d6  = 47;
    cfg.pin_d7  = 48;

    cfg.pin_wr = 8;
    cfg.pin_rd = 9;
    cfg.pin_rs = 7;   // DC

    cfg.freq_write = 20000000;

    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);

    // ---------------------------
    // PANEL
    // ---------------------------
    auto pcfg = _panel_instance.config();

    pcfg.pin_cs   = 6;
    pcfg.pin_rst  = 5;
    pcfg.pin_busy = -1;

    pcfg.memory_width  = 320;
    pcfg.memory_height = 170;

    pcfg.panel_width  = 320;
    pcfg.panel_height = 170;

    pcfg.offset_x = 0;
    pcfg.offset_y = 0;

    pcfg.invert = true;
    pcfg.rgb_order = false;

    _panel_instance.config(pcfg);

    setPanel(&_panel_instance);
  }
};