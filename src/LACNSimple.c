#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "http.h"

//#define MY_UUID { 0x8A, 0xA0, 0x6C, 0x84, 0x4F, 0x6F, 0x45, 0x3E, 0x9F, 0x75, 0xE3, 0xF1, 0x19, 0x21, 0xEB, 0x6B }
// Must use this ID for communication from HTTP Pebble to work.
#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x29, 0x08, 0x7A, 0xB9, 0xB6, 0x19}

#define WIDTH  144
#define HEIGHT 168
#define IND_WIDTH 11
#define IND_HEIGHT HEIGHT-17

#define TIME_ZONES 3

PBL_APP_INFO(MY_UUID,
             "LACNSimple", "IterationOne",
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);

Window window;

TextLayer text_date_layer;
TextLayer text_time_layer;
TextLayer text_day_layer;
TextLayer text_ampm_layer;
TextLayer text_z1_time_layer;
TextLayer text_z2_time_layer;
TextLayer text_z1_name_layer;
TextLayer text_z2_name_layer;

int got_tz = 0;
int utc_offset = 0;
char time_format[]="%I:%M";
int wday=0;
int is_ampm = 0;
char dot[] = ".";
uint32_t sys_unixtime;
PblTm lastTick;


typedef struct {
        const char * name;
        int offset;
        int has_dst;
} timezone_t;

static timezone_t timezones[TIME_ZONES] = {
    { .name = "US PST", .offset = -8 * 60, .has_dst=1 },
    { .name = "CHINA", .offset = 8 * 60, .has_dst=0 },
    { .name = "INDIA", .offset = 5 * 60 + 30, .has_dst=0 }
};


Layer line_layer;

// Function Prototypes

void line_layer_update_callback(Layer *me, GContext* ctx);
void handle_init(AppContextRef ctx);
void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t);
void set_phone_time(int32_t dst_offset, bool is_dst, uint32_t unixtime, const char* tz_name, void* context);
void string_format_time_with_offset(char * 	ptr, size_t 	maxsize, const char * 	format, const PblTm * timeptr, int tz_offset);
void line_layer_update_callback(Layer *me, GContext* ctx);
void update_time_zones(const PblTm * timeptr);

// -- The Pebble Main Function
void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,

    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    },
    .messaging_info = {
			.buffer_sizes = {
				.inbound = 124,
				.outbound = 256,
			}
		}

  };
  app_event_loop(params, &handlers);
}

// -- Handle Init
void handle_init(AppContextRef ctx) {

  static char day_text[]  = "S M T W R F S";
  static char ampm_text[] = "A P";
  window_init(&window, "LACNSimple");
  window_stack_push(&window, true );  
  window_set_background_color(&window, GColorBlack);

  resource_init_current_app(&APP_RESOURCES);

  GFont font_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UBUNTU_MEDIUM_14));
  GFont font_medium = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UBUNTU_MEDIUM_21));
  
  GFont font_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UBUNTU_MEDIUM_49));
  
  
  // The date
  text_layer_init(&text_date_layer, GRect(8, 68, WIDTH-16, 30));
  text_layer_set_text_color(&text_date_layer, GColorWhite);
  text_layer_set_background_color(&text_date_layer, GColorClear);
  text_layer_set_text_alignment(&text_date_layer, GTextAlignmentCenter);
  text_layer_set_font(&text_date_layer, font_medium);
  layer_add_child(&window.layer, &text_date_layer.layer);

  // The Time 
  text_layer_init(&text_time_layer, GRect(8, 92, WIDTH-16, HEIGHT-92));
  text_layer_set_text_color(&text_time_layer, GColorWhite);
  text_layer_set_background_color(&text_time_layer, GColorClear);
  text_layer_set_text_alignment(&text_time_layer, GTextAlignmentCenter);
  text_layer_set_font(&text_time_layer, font_large);
  layer_add_child(&window.layer, &text_time_layer.layer);

  // Line
  layer_init(&line_layer, window.layer.frame);
  line_layer.update_proc = &line_layer_update_callback;
  layer_add_child(&window.layer, &line_layer);
  
  // The AM PM
  text_layer_init(&text_ampm_layer, GRect(8, HEIGHT-16, WIDTH-16, 15));
  text_layer_set_text_color(&text_ampm_layer, GColorWhite);
  text_layer_set_background_color(&text_ampm_layer, GColorClear);
  text_layer_set_text_alignment(&text_ampm_layer, GTextAlignmentLeft);
  text_layer_set_font(&text_ampm_layer, font_small);
  text_layer_set_text(&text_ampm_layer, ampm_text);
  layer_add_child(&window.layer, &text_ampm_layer.layer);

  // The day
  text_layer_init(&text_day_layer, GRect(8, HEIGHT-16, WIDTH-16, 15));
  text_layer_set_text_color(&text_day_layer, GColorWhite);
  text_layer_set_background_color(&text_day_layer, GColorClear);
  text_layer_set_text_alignment(&text_day_layer, GTextAlignmentRight);
  text_layer_set_font(&text_day_layer, font_small);
  text_layer_set_text(&text_day_layer, day_text);
  layer_add_child(&window.layer, &text_day_layer.layer);

  
  // Zone 1 Time Layer
  text_layer_init(&text_z1_time_layer, GRect(0, 0, WIDTH/2, 30));
  text_layer_set_text_color(&text_z1_time_layer, GColorWhite);
  text_layer_set_background_color(&text_z1_time_layer, GColorClear);
  layer_set_frame(&text_z1_time_layer.layer, GRect(0, 0, WIDTH/2, 30));
  text_layer_set_text_alignment(&text_z1_time_layer, GTextAlignmentCenter);
  text_layer_set_font(&text_z1_time_layer, font_medium);
  layer_add_child(&window.layer, &text_z1_time_layer.layer);

  // Zone 1 Name Layer
  text_layer_init(&text_z1_name_layer, GRect(0, 25, WIDTH/2, 20));
  text_layer_set_text_color(&text_z1_name_layer, GColorWhite);
  text_layer_set_background_color(&text_z1_name_layer, GColorClear);
  text_layer_set_text_alignment(&text_z1_name_layer, GTextAlignmentCenter);
  text_layer_set_font(&text_z1_name_layer, font_small);
  layer_add_child(&window.layer, &text_z1_name_layer.layer);

  // Zone 2 Time Layer
  text_layer_init(&text_z2_time_layer, GRect(WIDTH/2+1,0, WIDTH/2-1, 30));
  text_layer_set_text_color(&text_z2_time_layer, GColorWhite);
  text_layer_set_background_color(&text_z2_time_layer, GColorClear);
  text_layer_set_text_alignment(&text_z2_time_layer, GTextAlignmentCenter);
  text_layer_set_font(&text_z2_time_layer, font_medium);
  layer_add_child(&window.layer, &text_z2_time_layer.layer);

  // Zone 2 Name Layer
  text_layer_init(&text_z2_name_layer, GRect(WIDTH/2+1,25, WIDTH/2-1, 20));
  text_layer_set_text_color(&text_z2_name_layer, GColorWhite);
  text_layer_set_background_color(&text_z2_name_layer, GColorClear);
  text_layer_set_text_alignment(&text_z2_name_layer, GTextAlignmentCenter);
  text_layer_set_font(&text_z2_name_layer, font_small);
  layer_add_child(&window.layer, &text_z2_name_layer.layer);


  if (clock_is_24h_style()) {
    memcpy(time_format, "%R", 2 + 1);
  } else {
    memcpy(time_format, "%I:%M", 5 + 1);
  }
  
  http_set_app_id(34255427); //         34525634);
	http_register_callbacks((HTTPCallbacks){
		.time=set_phone_time,
	}, NULL);
	
  http_time_request();

}

// -- Handle Minute Ticks
void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {

  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";
  static char date_text[] = "Xxxx 00 0000";


  // TODO: Only update the date when it's changed.
  string_format_time(date_text, sizeof(date_text), "%b %e %Y", t->tick_time);
  text_layer_set_text(&text_date_layer, date_text);
  

  //string_format_time(day_text, sizeof(day_text), "%A", t->tick_time);
  wday = t->tick_time->tm_wday;
  is_ampm = (t->tick_time->tm_hour<12)?0:1;
  
  string_format_time(time_text, sizeof(time_text), time_format, t->tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(&text_time_layer, time_text);
  
  update_time_zones(t->tick_time);
  lastTick = *(t->tick_time);
}

// Update Time Zones
void update_time_zones(const PblTm * timeptr) {
  static char z1_time_text[] = "00:00..";
  static char z2_time_text[] = "00:00..";

  if (got_tz==1 && timeptr) {
    // Z1 time text
    int zone = 0, tzi=0;
    for (tzi=0; tzi<TIME_ZONES; ++tzi) {
      int tzi_offset = timezones[tzi].offset;
      if (timezones[tzi].has_dst==1) { // Presumably you can set has_dst=2 and set its start and end dates here
        if (  ( timeptr->tm_mon + 1 > 3 || (timeptr->tm_mon + 1 == 3 && timeptr->tm_mday >= 10) ) 
             && ( timeptr->tm_mon + 1 < 11 || (timeptr->tm_mon + 1 == 11 && timeptr->tm_mday < 3) ) ) {
          tzi_offset+=60;
        }
      }
      if (tzi_offset != utc_offset) {
        zone++;
        
        if (zone==1) {
          string_format_time_with_offset(z1_time_text, sizeof(z1_time_text), time_format,timeptr, tzi_offset);
          text_layer_set_text(&text_z1_time_layer, z1_time_text);
          text_layer_set_text(&text_z1_name_layer, timezones[tzi].name);
          continue;
        } else if (zone==2) {
          string_format_time_with_offset(z2_time_text, sizeof(z2_time_text), time_format,timeptr, tzi_offset);
          text_layer_set_text(&text_z2_time_layer, z2_time_text);
          text_layer_set_text(&text_z2_name_layer, timezones[tzi].name);
          break;
        }
      }
    }
  }
}


// -- Phone Time Set from the call back
void set_phone_time(int32_t dst_offset, bool is_dst, uint32_t unixtime, const char* tz_name, void* context) {

	// Offset
	utc_offset = dst_offset / 60;
  sys_unixtime = unixtime;
  got_tz = 1;
  update_time_zones(&lastTick);
}

// -- Drawing Lines
void line_layer_update_callback(Layer *me, GContext* ctx) {

  graphics_context_set_stroke_color(ctx, GColorWhite);

  graphics_draw_line(ctx, GPoint(7, 97), GPoint(WIDTH-14, 97));
  graphics_draw_line(ctx, GPoint(7, 98), GPoint(WIDTH-14, 98));
  if (got_tz==1) {
    graphics_draw_line(ctx, GPoint(0, 45), GPoint(WIDTH, 45));
  }
  
  // Mark Day
  graphics_draw_line(ctx, GPoint(WIDTH-16-(7-wday)*IND_WIDTH+1, IND_HEIGHT), GPoint(WIDTH-16-(6-wday)*IND_WIDTH-3, IND_HEIGHT));
  graphics_draw_line(ctx, GPoint(WIDTH-16-(7-wday)*IND_WIDTH+1, IND_HEIGHT+1), GPoint(WIDTH-16-(6-wday)*IND_WIDTH-3, IND_HEIGHT+1));
  
  // Mark AMPM
  graphics_draw_line(ctx, GPoint(8 + IND_WIDTH*is_ampm+1, IND_HEIGHT), GPoint(8 + IND_WIDTH*(is_ampm+1)-3, IND_HEIGHT));
  graphics_draw_line(ctx, GPoint(8 + IND_WIDTH*is_ampm+1, IND_HEIGHT+1), GPoint(8 + IND_WIDTH*(is_ampm+1)-3, IND_HEIGHT+1));
  
}

// Utility Function to format string adding an offset
void string_format_time_with_offset(char * 	ptr, size_t 	maxsize, const char * 	format, const PblTm * timeptr, int tz_offset) {
  
  PblTm now = *timeptr;
  
  
  now.tm_min += (tz_offset - utc_offset) % 60;
  
  if (now.tm_min > 60) {
    now.tm_hour++;
    now.tm_min -= 60;
  } else if (now.tm_min < 0) {
    now.tm_hour--;
    now.tm_min += 60;
  }

  now.tm_hour += (tz_offset - utc_offset) / 60;
  if (now.tm_hour > 24)
    now.tm_hour -= 24;
  if (now.tm_hour < 0)
    now.tm_hour += 24;
  
  string_format_time(ptr, maxsize, format, &now);
  if (now.tm_hour>=12 && !clock_is_24h_style()) {
    strcat(ptr,dot);
  }
}




