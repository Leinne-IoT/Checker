#ifdef DEBUG_MODE
#define debugPrint(fmt, args...) printf(fmt, ##args)
#else
#define debugPrint(fmt, args...)
#endif