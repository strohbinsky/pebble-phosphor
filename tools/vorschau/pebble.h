// Minimal-Nachbau der Pebble-API, um main.c auf dem Mac zu übersetzen und das Display als PNG zu rendern.
// Zeit, Zeitzone, Akku, Schritte, Puls und Handy-Daten setzt vorschau.c (Testwerte).
#pragma once
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
typedef struct { uint8_t r, g, b; } GColor;
typedef struct GContext GContext;
typedef struct Layer Layer; typedef struct Window Window;
typedef int TimeUnits;
typedef struct { void (*load)(Window *); void (*unload)(Window *); } WindowHandlers;
typedef void (*LayerUpdateProc)(Layer *, GContext *);
enum { SECOND_UNIT = 1, MINUTE_UNIT = 2, APP_MSG_OK = 0, GCornerNone = 0, APP_LOG_LEVEL_WARNING = 1 };
enum { MESSAGE_KEY_BEREIT = 1, MESSAGE_KEY_ANFRAGE, MESSAGE_KEY_WETTER, MESSAGE_KEY_TAG, MESSAGE_KEY_STAND,
       MESSAGE_KEY_IATA, MESSAGE_KEY_BREITE, MESSAGE_KEY_FEHLER, MESSAGE_KEY_SONNE = 10, MESSAGE_KEY_GEZEITEN = 14, MESSAGE_KEY_SKIN, MESSAGE_KEY_PARK1, MESSAGE_KEY_PARK2, MESSAGE_KEY_SPRACHE };
#define GRect(x, y, w, h) ((GRect){{(x), (y)}, {(w), (h)}})
#define GPoint(x, y) ((GPoint){(x), (y)})
#define GColorFromHEX(v) ((GColor){((v) >> 16) & 255, ((v) >> 8) & 255, (v) & 255})
#define GColorBlack ((GColor){0, 0, 0})
#define GColorWhite ((GColor){255, 255, 255})
#define APP_LOG(l, ...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
extern uint8_t FB[228][200][3];
static GColor s_fill, s_stroke; static int s_sw = 1;
static inline void px(int x, int y, GColor k) { if (x >= 0 && y >= 0 && x < 200 && y < 228) { FB[y][x][0] = k.r; FB[y][x][1] = k.g; FB[y][x][2] = k.b; } }
static inline void graphics_context_set_fill_color(GContext *c, GColor k) { s_fill = k; }
static inline void graphics_context_set_stroke_color(GContext *c, GColor k) { s_stroke = k; }
static inline void graphics_context_set_stroke_width(GContext *c, int w) { s_sw = w; }
static inline void graphics_context_set_antialiased(GContext *c, bool a) {}
static inline void graphics_fill_rect(GContext *c, GRect r, int rad, int m) { for (int y = 0; y < r.size.h; y++) for (int x = 0; x < r.size.w; x++) px(r.origin.x + x, r.origin.y + y, s_fill); }
static inline void graphics_draw_pixel(GContext *c, GPoint p) { px(p.x, p.y, s_stroke); }
static inline void graphics_draw_line(GContext *c, GPoint a, GPoint b) {   // Bresenham, Strichbreite als Quadrat
  int x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y, dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (;;) {
    for (int i = 0; i < s_sw; i++) for (int j = 0; j < s_sw; j++) px(x0 - s_sw / 2 + i, y0 - s_sw / 2 + j, s_stroke);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; }
  }
}
// Trigonometrie wie auf der Uhr
#define TRIG_MAX_ANGLE 0x10000
#define TRIG_MAX_RATIO 0xffff
static inline int32_t cos_lookup(int32_t a) { return (int32_t)lround(cos(a * 2 * M_PI / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO); }
// Akku, Gesundheit
typedef struct { uint8_t charge_percent; bool is_charging; bool is_plugged; } BatteryChargeState;
typedef int32_t HealthValue; typedef enum { HealthMetricStepCount } HealthMetric;
typedef struct { uint8_t steps, orientation; uint16_t vmc; bool is_invalid: 1; uint8_t light: 3; uint8_t padding: 4; uint8_t heart_rate_bpm; uint8_t reserved[6]; } HealthMinuteData;
extern int FAKE_AKKU, FAKE_SCHRITTE, FAKE_PULS[360];
static inline BatteryChargeState battery_state_service_peek(void) { return (BatteryChargeState){ (uint8_t)FAKE_AKKU, false, false }; }
static inline void battery_state_service_subscribe(void (*h)(BatteryChargeState)) {}
static inline void battery_state_service_unsubscribe(void) {}
static inline HealthValue health_service_sum_today(HealthMetric m) { return FAKE_SCHRITTE; }
static inline uint32_t health_service_get_minute_history(HealthMinuteData *d, uint32_t n, time_t *s, time_t *e) {
  for (uint32_t i = 0; i < n && i < 360; i++) { memset(&d[i], 0, sizeof d[i]); d[i].heart_rate_bpm = (uint8_t)FAKE_PULS[i]; d[i].is_invalid = FAKE_PULS[i] == 0; }
  return n < 360 ? n : 360;
}
// Speicher, Nachrichten, Fenster: ohne Wirkung
static inline bool persist_exists(uint32_t k) { return false; }
static inline int32_t persist_read_int(uint32_t k) { return 0; }
static inline int persist_write_int(uint32_t k, int32_t v) { return 0; }
static inline int persist_delete(uint32_t k) { return 0; }
static inline int persist_read_data(uint32_t k, void *b, size_t n) { return 0; }
static inline int persist_write_data(uint32_t k, const void *b, size_t n) { return 0; }
static inline int persist_read_string(uint32_t k, char *b, size_t n) { return 0; }
static inline int persist_write_string(uint32_t k, const char *s) { return 0; }
typedef struct DictionaryIterator DictionaryIterator;
typedef struct { int32_t int32; char cstring[32]; uint8_t data[256]; } TupleValue;
typedef struct { uint16_t length; TupleValue *value; } Tuple;
static inline Tuple *dict_find(DictionaryIterator *it, int k) { return NULL; }
static inline int app_message_outbox_begin(DictionaryIterator **it) { return 1; }
static inline void dict_write_int32(DictionaryIterator *it, int k, int32_t v) {}
static inline int app_message_outbox_send(void) { return 1; }
static inline void app_message_register_inbox_received(void (*h)(DictionaryIterator *, void *)) {}
static inline void app_message_open(int a, int b) {}
static inline GRect layer_get_bounds(Layer *l) { return GRect(0, 0, 200, 228); }
static inline void layer_mark_dirty(Layer *l) {}
static inline Layer *window_get_root_layer(Window *w) { return NULL; }
static inline Layer *layer_create(GRect r) { return NULL; }
static inline void layer_set_update_proc(Layer *l, LayerUpdateProc p) {}
static inline void layer_add_child(Layer *a, Layer *b) {}
static inline void layer_destroy(Layer *l) {}
static inline Window *window_create(void) { return NULL; }
static inline void window_set_background_color(Window *w, GColor c) {}
static inline void window_set_window_handlers(Window *w, WindowHandlers h) {}
static inline void window_stack_push(Window *w, bool a) {}
static inline void window_destroy(Window *w) {}
static inline void tick_timer_service_subscribe(int u, void (*h)(struct tm *, TimeUnits)) {}
typedef int AccelAxisType;
typedef struct AppTimer AppTimer;
static inline AppTimer *app_timer_register(uint32_t ms, void (*cb)(void *), void *d) { return NULL; }
static inline bool app_timer_reschedule(AppTimer *t, uint32_t ms) { return true; }
static inline void app_timer_cancel(AppTimer *t) {}
static inline void accel_tap_service_subscribe(void (*h)(AccelAxisType, int32_t)) {}
static inline void accel_tap_service_unsubscribe(void) {}
// Handy-Verbindung (seit 0.24): Umgebungsvariable OHNE_HANDY simuliert „Handy weg“
typedef struct { void (*pebble_app_connection_handler)(bool); } ConnectionHandlers;
static inline bool connection_service_peek_pebble_app_connection(void) { return getenv("OHNE_HANDY") == NULL; }
static inline void connection_service_subscribe(ConnectionHandlers h) {}
static inline void connection_service_unsubscribe(void) {}
static inline void tick_timer_service_unsubscribe(void) {}
static inline void app_event_loop(void) {}
extern time_t FAKE_NOW;
#define time(x) (FAKE_NOW)
// Systemschriften für die Ansicht Klar (klar.h): nur Platzhalter, damit main.c übersetzt. Text zeichnet der Nachbau nicht —
// die Ansicht Klar wird im Emulator geprüft, hier nur Phosphor.
typedef const char *GFont;
typedef int GTextOverflowMode; typedef int GTextAlignment;
enum { GTextOverflowModeFill, GTextAlignmentLeft };
#define FONT_KEY_BITHAM_30_BLACK "b30"
#define FONT_KEY_GOTHIC_14_BOLD "g14b"
#define FONT_KEY_GOTHIC_18_BOLD "g18b"
#define FONT_KEY_GOTHIC_24_BOLD "g24b"
#define FONT_KEY_GOTHIC_14 "g14"
#define FONT_KEY_GOTHIC_18 "g18"
#define FONT_KEY_GOTHIC_24 "g24"
#define FONT_KEY_ROBOTO_BOLD_SUBSET_49 "r49"
#define FONT_KEY_LECO_42_NUMBERS "l42"
#define FONT_KEY_LECO_28_LIGHT_NUMBERS "l28"
#define FONT_KEY_GOTHIC_28_BOLD "g28b"
#define FONT_KEY_LECO_38_BOLD_NUMBERS "l38"
static inline GFont fonts_get_system_font(const char *k) { return k; }
static inline GSize graphics_text_layout_get_content_size(const char *s, GFont f, GRect r, int o, int a) { GSize z = { (int16_t)(strlen(s) * 8), 14 }; return z; }
static inline void graphics_draw_text(GContext *c, const char *s, GFont f, GRect r, int o, int a, void *l) {}
static inline void graphics_context_set_text_color(GContext *c, GColor col) {}
static inline void graphics_draw_rect(GContext *c, GRect r) {}
static inline void graphics_draw_round_rect(GContext *c, GRect r, int rad) {   // Umriss ohne die vier Eckpunkte
  for (int x = 1; x < r.size.w - 1; x++) { px(r.origin.x + x, r.origin.y, s_stroke); px(r.origin.x + x, r.origin.y + r.size.h - 1, s_stroke); }
  for (int y = 1; y < r.size.h - 1; y++) { px(r.origin.x, r.origin.y + y, s_stroke); px(r.origin.x + r.size.w - 1, r.origin.y + y, s_stroke); }
}
