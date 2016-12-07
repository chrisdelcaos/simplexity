// Simplistic
// Made by Ben Chapman-Kish from 2015-03-13 to 2015-03-14
#include "pebble.h"
// Don't judge me for this program, especially if it's the future
// I'll probably be a much better C programmer in the future, I promise

static Window *s_main_window;
static TextLayer *s_day_layer, *s_date_layer, *s_time_layer;
static Layer *s_line_layer, *s_bat_rect_layer;
static BitmapLayer *s_bt_layer, *s_bat_layer, *s_bat_charging_layer;
static GBitmap *s_bt_on_bitmap, *s_bt_off_bitmap, *s_bat_bord_bitmap, *s_bat_charging_bitmap, *s_bat_charged_bitmap;
static BatteryChargeState charge;

static void line_layer_update_callback(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void bat_rect_fill(Layer *layer, GContext* ctx) {
  // Took way too long to get this working...
	if (!charge.is_plugged || charge.is_charging) {
		graphics_context_set_compositing_mode(ctx, GCompOpAssign);
		graphics_context_set_stroke_color(ctx, GColorBlack);
		graphics_context_set_fill_color(ctx, GColorWhite);
		// This GRect's origin is based on the origin of its parent layer
		// Why didn't I realize this sooner? Duh, that must be what the context is for
  	graphics_fill_rect(ctx, GRect(0,0, (uint8_t)(charge.charge_percent/10), 4), 0, GCornerNone);
	}
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Need to be static because they're used by the system later.
  static char s_time_text[] = "12:00";
  static char s_date_text[] = "Xxxxxxxxx 00";
	static char s_day_text[] = "Xxxxxxxxx,";

	// Date is currently updated every minute, should be changed later.
	strftime(s_day_text, sizeof(s_day_text), "%A,", tick_time);
  text_layer_set_text(s_day_layer, s_day_text);
	
  strftime(s_date_text, sizeof(s_date_text), "%B %e", tick_time);
  text_layer_set_text(s_date_layer, s_date_text);
	

  char *time_format;
  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }
  strftime(s_time_text, sizeof(s_time_text), time_format, tick_time);

  // Handle lack of non-padded hour format string for twelve hour clock.
  if (!clock_is_24h_style() && (s_time_text[0] == '0')) {
    memmove(s_time_text, &s_time_text[1], sizeof(s_time_text) - 1);
  }
  text_layer_set_text(s_time_layer, s_time_text);
}

static void bt_handler(bool connected) {
  // Show current connection state
  if (connected) {
    bitmap_layer_set_bitmap(s_bt_layer, s_bt_on_bitmap);
  } else {
    bitmap_layer_set_bitmap(s_bt_layer, s_bt_off_bitmap);
  }
	// For some reason these don't work with vibes after light...
	// Must be hidden in the docs somewhere explaining why
	vibes_long_pulse();
	light_enable_interaction();
}

static void battery_handler(BatteryChargeState new_state) {
	// Display battery charge icon
	// It's ugly but I can't think of a better way right now
	if (new_state.is_charging) {
		layer_set_hidden(bitmap_layer_get_layer(s_bat_charging_layer), false);
		bitmap_layer_set_bitmap(s_bat_layer, s_bat_bord_bitmap);
	} else if (new_state.is_plugged) {
		layer_set_hidden(bitmap_layer_get_layer(s_bat_charging_layer), true);
		bitmap_layer_set_bitmap(s_bat_layer, s_bat_charged_bitmap);
	} else {
		layer_set_hidden(bitmap_layer_get_layer(s_bat_charging_layer), true);
		bitmap_layer_set_bitmap(s_bat_layer, s_bat_bord_bitmap);
	}
	
	/* If I wanted to display the charge percent (for future reference):
	static char s_battery_buffer[16];
	snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%% charged", new_state.charge_percent);
	text_layer_set_text(battery_layer, s_battery_buffer); */
	
	charge=new_state;
	layer_mark_dirty(s_bat_rect_layer);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

	// Day of week
	s_day_layer = text_layer_create(GRect(8, 44, 144-8, 168-44));
  text_layer_set_text_color(s_day_layer, GColorWhite);
  text_layer_set_background_color(s_day_layer, GColorClear);
  text_layer_set_font(s_day_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  layer_add_child(window_layer, text_layer_get_layer(s_day_layer));
	
	// Month and day
  s_date_layer = text_layer_create(GRect(8, 68, 144-8, 168-68));
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

	// The time
  s_time_layer = text_layer_create(GRect(7, 92, 144-7, 168-92));
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

	// Line seperating time and date
  s_line_layer = layer_create(GRect(8, 97, 144-20, 2));
  layer_set_update_proc(s_line_layer, line_layer_update_callback);
  layer_add_child(window_layer, s_line_layer);
	
	// Bluetooth indicator
	s_bt_on_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BT_ON);
	s_bt_off_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BT_OFF);
	
	s_bt_layer = bitmap_layer_create(GRect(3, 3, 12, 11));
	if (bluetooth_connection_service_peek()) {
  bitmap_layer_set_bitmap(s_bt_layer, s_bt_on_bitmap);
  } else {
  bitmap_layer_set_bitmap(s_bt_layer, s_bt_off_bitmap);
  }
	
  bitmap_layer_set_alignment(s_bt_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bt_layer));
	
	// Battery indicator
	s_bat_bord_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_BORDER);
	s_bat_charging_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_CHARGING);
	s_bat_charged_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_CHARGED);
	
	s_bat_layer = bitmap_layer_create(GRect(144-18, 4, 15, 8));
	bitmap_layer_set_bitmap(s_bat_layer, s_bat_bord_bitmap);
  bitmap_layer_set_alignment(s_bat_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bat_layer));
	
	s_bat_charging_layer = bitmap_layer_create(GRect(144-25, 4, 5, 8));
	bitmap_layer_set_bitmap(s_bat_charging_layer, s_bat_charging_bitmap);
  bitmap_layer_set_alignment(s_bat_charging_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bat_charging_layer));
	layer_set_hidden(bitmap_layer_get_layer(s_bat_charging_layer), true);
	
	s_bat_rect_layer = layer_create(GRect(144-16, 6, 10, 4));
  layer_set_update_proc(s_bat_rect_layer, &bat_rect_fill);
	layer_add_child(window_layer, s_bat_rect_layer);
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_day_layer);
	text_layer_destroy(s_date_layer);
  text_layer_destroy(s_time_layer);
	layer_destroy(s_line_layer);
	
	bitmap_layer_destroy(s_bt_layer);
  gbitmap_destroy(s_bt_on_bitmap);
	gbitmap_destroy(s_bt_off_bitmap);
	
	layer_destroy(s_bat_rect_layer);
	bitmap_layer_destroy(s_bat_layer);
	bitmap_layer_destroy(s_bat_charging_layer);
	
	gbitmap_destroy(s_bat_bord_bitmap);
	gbitmap_destroy(s_bat_charging_bitmap);
	gbitmap_destroy(s_bat_charged_bitmap);
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

	bluetooth_connection_service_subscribe(&bt_handler);
	battery_state_service_subscribe(&battery_handler);
  tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);
  
  // Prevent starting blank
	battery_handler(battery_state_service_peek());
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  handle_minute_tick(t, MINUTE_UNIT);
}

static void deinit() {
  window_destroy(s_main_window);
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
}

int main() {
  init();
  app_event_loop();
  deinit();
}