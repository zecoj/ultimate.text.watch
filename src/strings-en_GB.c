#include "strings-en_GB.h"

const char* const HOURS_EN_GB[] = {
  // AM hours
  "twelve",
  "one",
  "two",
  "three",
  "four",
  "five",
  "six",
  "seven",
  "eight",
  "nine",
  "ten",
  "eleven",

  // PM hours
  "twelve",
  "one",
  "two",
  "three",
  "four",
  "five",
  "six",
  "seven",
  "eight",
  "nine",
  "ten",
  "eleven"
};

/**
 * The string "$1" will be replaced with the current hour (e.g., "three"
 * at 3:45).  The string "$2" will be replaced with the *next* hour
 * (e.g., "four" at 3:45).
 *
 * A "*" character before a word makes that word bold.
 */
const char* const RELS_EN_GB[] = {
  "*$1 o'clock",
  "five past *$1",
  "ten past *$1",
  "quarter past *$1",
  "twenty past *$1",
  "twenty five past *$1",
  "half past *$1",
  "twenty five to *$2",
  "twenty to *$2",
  "quarter to *$2",
  "ten to *$2",
  "five to *$2"
};

