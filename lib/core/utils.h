
//sizeof returns the number of bytes of String (2), so this define convert and return the number of Objects in array by type
#define NUM_ITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))