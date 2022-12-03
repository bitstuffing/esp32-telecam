
//sizeof returns the number of bytes of String (2), so this define convert and return the number of Objects in array by type
#define NUM_ITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds

RTC_DATA_ATTR int bootCount = 0;

#define TIME_TO_SLEEP  30        // Time ESP32 will go to sleep (in seconds)
#define LIMIT_RETRIES  60        // timeout wifi retries, each 0.5 seconds
#define BOT_MTBS 5000 //mean time between scan messages