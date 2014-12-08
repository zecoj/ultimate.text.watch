#include <pebble.h>
#include <ctype.h>

#include "num2words.h"

#define DEBUG 0

#define NUM_LINES 4
#define LINE_LENGTH 7
#define STATUS_LINE_LENGTH 10
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

static int text_align = TEXT_ALIGN_RIGHT;
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
//bool bt_connect_toggle;

typedef struct {
  TextLayer 		*layer[2];
} TextLine;

typedef struct {
  char 	topbar[10];
  char 	bottombarL[10];
  char  bottombarR[4];
} StatusBars;

static StatusBars status_bars;

static TextLine topbar;
static TextLine bottombarL;
static TextLine bottombarR;

static AppTimer *shake_timeout = NULL;
/////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////

static Line lines[NUM_LINES];
static InverterLayer *inverter_layer;

static struct tm *t;

static int currentNLines;


/////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////
void info_lines() {
  BatteryChargeState charge_state = battery_state_service_peek();
  
  strftime(status_bars.topbar, sizeof(status_bars.topbar), "%H:%M", t);
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
  shake_timeout = NULL;
}
void show_bars ()  {
  info_lines();
  layer_set_hidden(text_layer_get_layer(topbar.layer[0]), false);
  layer_set_hidden(text_layer_get_layer(bottombarL.layer[0]), false);
  layer_set_hidden(text_layer_get_layer(bottombarR.layer[0]), false);
}
void wrist_flick_handler(AccelAxisType axis, int32_t direction) {
  if (axis == 1) {
    if (!shake_timeout) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Shaken not stirred!!!");
      show_bars();
      shake_timeout = app_timer_register (5000, hide_bars, NULL);
    }
    else {APP_LOG(APP_LOG_LEVEL_DEBUG, "STIRRED!!!");}
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
	t = tick_time;
	display_time(tick_time);
  info_lines();
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


  /////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////


	inverter_layer = inverter_layer_create(bounds);
	layer_set_hidden(inverter_layer_get_layer(inverter_layer), !invert);
	layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));
  
	// Configure time on init
	time_t raw_time;

	time(&raw_time);
	t = localtime(&raw_time);
	display_initial_time(t);
  info_lines();

}

static void window_unload(Window *window)
{
//	app_sync_deinit(&sync);

	// Free layers
	inverter_layer_destroy(inverter_layer);
	for (int i = 0; i < NUM_LINES; i++)
	{
		destroy_line(&lines[i]);
	}
}






static void handle_init() {

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
  // Subscribe to shake events
  accel_tap_service_subscribe(wrist_flick_handler);
  /////////////////////////////////////////////////ZECOJ/////////////////////////////////////////////////

}

static void handle_deinit()
{
	// Free window
	window_destroy(window);
}

int main(void)
{
	handle_init();
	app_event_loop();
	handle_deinit();
}