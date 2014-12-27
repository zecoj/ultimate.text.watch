#include <pebble.h>
#include <ctype.h>

#include "num2words.h"

#define DEBUG 0

#define NUM_LINES 4
#define LINE_LENGTH 7
#define STATUS_LINE_LENGTH 10
#define STATUS_LINE_TIMEOUT 7000
#define BUFFER_SIZE (LINE_LENGTH + 2)
#define ROW_HEIGHT 37
#define TOP_MARGIN 10

#define TEXT_ALIGN_CENTER 0
#define TEXT_ALIGN_LEFT 1
#define TEXT_ALIGN_RIGHT 2

// The time it takes for a layer to slide in or out.
#define ANIMATION_DURATION 800
// Delay between the layers animations, from top to bottom
#define ANIMATION_STAGGER_TIME 150
// Delay from the start of the current layer going out until the next layer slides in
#define ANIMATION_OUT_IN_DELAY 100

#define LINE_APPEND_MARGIN 0
// We can add a new word to a line if there are at least this many characters free after
#define LINE_APPEND_LIMIT (LINE_LENGTH - LINE_APPEND_MARGIN)
  
#define MyTupletCString(_key, _cstring) ((const Tuplet) { .type = TUPLE_CSTRING, .key = _key, .cstring = { .data = _cstring, .length = strlen(_cstring) + 1 }})

static uint8_t text_align = TEXT_ALIGN_CENTER;
static bool bluetooth = true;
static bool bluetooth_old = true;
static uint8_t weather = 15;
static bool weather_force_update = false;
static bool invert = false;
static Language lang = EN_GB;

static Window *window;

typedef struct {
  TextLayer *currentLayer;
  TextLayer *nextLayer;
  char lineStr1[BUFFER_SIZE];
  char lineStr2[BUFFER_SIZE];
  PropertyAnimation *animation1;
  PropertyAnimation *animation2;
} Line;

/////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////
bool bt_connect_toggle;

typedef struct {
  TextLayer     *layer[2];
} TextLine;

typedef struct {
  char  topbar[25];
  char  bottombarL[10];
  char  bottombarR[4];
} StatusBars;

static StatusBars status_bars;

static TextLine topbar;
static TextLine bottombarL;
static TextLine bottombarR;

static AppTimer *shake_timeout = NULL;

static AppSync sync;
static uint8_t sync_buffer[128];

static char weather_str[] = "012345678901234";
static char temp_c_str[] = "01234";

#define  CONF_ALIGNMENT             0
#define  CONF_BLUETOOTH             1
#define  CONF_WEATHER               2
#define  WEATHER_ICON_KEY           3
#define  WEATHER_TEMPERATURE_C_KEY  4

/////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////

static Line lines[NUM_LINES];
static InverterLayer *inverter_layer;

static struct tm *t;

static uint8_t currentNLines;

char *translate_error(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

static GTextAlignment lookup_text_alignment(int align_key)
{
GTextAlignment alignment;
  switch (align_key)
  {
    case TEXT_ALIGN_LEFT:
      alignment = GTextAlignmentLeft;
      break;
    case TEXT_ALIGN_RIGHT:
      alignment = GTextAlignmentRight;
      break;
    default:
      alignment = GTextAlignmentCenter;
      break;
  }
  return alignment;
}

/////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////
void info_lines(char *load_status) {
  BatteryChargeState charge_state = battery_state_service_peek();
  
  strftime(status_bars.topbar, sizeof(status_bars.topbar), "%H:%M", t);
  if (strcmp(weather_str, "no data") && weather) {
    if (!strcmp(load_status, "")) {
    snprintf(status_bars.topbar, sizeof(status_bars.topbar), "%s • %s %s", status_bars.topbar, weather_str, temp_c_str);
    }
    else {
    snprintf(status_bars.topbar, sizeof(status_bars.topbar), "%s • %s", status_bars.topbar, load_status);
    }
  }

  strcpy(status_bars.bottombarL, "");
  strftime(status_bars.bottombarL, sizeof(status_bars.bottombarL), "%a %e", t);
  snprintf(status_bars.bottombarR, sizeof(status_bars.bottombarR), "%d%%", charge_state.charge_percent);
  
  status_bars.bottombarL[0]=tolower((unsigned char)status_bars.bottombarL[0]);

  text_layer_set_text(topbar.layer[0], status_bars.topbar);
  text_layer_set_text(bottombarL.layer[0], status_bars.bottombarL);
  text_layer_set_text(bottombarR.layer[0], status_bars.bottombarR);
}

void hide_bars () {
  layer_set_hidden(text_layer_get_layer(topbar.layer[0]), true);
  layer_set_hidden(text_layer_get_layer(bottombarL.layer[0]), true);
  layer_set_hidden(text_layer_get_layer(bottombarR.layer[0]), true);
  weather_force_update = false;
  shake_timeout = NULL;
}
void show_bars ()  {
  info_lines("");
  layer_set_hidden(text_layer_get_layer(topbar.layer[0]), false);
  layer_set_hidden(text_layer_get_layer(bottombarL.layer[0]), false);
  layer_set_hidden(text_layer_get_layer(bottombarR.layer[0]), false);
}
void wrist_flick_handler(AccelAxisType axis, int32_t direction) {
  if (axis == 1 && !shake_timeout) {
    //if (!strcmp(weather_str, "no data")) {
    //  app_message_outbox_send();
    //}
    show_bars();
    shake_timeout = app_timer_register (STATUS_LINE_TIMEOUT, hide_bars, NULL);
  }
  else if (axis == 1 && shake_timeout && !weather_force_update) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "SHAKE it like a Polaroid picture");
    app_message_outbox_send();
    info_lines("loading...");
    weather_force_update = true;
    app_timer_reschedule(shake_timeout, STATUS_LINE_TIMEOUT);
  }
}

void bluetooth_connection_handler(bool connected) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "bluetooth_connection_handler called");
  if(bluetooth){
    if (!bt_connect_toggle && connected) {
      bt_connect_toggle = true;
      vibes_short_pulse();
    }
    if (bt_connect_toggle && !connected) {
      bt_connect_toggle = false;
      vibes_short_pulse();
    }
    layer_set_hidden(inverter_layer_get_layer(inverter_layer), bt_connect_toggle);
  }
  else {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "I'm still here, just ignoring bluetooth events");
    layer_set_hidden(inverter_layer_get_layer(inverter_layer), true);
  }
}

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %i - %s", app_message_error, translate_error(app_message_error));
    strcpy(weather_str, "no data");
    strcpy(temp_c_str, "01234");
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  GTextAlignment alignment;

    // process the first and subsequent update
    switch (key) {
        case CONF_ALIGNMENT:
//  APP_LOG(APP_LOG_LEVEL_DEBUG, "Tuple Changed:  %u, %u", old_tuple->value->uint8, new_tuple->value->uint8);
            text_align = new_tuple->value->uint8;
            persist_write_int(CONF_ALIGNMENT, text_align);
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "Set text alignment: %u", text_align);

            alignment = lookup_text_alignment(text_align);
            for (int i = 0; i < NUM_LINES; i++)
            {
              text_layer_set_text_alignment(lines[i].currentLayer, alignment);
              text_layer_set_text_alignment(lines[i].nextLayer, alignment);
              layer_mark_dirty(text_layer_get_layer(lines[i].currentLayer));
              layer_mark_dirty(text_layer_get_layer(lines[i].nextLayer));
            }
            break;
      
        case CONF_BLUETOOTH:
            bluetooth_old = bluetooth;
            bluetooth = new_tuple->value->uint8 == 1;
            persist_write_bool(CONF_BLUETOOTH, bluetooth);
            if (bluetooth && !bluetooth_old) {
              //APP_LOG(APP_LOG_LEVEL_DEBUG, "re-subscribing");
              bluetooth_connection_service_subscribe(bluetooth_connection_handler);
              bt_connect_toggle = bluetooth_connection_service_peek();
              layer_set_hidden(inverter_layer_get_layer(inverter_layer), bt_connect_toggle);
            }
            if (!bluetooth && bluetooth_old) {
              //APP_LOG(APP_LOG_LEVEL_DEBUG, "unsubscribing");
              bluetooth_connection_service_unsubscribe();
            }
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "Set bluetooth: %u", bluetooth ? 1 : 0);
            break;
      
        case CONF_WEATHER:
            weather = new_tuple->value->uint8;
            persist_write_int(CONF_WEATHER, weather);
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "Set weather: %u", weather);
            break;

        case WEATHER_ICON_KEY:
            strcpy(weather_str, new_tuple->value->cstring);
            persist_write_string(WEATHER_ICON_KEY, weather_str);
            // make all lowercase
            weather_str[0] = tolower((unsigned char)weather_str[0]);
            break;
            
        case WEATHER_TEMPERATURE_C_KEY:
            strcpy(temp_c_str, new_tuple->value->cstring);
            //strcat(temp_c_str, "\u00B0C");
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "Set temp_c_str: %s", temp_c_str);
            persist_write_string(WEATHER_TEMPERATURE_C_KEY, temp_c_str);
            if(weather_force_update) {
              info_lines("");
              //weather_force_update = false;
            }
            break;
    }
}

/////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////



// Animation handler
static void animationStoppedHandler(struct Animation *animation, bool finished, void *context)
{
  TextLayer *current = (TextLayer *)context;
  GRect rect = layer_get_frame((Layer *)current);
  rect.origin.x = 144;
  layer_set_frame((Layer *)current, rect);
}

// Animate line
static void makeAnimationsForLayer(Line *line, int delay)
{
  TextLayer *current = line->currentLayer;
  TextLayer *next = line->nextLayer;

  // Destroy old animations
  if (line->animation1 != NULL)
  {
     property_animation_destroy(line->animation1);
  }
  if (line->animation2 != NULL)
  {
     property_animation_destroy(line->animation2);
  }

  // Configure animation for current layer to move out
  GRect rect = layer_get_frame((Layer *)current);
  rect.origin.x =  -144;
  line->animation1 = property_animation_create_layer_frame((Layer *)current, NULL, &rect);
  animation_set_duration(&line->animation1->animation, ANIMATION_DURATION);
  animation_set_delay(&line->animation1->animation, delay);
  animation_set_curve(&line->animation1->animation, AnimationCurveEaseIn); // Accelerate

  // Configure animation for current layer to move in
  GRect rect2 = layer_get_frame((Layer *)next);
  rect2.origin.x = 0;
  line->animation2 = property_animation_create_layer_frame((Layer *)next, NULL, &rect2);
  animation_set_duration(&line->animation2->animation, ANIMATION_DURATION);
  animation_set_delay(&line->animation2->animation, delay + ANIMATION_OUT_IN_DELAY);
  animation_set_curve(&line->animation2->animation, AnimationCurveEaseOut); // Deaccelerate

  // Set a handler to rearrange layers after animation is finished
  animation_set_handlers(&line->animation2->animation, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler)animationStoppedHandler
  }, current);

  // Start the animations
  animation_schedule(&line->animation1->animation);
  animation_schedule(&line->animation2->animation);  
}

static void updateLayerText(TextLayer* layer, char* text)
{
  const char* layerText = text_layer_get_text(layer);
  strcpy((char*)layerText, text);
  // To mark layer dirty
  text_layer_set_text(layer, layerText);
    //layer_mark_dirty(&layer->layer);
}

// Update line
static void updateLineTo(Line *line, char *value, int delay)
{
  updateLayerText(line->nextLayer, value);
  makeAnimationsForLayer(line, delay);

  // Swap current/next layers
  TextLayer *tmp = line->nextLayer;
  line->nextLayer = line->currentLayer;
  line->currentLayer = tmp;
}

// Check to see if the current line needs to be updated
static bool needToUpdateLine(Line *line, char *nextValue)
{
  const char *currentStr = text_layer_get_text(line->currentLayer);

  if (strcmp(currentStr, nextValue) != 0) {
    return true;
  }
  return false;
}

// Configure bold line of text
static void configureBoldLayer(TextLayer *textlayer)
{
  text_layer_set_font(textlayer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_color(textlayer, GColorWhite);
  text_layer_set_background_color(textlayer, GColorClear);
  text_layer_set_text_alignment(textlayer, lookup_text_alignment(text_align));
}

// Configure light line of text
static void configureLightLayer(TextLayer *textlayer)
{
  text_layer_set_font(textlayer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  text_layer_set_text_color(textlayer, GColorWhite);
  text_layer_set_background_color(textlayer, GColorClear);
  text_layer_set_text_alignment(textlayer, lookup_text_alignment(text_align));
}

// Configure the layers for the given text
static int configureLayersForText(char text[NUM_LINES][BUFFER_SIZE], char format[])
{
  int numLines = 0;

  // Set bold layer.
  int i;
  for (i = 0; i < NUM_LINES; i++) {
    if (strlen(text[i]) > 0) {
      if (format[i] == 'b')
      {
        configureBoldLayer(lines[i].nextLayer);
      }
      else
      {
        configureLightLayer(lines[i].nextLayer);
      }
    }
    else
    {
      break;
    }
  }
  numLines = i;

  // Calculate y position of top Line
  int ypos = (168 - numLines * ROW_HEIGHT) / 2 - TOP_MARGIN;

  // Set y positions for the lines
  for (int i = 0; i < numLines; i++)
  {
    layer_set_frame((Layer *)lines[i].nextLayer, GRect(144, ypos, 144, 50));
    ypos += ROW_HEIGHT;
  }

  return numLines;
}

static void time_to_lines(int hours, int minutes, int seconds, char lines[NUM_LINES][BUFFER_SIZE], char format[])
{
  int length = NUM_LINES * BUFFER_SIZE + 1;
  char timeStr[length];
  time_to_words(lang, hours, minutes, seconds, timeStr, length);
  
  // Empty all lines
  for (int i = 0; i < NUM_LINES; i++)
  {
    lines[i][0] = '\0';
  }

  char *start = timeStr;
  char *end = strstr(start, " ");
  int l = 0;
  while (end != NULL && l < NUM_LINES) {
    // Check word for bold prefix
    if (*start == '*' && end - start > 1)
    {
      // Mark line bold and move start to the first character of the word
      format[l] = 'b';
      start++;
    }
    else
    {
      // Mark line normal
      format[l] = ' ';
    }

    // Can we add another word to the line?
    if (format[l] == ' ' && *(end + 1) != '*'    // are both lines formatted normal?
      && end - start < LINE_APPEND_LIMIT - 1)  // is the first word is short enough?
    {
      // See if next word fits
      char *try = strstr(end + 1, " ");
      if (try != NULL && try - start <= LINE_APPEND_LIMIT)
      {
        end = try;
      }
    }

    // copy to line
    *end = '\0';
    strcpy(lines[l++], start);

    // Look for next word
    start = end + 1;
    end = strstr(start, " ");
  }
  
}

// Update screen based on new time
static void display_time(struct tm *t)
{
  // The current time text will be stored in the following strings
  char textLine[NUM_LINES][BUFFER_SIZE];
  char format[NUM_LINES];

  time_to_lines(t->tm_hour, t->tm_min, t->tm_sec, textLine, format);
  
  int nextNLines = configureLayersForText(textLine, format);

  int delay = 0;
  for (int i = 0; i < NUM_LINES; i++) {
    if (nextNLines != currentNLines || needToUpdateLine(&lines[i], textLine[i])) {
      updateLineTo(&lines[i], textLine[i], delay);
      delay += ANIMATION_STAGGER_TIME;
    }
  }
  
  currentNLines = nextNLines;
}

static void initLineForStart(Line* line)
{
  // Switch current and next layer
  TextLayer* tmp  = line->currentLayer;
  line->currentLayer = line->nextLayer;
  line->nextLayer = tmp;

  // Move current layer to screen;
  GRect rect = layer_get_frame((Layer *)line->currentLayer);
  rect.origin.x = 0;
  layer_set_frame((Layer *)line->currentLayer, rect);
}

// Update screen without animation first time we start the watchface
static void display_initial_time(struct tm *t)
{
  // The current time text will be stored in the following strings
  char textLine[NUM_LINES][BUFFER_SIZE];
  char format[NUM_LINES];

  time_to_lines(t->tm_hour, t->tm_min, t->tm_sec, textLine, format);

  // This configures the nextLayer for each line
  currentNLines = configureLayersForText(textLine, format);

  // Set the text and configure layers to the start position
  for (int i = 0; i < currentNLines; i++)
  {
    updateLayerText(lines[i].nextLayer, textLine[i]);
    // This call switches current- and nextLayer
    initLineForStart(&lines[i]);
  }  
}

// Time handler called every minute by the system
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{
  //t = tick_time;
  display_time(tick_time);
  //info_lines();
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "MINUTE_TICK I'm seeing weather: %u", weather);
  if(weather && tick_time->tm_min % weather == 0) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "MINUTE_TICK I'm supposed to check the weather now");
    app_message_outbox_send();
  }
}

static void init_line(Line* line)
{
  // Create layers with dummy position to the right of the screen
  line->currentLayer = text_layer_create(GRect(144, 0, 144, 50));
  line->nextLayer = text_layer_create(GRect(144, 0, 144, 50));

  // Configure a style
  configureLightLayer(line->currentLayer);
  configureLightLayer(line->nextLayer);

  // Set the text buffers
  line->lineStr1[0] = '\0';
  line->lineStr2[0] = '\0';
  text_layer_set_text(line->currentLayer, line->lineStr1);
  text_layer_set_text(line->nextLayer, line->lineStr2);

  // Initially there are no animations
  line->animation1 = NULL;
  line->animation2 = NULL;
}

static void destroy_line(Line* line)
{
  // Free layers
  text_layer_destroy(line->currentLayer);
  text_layer_destroy(line->nextLayer);
}

static void window_load(Window *window)
{
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  // Init and load lines
  for (int i = 0; i < NUM_LINES; i++)
  {
    init_line(&lines[i]);
    layer_add_child(window_layer, (Layer *)lines[i].currentLayer);
    layer_add_child(window_layer, (Layer *)lines[i].nextLayer);
  }
  
  /////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////
  topbar.layer[0] = text_layer_create(GRect(0, -4, 144, 16));
  text_layer_set_text_color(topbar.layer[0], GColorWhite);
  text_layer_set_background_color(topbar.layer[0], GColorBlack);
  text_layer_set_font(topbar.layer[0], fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(topbar.layer[0], GTextAlignmentCenter);
  
  bottombarL.layer[0] = text_layer_create(GRect(0, 153, 45, 15));
  text_layer_set_text_color(bottombarL.layer[0], GColorWhite);
  text_layer_set_background_color(bottombarL.layer[0], GColorBlack);
  text_layer_set_font(bottombarL.layer[0], fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(bottombarL.layer[0], GTextAlignmentLeft);
  
  bottombarR.layer[0] = text_layer_create(GRect(110, 153, 34, 15));
  text_layer_set_text_color(bottombarR.layer[0], GColorWhite);
  text_layer_set_background_color(bottombarR.layer[0], GColorBlack);
  text_layer_set_font(bottombarR.layer[0], fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(bottombarR.layer[0], GTextAlignmentRight);

  layer_add_child(window_layer, text_layer_get_layer(topbar.layer[0]));
  layer_add_child(window_layer, text_layer_get_layer(bottombarL.layer[0]));
  layer_add_child(window_layer, text_layer_get_layer(bottombarR.layer[0]));

  layer_set_hidden(text_layer_get_layer(topbar.layer[0]), true);
  layer_set_hidden(text_layer_get_layer(bottombarL.layer[0]), true);
  layer_set_hidden(text_layer_get_layer(bottombarR.layer[0]), true);

  // prepare the initial values of your data
    Tuplet initial_values[] = {
        TupletInteger(CONF_ALIGNMENT, (uint8_t) text_align),
        TupletInteger(CONF_BLUETOOTH, (uint8_t) bluetooth ? 1 : 0),
        TupletInteger(CONF_WEATHER,   (uint8_t) weather),
        MyTupletCString(WEATHER_ICON_KEY, weather_str),
        MyTupletCString(WEATHER_TEMPERATURE_C_KEY, temp_c_str)
    };
    // initialize the syncronization
    app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
        sync_tuple_changed_callback, sync_error_callback, NULL);
    //send_cmd();

  /////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////
  inverter_layer = inverter_layer_create(bounds);
  layer_set_hidden(inverter_layer_get_layer(inverter_layer), !invert);
  layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));
  
  // Configure time on init
  time_t raw_time;

  time(&raw_time);
  t = localtime(&raw_time);
  display_initial_time(t);
  info_lines("");
}

static void window_unload(Window *window)
{

  // Free layers
  layer_destroy(text_layer_get_layer(topbar.layer[0]));
  layer_destroy(text_layer_get_layer(bottombarL.layer[0]));
  layer_destroy(text_layer_get_layer(bottombarR.layer[0]));
  inverter_layer_destroy(inverter_layer);

  for (int i = 0; i < NUM_LINES; i++)
  {
    destroy_line(&lines[i]);
  }
}

static void handle_init() {
  
  // Load settings from persistent storage
  if (persist_exists(CONF_ALIGNMENT))
  {
    text_align = persist_read_int(CONF_ALIGNMENT);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Read text CONF_ALIGNMENT from store: %u", text_align);
  }
  if (persist_exists(CONF_BLUETOOTH))
  {
    bluetooth = persist_read_bool(CONF_BLUETOOTH);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Read CONF_BLUETOOTH from store: %u", bluetooth ? 1 : 0);
  }
  if (persist_exists(CONF_WEATHER))
  {
    weather = persist_read_int(CONF_WEATHER);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Read CONF_WEATHER from store: %u", weather);
  }
  if (persist_exists(WEATHER_ICON_KEY))
  {
    persist_read_string(WEATHER_ICON_KEY, weather_str, sizeof(weather_str));
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Read WEATHER_ICON_KEY from store: %s", weather_str);
  }
  if (persist_exists(WEATHER_TEMPERATURE_C_KEY))
  {
    persist_read_string(WEATHER_TEMPERATURE_C_KEY, temp_c_str, sizeof(temp_c_str));
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Read WEATHER_TEMPERATURE_C_KEY from store: %s", temp_c_str);
  }

  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  const bool animated = true;
  window_stack_push(window, animated);
  
  // Subscribe to minute ticks
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  
  /////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////
  // If I monitor bluetooth Subscribe to bluetooth service
  if (bluetooth) {
    bluetooth_connection_service_subscribe(bluetooth_connection_handler);
    bt_connect_toggle = bluetooth_connection_service_peek();
    layer_set_hidden(inverter_layer_get_layer(inverter_layer), bt_connect_toggle);
  }

  // Subscribe to shake events
  accel_tap_service_subscribe(wrist_flick_handler);
  
  // to sync watch fields
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  /////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////
}

static void handle_deinit()
{
  // Free window
  app_sync_deinit(&sync);
  accel_tap_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  window_destroy(window);
}

int main(void)
{
  handle_init();
  app_event_loop();
  handle_deinit();
}