#include "pebble.h"
#include "num2words.h"
#include "strings-en_GB.h"
#include "string.h"

static const char* const ONES[] = {
  "o'clock",
  "one",
  "two",
  "three",
  "four",
  "five",
  "six",
  "seven",
  "eight",
  "nine"
};

static const char* const TEENS[] ={
  "",
  "eleven",
  "twelve",
  "thirteen",
  "four teen",
  "fifteen",
  "sixteen",
  "seven teen",
  "eight teen",
  "nine teen"
};

static const char* const TENS[] = {
  "",
  "ten",
  "twenty",
  "thirty",
  "forty",
  "fifty",
  "sixty",
  "seventy",
  "eighty",
  "ninety"
};

static const char* STR_MIDDAY = "mid *day";
static const char* STR_MIDNIGHT = "mid *night";

size_t min(const size_t a, const size_t b) {
  return a < b ? a : b;
}

static size_t append_string(char* buffer, const size_t length, const char* str) {
  strncat(buffer, str, length);

  size_t written = strlen(str);
  return (length > written) ? written : length;
}

static size_t interpolate_and_append(char* buffer, const size_t length,
    const char* parent_str, const char* first_placeholder_str, const char* second_placeholder_str) {
  const char* placeholder_str;
  char* insert_ptr = strstr(parent_str, "$1");

  if (insert_ptr) {
    placeholder_str = first_placeholder_str;
  }
  else {
    insert_ptr = strstr(parent_str, "$2");
    placeholder_str = second_placeholder_str;
  }

  size_t parent_len = strlen(parent_str);
  size_t insert_offset = insert_ptr ? (size_t) insert_ptr - (size_t) parent_str : parent_len;

  size_t remaining = length;

  remaining -= append_string(buffer, min(insert_offset, remaining), parent_str);
  remaining -= append_string(buffer, remaining, placeholder_str);
  if (insert_ptr) {
    remaining -= append_string(buffer, remaining, insert_ptr + 2);
  }

  return remaining;
}

const char* get_hour(Language lang, int index) {
  switch (lang) {
    case EN_GB:
      return HOURS_EN_GB[index];
      break;
    default:
      return HOURS_EN_GB[index];
  
  }
}

const char* get_rel(Language lang, int index) {
  switch (lang) {
    case EN_GB:
      return RELS_EN_GB[index];
      break;
    default:
      return RELS_EN_GB[index];
  }
}

static size_t append_number(char* words, int num, int minutes) {
  int tens_val = num / 10 % 10;
  int ones_val = num % 10;

  size_t len = 0;

  if (tens_val > 0 || minutes ) {
	/*if ( tens_val == 0 && minutes ) {
		strcat(words,STR_OH);
		strcat(words," ");
	} else  */
    if (tens_val == 1 && num != 10) {
      strcat(words, TEENS[ones_val]);
      return strlen(TEENS[ones_val]);
    }
    strcat(words, TENS[tens_val]);
    len += strlen(TENS[tens_val]);
    if (ones_val > 0 && tens_val !=0) {
      strcat(words, " ");
      len += 1;
    }
  }

  if (ones_val > 0 || num == 0) {
    strcat(words, ONES[ones_val]);
    len += strlen(ONES[ones_val]);
  }
  return len;
}


void time_to_words_0(Language lang, int hours, int minutes, int seconds, char* words, size_t buffer_size) {
  if ((hours == 11 && minutes >56)||(hours == 12 && minutes <4)) {
    strcpy(words, "mid *day ");
    return;
  }
  if ((hours == 23 && minutes >56)||(hours == 00 && minutes <4)) {
    strcpy(words, "mid *night ");
    return;
  }

  size_t remaining = buffer_size;
  memset(words, 0, buffer_size);

  // We want to operate with a resolution of 30 seconds.  So multiply
  // minutes and seconds by 2.  Then divide by (2 * 5) to carve the hour
  // into five minute intervals.
  int half_mins  = (2 * minutes) + (seconds / 30);
  int rel_index  = ((half_mins + 5) / (2 * 5)) % 12;
  int hour_index;

  if (rel_index == 0 && minutes > 30) {
    hour_index = (hours + 1) % 24;
  }
  else {
    hour_index = hours % 24;
  }

  const char* hour = get_hour(lang, hour_index);
  const char* next_hour = get_hour(lang, (hour_index + 1) % 24);
  const char* rel  = get_rel(lang, rel_index);

  remaining -= interpolate_and_append(words, remaining, rel, hour, next_hour);

  // Leave one space at the end
  remaining -= append_string(words, remaining, " ");
}


void time_to_words_1(Language lang, int hours, int minutes, int seconds, char* words, size_t length) {
  static char past_to[5];
  size_t remaining = length;
  memset(words, 0, length);
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "INITIAL minutes: %d", minutes);

  //O'clock
  if (minutes ==0) {
    if (hours == 0) {
      remaining -= append_string(words, remaining, STR_MIDNIGHT);
      remaining -= append_string(words, remaining, " ");
      return;
    } else if (hours == 12) {
      remaining -= append_string(words, remaining, STR_MIDDAY);
      remaining -= append_string(words, remaining, " ");
      return;
    }
    else {
      remaining -= append_string(words, remaining, "*");
      remaining -= append_number(words, hours % 12,0);
      remaining -= append_string(words, remaining, " ");
      remaining -= append_number(words, 0, 1);
      remaining -= append_string(words, remaining, " ");
      return;
    }
  }
  //Past
  else if (minutes > 0 && minutes <= 30) {
    strcpy (past_to, "past ");
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "PAST called: %s", past_to);
  }
  //To
  else if (minutes > 30 && minutes <=59) {
    strcpy (past_to, "to ");
    minutes = 60 - minutes;
    hours += 1;
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "TO called: %s", past_to);
  }
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "FIXEDUP minutes: %d", minutes);

  if (minutes == 15) {
    remaining -= append_string(words, remaining, "quarter");
  }
  else if (minutes == 30 ) {
    remaining -= append_string(words, remaining, "half");
  }
  else {
    remaining -= append_number(words, minutes, 1);
  }
  
  remaining -= append_string(words, remaining, " ");
  remaining -= append_string(words, remaining, past_to);
  
  if ((hours == 12 || hours ==0) && minutes != 0) {
    remaining -= append_string(words, remaining, "*");
    remaining -= append_number(words, 12, 0);
  } else {
    //strcpy(words, "*");
    remaining -= append_string(words, remaining, "*");
    remaining -= append_number(words, hours % 12,0);
  }

  remaining -= append_string(words, remaining, " ");
}


void time_to_words_2(Language lang, int hours, int minutes, int seconds, char* words, size_t length) {
  size_t remaining = length;
  memset(words, 0, length);

  if (hours == 0 && minutes == 0) {
    remaining -= append_string(words, remaining, STR_MIDNIGHT);
    remaining -= append_string(words, remaining, " ");
    return;
  } else if (hours == 12 && minutes == 0) {
    remaining -= append_string(words, remaining, STR_MIDDAY);
    remaining -= append_string(words, remaining, " ");
    return;
  } else if ((hours == 12 || hours ==0) && minutes != 0) {
    remaining -= append_string(words, remaining, "*");
    remaining -= append_number(words, 12, 0);
  } else {
    //strcpy(words, "*");
    remaining -= append_string(words, remaining, "*");
    remaining -= append_number(words, hours % 12,0);
  }

  remaining -= append_string(words, remaining, " ");
  if(minutes > 0 && minutes <10) {
    remaining -= append_string(words, remaining, "oh ");
  }
  remaining -= append_number(words, minutes, 1);
  remaining -= append_string(words, remaining, " ");
}