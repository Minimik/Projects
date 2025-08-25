#include <lvgl.h>
#include <TFT_eSPI.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

TFT_eSPI tft = TFT_eSPI();

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * 10];  // Zeilenpuffer
static lv_color_t buf2[SCREEN_WIDTH * 10];

// LVGL → TFT_eSPI Flush
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  int32_t w = (area->x2 - area->x1 + 1);
  int32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

// Zeiger global speichern
static lv_meter_indicator_t *needle;
static lv_meter_scale_t *scale;
static lv_obj_t *meter;

void setup() {
  Serial.begin(115200);
  lv_init();
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // LVGL Buffer
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_WIDTH * 10);

  // Display Treiber registrieren
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Jetzt: Gauge anlegen
  meter = lv_meter_create(lv_scr_act());
  lv_obj_center(meter);
  lv_obj_set_size(meter, 200, 200);

  // Skala
  scale = lv_meter_add_scale(meter);
  lv_meter_set_scale_range(meter, scale, 0, 8000, 270, 135);  // 0..100, 270° Bogen, Start 135°
  lv_meter_set_scale_ticks(meter, scale, 9, 2, 10, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(meter, scale, 4, 4, 15, lv_color_black(), 10);

  // Farbbereiche als Arc-Objekte
  // Farbbögen als Arc-Indikatoren
  lv_meter_indicator_t *green = lv_meter_add_arc(meter, scale, 15, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_meter_indicator_t *yellow = lv_meter_add_arc(meter, scale, 15, lv_palette_main(LV_PALETTE_YELLOW), -10);
  lv_meter_indicator_t *red = lv_meter_add_arc(meter, scale, 15, lv_palette_main(LV_PALETTE_RED), -20);

  // // Grün
  // lv_obj_t *green_arc = lv_arc_create(lv_scr_act());
  // lv_arc_set_angles(green_arc, 135, 297);
  // lv_obj_set_style_arc_width(green_arc, 15, 0);
  // lv_obj_set_style_arc_color(green_arc, lv_palette_main(LV_PALETTE_GREEN), 0);
  // lv_obj_align(green_arc, LV_ALIGN_CENTER, 0, 0);
  // lv_arc_set_bg_angles(green_arc, 0, 0);
  // lv_arc_set_value(green_arc, 100);

  // // Gelb
  // lv_obj_t *yellow_arc = lv_arc_create(lv_scr_act());
  // lv_arc_set_angles(yellow_arc, 297, 378);
  // lv_obj_set_style_arc_width(yellow_arc, 15, 0);
  // lv_obj_set_style_arc_color(yellow_arc, lv_palette_main(LV_PALETTE_YELLOW), 0);
  // lv_obj_align(yellow_arc, LV_ALIGN_CENTER, 0, 0);
  // lv_arc_set_bg_angles(yellow_arc, 0, 0);
  // lv_arc_set_value(yellow_arc, 100);

  // // Rot
  // lv_obj_t *red_arc = lv_arc_create(lv_scr_act());
  // lv_arc_set_angles(red_arc, 378, 405);
  // lv_obj_set_style_arc_width(red_arc, 15, 0);
  // lv_obj_set_style_arc_color(red_arc, lv_palette_main(LV_PALETTE_RED), 0);
  // lv_obj_align(red_arc, LV_ALIGN_CENTER, 0, 0);
  // lv_arc_set_bg_angles(red_arc, 0, 0);
  // lv_arc_set_value(red_arc, 100);

  // Nadel
  //static lv_meter_indicator_t *needle;
  needle = lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_RED), -10);

  // Demo: Wert setzen
  //lv_meter_set_indicator_value(meter, needle, 0);
  //lv_meter_set_indicator_value(meter, needle, 2000);  // Beispielwert mittig

  // Werte setzen (für statische Farbbögen)
  lv_meter_set_indicator_value(meter, green, 4800);
  lv_meter_set_indicator_value(meter, yellow, 7200);
  lv_meter_set_indicator_value(meter, red, 8000);

  // Zufall initialisieren
  randomSeed(analogRead(0));
}

int oldValue = 0;
unsigned long lastUpdate = 0;

unsigned long lastTick = 0;

void loop() {

  // LVGL Tick simulieren
  unsigned long now = millis();
  if (now - lastTick >= 1) {
    lv_tick_inc(now - lastTick);
    lastTick = now;
  }

  lv_timer_handler();  // LVGL laufen lassen

  // alle 500ms neuen Wert setzen
  if (millis() - lastUpdate > 500) {
    unsigned int randomValue = random(0, 8001);

    // Animation von altem zu neuem Wert
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, needle);
    lv_anim_set_values(&a, oldValue, randomValue);
    lv_anim_set_time(&a, 250);  // Animationszeit
    lv_anim_set_exec_cb(&a, [](void *ind, int32_t v) {
      lv_meter_set_indicator_value(meter, (lv_meter_indicator_t *)ind, v);
    });
    lv_anim_start(&a);

    // Nadel direkt setzen (keine Animation)
    //lv_meter_set_indicator_value(meter, needle, randomValue);

    Serial.print("OldValue: ");
    Serial.print(oldValue);
    Serial.print("\trandValue: ");
    Serial.println(randomValue);

    oldValue = randomValue;
    lastUpdate = millis();
  }




  //delay(500);  // neue Zufallswerte alle 0,5 Sekunden
}
