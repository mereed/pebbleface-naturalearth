#include "pebble.h"

static const uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_CLEAR_DAY,
  RESOURCE_ID_CLEAR_NIGHT,
  RESOURCE_ID_WINDY,
  RESOURCE_ID_COLD,
  RESOURCE_ID_PARTLY_CLOUDY_DAY,
  RESOURCE_ID_PARTLY_CLOUDY_NIGHT,
  RESOURCE_ID_HAZE,
  RESOURCE_ID_CLOUD,
  RESOURCE_ID_RAIN,
  RESOURCE_ID_SNOW,
  RESOURCE_ID_HAIL,
  RESOURCE_ID_CLOUDY,
  RESOURCE_ID_STORM,
  RESOURCE_ID_NA,
};

enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,         // TUPLE_INT
  WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
  BLUETOOTHVIBE_KEY = 0x2,
  HOURLYVIBE_KEY = 0x3,
};

Window *window;
static Layer *window_layer;

static GBitmap *Map_Night;
static GBitmap *Map_Day;
static BitmapLayer* Map_Layer;

BitmapLayer *icon_layer;
GBitmap *icon_bitmap = NULL;
TextLayer *temp_layer;

TextLayer *text_day_layer;
TextLayer *text_date_layer;
TextLayer *text_time_layer;
TextLayer *battery_text_layer;

static GFont custom_font_time;
static GFont custom_font_tz;
static GFont custom_font_date;

BitmapLayer *layer_conn_img;
GBitmap *img_bt_connect;
GBitmap *img_bt_disconnect;

static int day = 0;

static int hourlyvibe;
static int bluetoothvibe;

static AppSync sync;
static uint8_t sync_buffer[128];

static char s_region_string[32] = "";          // Must be 32 characters large for longest option
static TextLayer *s_region_layer = NULL;

static bool appStarted = false;



static long catoi(const char *s) {
  const char *p = s, *q;
  long n = 0;
  int sign = 1, k = 1;
  if (p != NULL) {
      if (*p != '\0') {
          if ((*p == '+') || (*p == '-')) {
              if (*p++ == '-') sign = -1;
          }
          for (q = p; (*p != '\0'); p++);
          for (--p; p >= q; --p, k *= 10) n += (*p - '0') * k;
      }
  }
  return n * sign;
}

void get_tzone () {
	
  // grab timezone region name
		
#if defined(PBL_SDK_3)
	clock_get_timezone(s_region_string, TIMEZONE_NAME_LENGTH);

#else
    snprintf(s_region_string, sizeof(s_region_string), "World Timezone N/A");
#endif
    layer_set_hidden(text_layer_get_layer(s_region_layer),false);
    layer_mark_dirty(text_layer_get_layer(s_region_layer));
		
}	
	
static void sync_tuple_changed_callback(const uint32_t key,
                                        const Tuple* new_tuple,
                                        const Tuple* old_tuple,
                                        void* context) {

  // App Sync keeps new_tuple in sync_buffer, so we may use it directly
  switch (key) {
    case WEATHER_ICON_KEY:
      if (icon_bitmap) {
        gbitmap_destroy(icon_bitmap);
      }

      icon_bitmap = gbitmap_create_with_resource(
          WEATHER_ICONS[new_tuple->value->uint8]);
      bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
    break;

    case WEATHER_TEMPERATURE_KEY:
      text_layer_set_text(temp_layer, new_tuple->value->cstring);
    break;

    case BLUETOOTHVIBE_KEY:
      bluetoothvibe = new_tuple->value->uint8 != 0;
      persist_write_bool(BLUETOOTHVIBE_KEY, bluetoothvibe);
    break;
	  
    case HOURLYVIBE_KEY:
      hourlyvibe = new_tuple->value->uint8 != 0;
      persist_write_bool(HOURLYVIBE_KEY, hourlyvibe);
    break;
  }
}

/*
void bluetooth_connection_changed(bool connected) {
  static bool _connected = true;

  // This seemed to get called twice on disconnect
  if (!connected && _connected) {
    vibes_short_pulse();
    layer_set_hidden(text_layer_get_layer(temp_layer), true);

    if (icon_bitmap) {
      gbitmap_destroy(icon_bitmap);
    }

    icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_NO_BT);
    bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
  }
  _connected = connected;
}

*/

static void toggle_bluetooth(bool connected) {

if (!connected) {
		bitmap_layer_set_bitmap(layer_conn_img, img_bt_disconnect);
	    layer_set_hidden(bitmap_layer_get_layer(icon_layer), true);
	    layer_set_hidden(text_layer_get_layer(temp_layer), true);

     } else {
		bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);
	    layer_set_hidden(bitmap_layer_get_layer(icon_layer), false);
	    layer_set_hidden(text_layer_get_layer(temp_layer), false);	
	}
	
   if (appStarted && bluetoothvibe) {
        vibes_short_pulse();
	}
}

void hourvibe (struct tm *tick_time) {
  if (appStarted && hourlyvibe) {
    //vibe!
    vibes_short_pulse();
  }
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Need to be static because they're used by the system later.
  static char date_text[] = "Xxx 00";
  static char time_text[] = "00:00";
  static int yday = -1;
  static char dp[] = "18";

  char *time_format;

	
	if (units_changed & HOUR_UNIT) {
    hourvibe(tick_time);
}
	
  // Only update the date when it has changed.
  if (yday != tick_time->tm_yday) {
  
    strftime(date_text, sizeof(date_text), "%b %e", tick_time);
    text_layer_set_text(text_date_layer, date_text);
	  
  strftime(dp, sizeof(dp), "%H", tick_time);

  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  strftime(time_text, sizeof(time_text), time_format, tick_time);

  // Handle lack of non-padded hour format string for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    text_layer_set_text(text_time_layer, time_text + 1);
  } else {
    text_layer_set_text(text_time_layer, time_text);
  }
	  
	if (catoi(dp)>=18 || catoi(dp)<6) {
	  day=0;
  }else{
	  day=1;
  }
	  if (day == 0) {
#ifdef PBL_COLOR
		text_layer_set_text_color(s_region_layer, GColorPictonBlue     );
		text_layer_set_text_color(text_date_layer, GColorOxfordBlue );
		text_layer_set_text_color(temp_layer, GColorOxfordBlue );
		text_layer_set_text_color(battery_text_layer, GColorOxfordBlue);
		window_set_background_color(window, GColorOxfordBlue);		  
#else
		text_layer_set_text_color(s_region_layer, GColorWhite );
		text_layer_set_text_color(temp_layer, GColorWhite);
		text_layer_set_text_color(battery_text_layer, GColorWhite);
		text_layer_set_text_color(text_date_layer, GColorWhite);
		window_set_background_color(window, GColorBlack);
#endif
		bitmap_layer_set_bitmap(Map_Layer, Map_Night);

	  } else {
#ifdef PBL_COLOR
		text_layer_set_text_color(s_region_layer, GColorPictonBlue      );
		text_layer_set_text_color(temp_layer, GColorOxfordBlue ); 
		text_layer_set_text_color(battery_text_layer, GColorOxfordBlue );
		text_layer_set_text_color(text_date_layer, GColorOxfordBlue); 
		window_set_background_color(window, GColorOxfordBlue);
#else
		text_layer_set_text_color(s_region_layer, GColorWhite );
		text_layer_set_text_color(temp_layer, GColorWhite); 
		text_layer_set_text_color(battery_text_layer, GColorWhite);
		text_layer_set_text_color(text_date_layer, GColorWhite);
		window_set_background_color(window, GColorBlack);
#endif
		bitmap_layer_set_bitmap(Map_Layer, Map_Day);
	  }
  }
}

// battery
void update_battery_state(BatteryChargeState battery_state) {
  static char battery_text[] = "x100%";
	
	
  if ( battery_state.is_charging ) {
	snprintf(battery_text, sizeof(battery_text), "+%d", battery_state.charge_percent);			   
  }   else {
	snprintf(battery_text, sizeof(battery_text), "%d%%", battery_state.charge_percent);       
  }
  //    battery_state.charge_percent);
  text_layer_set_text(battery_text_layer, battery_text);
}

void handle_init(void) {
  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  Layer *window_layer = window_get_root_layer(window);

  // Background image
	
#ifdef PBL_PLATFORM_CHALK
  Map_Night = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND2);
  Map_Day = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND1);
  Map_Layer = bitmap_layer_create(GRect(0, 80, 180, 100));
  bitmap_layer_set_background_color(Map_Layer, GColorOxfordBlue);
  bitmap_layer_set_bitmap(Map_Layer, Map_Night);
  layer_add_child(window_layer, bitmap_layer_get_layer(Map_Layer));
#endif
	
#ifdef PBL_PLATFORM_BASALT
  Map_Night = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND2);
  Map_Day = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND1);
  Map_Layer = bitmap_layer_create(GRect(0, 68, 144, 100));
  bitmap_layer_set_background_color(Map_Layer, GColorOxfordBlue);
  bitmap_layer_set_bitmap(Map_Layer, Map_Night);
  layer_add_child(window_layer, bitmap_layer_get_layer(Map_Layer));
#endif
	
#ifdef PBL_PLATFORM_APLITE
  Map_Night = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND2);
  Map_Day = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND1);
  Map_Layer = bitmap_layer_create(GRect(0, 68, 144, 100));
  bitmap_layer_set_background_color(Map_Layer, GColorBlack);
  bitmap_layer_set_bitmap(Map_Layer, Map_Night);
  layer_add_child(window_layer, bitmap_layer_get_layer(Map_Layer));
#endif	
	
	
  custom_font_tz= fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CUSTOM_14));
  custom_font_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CUSTOM_48));
  custom_font_date = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CUSTOM_12));

  img_bt_connect     = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_ON);
  img_bt_disconnect  = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_OFF);
	
#ifdef PBL_PLATFORM_CHALK
  layer_conn_img  = bitmap_layer_create(GRect(84, 151, 10, 13));
#else
  layer_conn_img  = bitmap_layer_create(GRect(126, 155, 10, 13));
#endif	
  bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);
  layer_add_child(window_layer, bitmap_layer_get_layer(layer_conn_img)); 
	
	

  // Setup weather info
	
//  Layer *weather_holder = layer_create(GRect(0, 0, 144, 50));
//  layer_add_child(window_layer, weather_holder);

#ifdef PBL_PLATFORM_CHALK
  icon_layer = bitmap_layer_create(GRect(0, 0, 180, 44));
#else
  icon_layer = bitmap_layer_create(GRect(0, 0, 144, 44));
#endif	
  layer_add_child(window_layer, bitmap_layer_get_layer(icon_layer));

	
#ifdef PBL_PLATFORM_CHALK
  temp_layer = text_layer_create(GRect( 0, 150, 180, 30));
  text_layer_set_text_alignment(temp_layer, GTextAlignmentCenter);
#else
  temp_layer = text_layer_create(GRect(0, 154, 36, 20));
  text_layer_set_text_alignment(temp_layer, GTextAlignmentRight);
#endif	
  text_layer_set_text_color(temp_layer, GColorWhite);
  text_layer_set_background_color(temp_layer, GColorClear);
  text_layer_set_font(temp_layer, custom_font_date);
  layer_add_child(window_layer, text_layer_get_layer(temp_layer));

  // Initialize date & time text

#ifdef PBL_PLATFORM_CHALK
 text_date_layer = text_layer_create(GRect(0, 161, 180, 25));
 text_layer_set_text_alignment(text_date_layer, GTextAlignmentCenter);
#else
 text_date_layer = text_layer_create(GRect(40, 154, 100, 25));
 text_layer_set_text_alignment(text_date_layer, GTextAlignmentLeft);
#endif
  text_layer_set_text_color(text_date_layer, GColorWhite);
  text_layer_set_background_color(text_date_layer, GColorClear);
  text_layer_set_font(text_date_layer, custom_font_date);
  layer_add_child(window_layer, text_layer_get_layer(text_date_layer));


#ifdef PBL_PLATFORM_CHALK
  text_time_layer = text_layer_create(GRect( 0, 18, 180,  50));
#else
  text_time_layer = text_layer_create(GRect(0, 18, 144,  50));
#endif
  text_layer_set_text_color(text_time_layer, GColorWhite);
  text_layer_set_background_color(text_time_layer, GColorClear);
  text_layer_set_font(text_time_layer, custom_font_time);
  text_layer_set_text_alignment(text_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_time_layer));

// battery text
	
#ifdef PBL_PLATFORM_CHALK
  battery_text_layer = text_layer_create(GRect(0, 0, 0, 0));
#else
  battery_text_layer = text_layer_create(GRect(80, 154, 40, 30));
#endif	
  text_layer_set_text_color(battery_text_layer, GColorWhite);
  text_layer_set_background_color(battery_text_layer, GColorClear);
  text_layer_set_font(battery_text_layer, custom_font_date);
  text_layer_set_text_alignment(battery_text_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(battery_text_layer));

  // timezone text
	
#ifdef PBL_PLATFORM_CHALK
  s_region_layer = text_layer_create(GRect(0, 67, 180, 16));	
#else
  s_region_layer = text_layer_create(GRect(0, 66, 144, 16));	
#endif
  text_layer_set_text(s_region_layer, s_region_string);
  text_layer_set_background_color(s_region_layer, GColorClear);
  text_layer_set_text_color(s_region_layer, GColorWhite);
  text_layer_set_font(s_region_layer, custom_font_tz);
  text_layer_set_text_alignment(s_region_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_region_layer));	
  layer_set_hidden(text_layer_get_layer(s_region_layer), true);
	
	
  get_tzone();

	
	// Setup messaging
  const int inbound_size = 128;
  const int outbound_size = 128;
  app_message_open(inbound_size, outbound_size);

  Tuplet initial_values[] = {
    TupletInteger(WEATHER_ICON_KEY, (uint8_t) 13),
    TupletCString(WEATHER_TEMPERATURE_KEY, ""),
    TupletInteger(BLUETOOTHVIBE_KEY, persist_read_bool(BLUETOOTHVIBE_KEY)),
    TupletInteger(HOURLYVIBE_KEY, persist_read_bool(HOURLYVIBE_KEY)),
  };

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values,
                ARRAY_LENGTH(initial_values), sync_tuple_changed_callback,
                NULL, NULL);

  appStarted = true;

  // Subscribe to notifications
  bluetooth_connection_service_subscribe(&toggle_bluetooth);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  battery_state_service_subscribe(update_battery_state);

  // Update the battery on launch
  update_battery_state(battery_state_service_peek());
  toggle_bluetooth(bluetooth_connection_service_peek());

  // TODO: Update display here to avoid blank display on launch?
}

void handle_deinit(void) {
	
  app_sync_deinit( &sync );
	
  bluetooth_connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
	
  layer_remove_from_parent(bitmap_layer_get_layer(Map_Layer));
  bitmap_layer_destroy(Map_Layer);
  gbitmap_destroy(Map_Day);
  gbitmap_destroy(Map_Night);
  Map_Day = NULL;
  Map_Night = NULL;
	
  layer_remove_from_parent(bitmap_layer_get_layer(layer_conn_img));
  bitmap_layer_destroy(layer_conn_img);
  gbitmap_destroy(img_bt_connect);
  gbitmap_destroy(img_bt_disconnect);
  img_bt_connect = NULL;
  img_bt_disconnect = NULL;

  text_layer_destroy(text_date_layer);
  text_layer_destroy(text_time_layer);
  text_layer_destroy(temp_layer);
  text_layer_destroy(battery_text_layer);
  text_layer_destroy(s_region_layer);
	
  layer_remove_from_parent(bitmap_layer_get_layer(icon_layer));
  bitmap_layer_destroy(icon_layer);
  gbitmap_destroy(icon_bitmap);
  icon_bitmap = NULL;
	
  fonts_unload_custom_font( custom_font_time );
  fonts_unload_custom_font( custom_font_tz );
  fonts_unload_custom_font( custom_font_date );
	
  layer_remove_from_parent(window_layer);
  layer_destroy(window_layer);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
