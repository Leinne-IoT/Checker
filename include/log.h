#ifdef DEBUG_MODE
#define debug(fmt, args...) printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif