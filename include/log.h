#ifdef DEBUG_MODE
#define debugPrint(fmt, args...) printf(fmt "\n", ##args)
#else
#define debugPrint(fmt, args...)
#endif