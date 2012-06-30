#ifndef _H_LOG
#define _H_LOG

#include <time.h>
#include "config.h"

typedef enum {
    LOG_EMERG,
    LOG_ALERT,
    LOG_CRIT,
    LOG_ERR,
    LOG_WARN,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG,
} loglevel_t;

typedef enum {
    WT_LASTPRINT,
    WT_LASTCALL,
    WT_COUNTER,
    WT_COUNT
} wt_index_t;

#define LOGLEVEL logconfig->level
#define LOG(level, msg, ...) if(level <= LOGLEVEL) { logmsg(level, __FILE__, __LINE__, msg, ##__VA_ARGS__); }
#define ULOG(level, msg, ...) if(level <= LOGLEVEL) { logudata(level, __FILE__, __LINE__, msg, ##__VA_ARGS__); }
#define ASSERT_LOG(cond, level, msg, ...) if(level <= LOGLEVEL && !(cond)) { logmsg(level, __FILE__, __LINE__, msg, ##__VA_ARGS__); }

#define LEMERG(msg, ...) LOG(LOG_EMERG, msg, ##__VA_ARGS__)
#define LALERT(msg, ...) LOG(LOG_ALERT, msg, ##__VA_ARGS__)
#define LNIMPL(msg, ...) LOG(LOG_ALERT, msg, ##__VA_ARGS__)
#define LCRIT(msg, ...) LOG(LOG_CRIT, msg, ##__VA_ARGS__)
#define LERR(msg, ...) LOG(LOG_ERR, msg, ##__VA_ARGS__)
#define LWARN(msg, ...) LOG(LOG_WARN, msg, ##__VA_ARGS__)
#define TWARN(msg, ...) if(LOG_WARN <= LOGLEVEL) { static time_t warnto_##__LINE__[WT_COUNT]={0,0,0}; \
    timedwarn(warnto_##__LINE__, __FILE__, __LINE__, msg, ##__VA_ARGS__); }
#define LNOTICE(msg, ...) LOG(LOG_NOTICE, msg, ##__VA_ARGS__)
#define LINFO(msg, ...) LOG(LOG_INFO, msg, ##__VA_ARGS__)

#define UWARN(msg, ...) ULOG(LOG_WARN, msg, ##__VA_ARGS__)
#define UNOTICE(msg, ...) ULOG(LOG_NOTICE, msg, ##__VA_ARGS__)

#define AEMERG(cond) ASSERT_LOG(cond, LOG_EMERG, "Assertion failed " # cond)
#define AALERT(cond) ASSERT_LOG(cond, LOG_ALERT, "Assertion failed " # cond)
#define ANIMPL(cond) ASSERT_LOG(cond, LOG_ALERT, "Handling of !(" # cond ") is not implemented")
#define ACRIT(cond) ASSERT_LOG(cond, LOG_CRIT, "Assertion failed " # cond)
#define AERR(cond) ASSERT_LOG(cond, LOG_ERR, "Assertion failed " # cond)
#define AWARN(cond) ASSERT_LOG(cond, LOG_WARN, "Assertion failed " # cond)

#define AEMERG2(cond, msg, ...) ASSERT_LOG(cond, LOG_EMERG, msg, ##__VA_ARGS__)
#define AALERT2(cond, msg, ...) ASSERT_LOG(cond, LOG_ALERT, msg, ##__VA_ARGS__)
#define ANIMPL2(cond, msg, ...) ASSERT_LOG(cond, LOG_ALERT, msg, ##__VA_ARGS__)
#define ACRIT2(cond, msg, ...) ASSERT_LOG(cond, LOG_CRIT, msg, ##__VA_ARGS__)
#define AERR2(cond, msg, ...) ASSERT_LOG(cond, LOG_ERR, msg, ##__VA_ARGS__)
#define AWARN2(cond, msg, ...) ASSERT_LOG(cond, LOG_WARN, msg, ##__VA_ARGS__)

#define SNIMPL(res) if((res) && LOG_ALERT <= LOGLEVEL) { logstd(LOG_ALERT, __FILE__, __LINE__, #res); }
#define SNIMPL2(res, msg, ...) if(res && LOG_ALERT <= LOGLEVEL) { logstd(LOG_ALERT, __FILE__, __LINE__, msg, ##__VA_ARGS__); }
#define SWARN() if(LOG_WARN <= LOGLEVEL) { logstd(LOG_WARN, __FILE__, __LINE__, ""); }
#define SWARN2(msg, ...) if(LOG_WARN <= LOGLEVEL) { logstd(LOG_WARN, __FILE__, __LINE__, msg, ##__VA_ARGS__); }

#ifndef NDEBUG
#define LDEBUG(msg, ...) LOG(LOG_DEBUG, msg, ##__VA_ARGS__)
#define ANOTICE(cond) ASSERT_LOG(cond, LOG_NOTICE, "Assertion failed " # cond)
#define AINFO(cond) ASSERT_LOG(cond, LOG_INFO, "Assertion failed " # cond)
#define ADEBUG(cond) ASSERT_LOG(cond, LOG_DEBUG, "Assertion failed " # cond)
#define ANOTICE2(cond, msg, ...) ASSERT_LOG(cond, LOG_NOTICE, msg, ##__VA_ARGS__)
#define AINFO2(cond, msg, ...) ASSERT_LOG(cond, LOG_INFO, msg, ##__VA_ARGS__)
#define ADEBUG2(cond, msg, ...) ASSERT_LOG(cond, LOG_DEBUG, msg, ##__VA_ARGS__)
#else
#define LDEBUG(msg, ...)
#define ANOTICE(cond)
#define AINFO(cond)
#define ADEBUG(cond)
#define ANOTICE2(cond, msg, ...)
#define AINFO2(cond, msg, ...)
#define ADEBUG2(cond, msg, ...)
#endif

extern config_logging_t *logconfig;

void logmsg(int level, char *file, int line, char *msg, ...);
void logstd(int level, char *file, int line, char *msg, ...);
void logudata(int level, char *file, int line, char *msg, ...);
void timedwarn(time_t *tt, char *file, int line, char *msg, ...);
void openlogs();
bool reopenlogs();
void *obstack_chunk_alloc(int len);
void obstack_chunk_free(void *ptr);

#endif
