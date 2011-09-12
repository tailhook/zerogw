#include "log.h"
#include "config.h"
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define min(a, b) (((a) < (b))?(a):(b))
#define STDIN   0
#define STDOUT  1
#define STDERR  2
#define ERRLEVEL 2

static char *levels[] = {
    "EMRG",
    "ALRT",
    "CRIT",
    "ERRR",
    "WARN",
    "NOTE",
    "INFO",
    "DEBG"
};
static char source_path[] = LOG_STRIP_PATH;
static int path_strip_chars = sizeof(source_path)-1;
static int logfile = 1;
config_logging_t *logconfig;

void logmsg(int level, char *file, int line, char *msg, ...) {
    char buf[4096];
    time_t tm;
    struct tm ts;
    va_list args;
    time(&tm);
    localtime_r(&tm, &ts);
    int idx = snprintf(buf, 4096, "%04d-%02d-%02d %02d:%02d:%02d [%4s] %s:%d: ",
        ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec, levels[level], file+path_strip_chars, line);
    va_start(args, msg);
    idx += vsnprintf(buf + idx, 4096 - idx, msg, args);
    idx = min(idx, 4094);
    buf[idx++] = '\n';
    write(logfile, buf, idx);
    if(level <= ERRLEVEL) {
        abort();
    }
}
void logstd(int level, char *file, int line, char *msg, ...) {
    char buf[4096];
    time_t tm;
    struct tm ts;
    va_list args;
    int local_errno = errno;
    time(&tm);
    localtime_r(&tm, &ts);
    int idx = snprintf(buf, 4096, "%04d-%02d-%02d %02d:%02d:%02d [%4s] %s:%d: (e%d) ",
        ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday,
        ts.tm_hour, ts.tm_min, ts.tm_sec,
        levels[level], file+path_strip_chars, line, local_errno);
    va_start(args, msg);
    idx += vsnprintf(buf + idx, 4096 - idx, msg, args);
    if(idx < 4093) {
        buf[idx++] = ':';
        buf[idx++] = ' ';
        strerror_r(local_errno, buf + idx, 4096-idx);
        idx += strlen(buf + idx);
    }
    idx = min(idx, 4094);
    buf[idx++] = '\n';
    write(logfile, buf, idx);
    if(level <= ERRLEVEL) {
        abort();
    }
}

void timedwarn(time_t *tt, char *file, int line, char *msg, ...) {
    time_t tm;
    time(&tm);
    tt[WT_COUNTER] ++;
    if(tt[WT_LASTPRINT] < tm - logconfig->warning_timeout) {
        char buf[4096];
        struct tm ts;
        va_list args;
        localtime_r(&tm, &ts);
        int idx = snprintf(buf, 4096, "%04d-%02d-%02d %02d:%02d:%02d [%4s] %s:%d: ",
            ts.tm_year+1900, ts.tm_mon+1, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec, levels[LOG_WARN], file+path_strip_chars, line);
        va_start(args, msg);
        idx += vsnprintf(buf + idx, 4096 - idx, msg, args);
        idx = min(idx, 4094);
        if(tt[WT_LASTCALL] > tm - logconfig->warning_timeout) {
            idx += snprintf(buf + idx, 4096 - idx, " (repeated %ld times)", tt[WT_COUNTER]);
        }
        buf[idx++] = '\n';
        write(logfile, buf, idx);
        if(LOG_WARN <= ERRLEVEL) {
            abort();
        }
        tt[WT_LASTPRINT]  = tm;
        tt[WT_COUNTER] = 0;
    }
    tt[WT_LASTCALL] = tm;
}

void setcloexec(int fd) {
    int flags = fcntl(fd, F_GETFD);
    SNIMPL(flags < 0);
    fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

void openlogs() {
    if(logconfig->error) {
        int oldlogfile = logfile;
        logfile = open(logconfig->error, O_APPEND|O_WRONLY|O_CREAT, 0666);
        if(logfile <= 0) {
            logfile = oldlogfile;
            LWARN("Can't open logfile. Continuing writing to stdout");
        } else {
            LINFO("Log sucessfully opened");
            setcloexec(logfile);
            dup2(logfile, STDOUT);
            dup2(logfile, STDERR);
        }
    }
}

void reopenlogs() {
    if(logconfig->error) {
        int oldlogfile = logfile;
        logfile = open(logconfig->error, O_APPEND|O_WRONLY|O_CREAT, 0666);
        if(logfile <= 0) {
            logfile = oldlogfile;
            SWARN2("Can't open logfile. Continuing writing old file");
        } else {
            LINFO("New log file successfully opened");
            setcloexec(logfile);
            dup2(logfile, STDOUT);
            dup2(logfile, STDERR);
            close(oldlogfile);
        }
    } else {
        LWARN("No logfile specified");
    }
}

void *safe_realloc(void *ptr, int size, char *file, int line) {
    void *res;
    while(!(res = realloc(ptr, size))) {
        if(LOG_WARN <= LOGLEVEL) {
            static time_t warnto[WT_COUNT]={0,0,0};
            timedwarn(warnto, file, line, "Can't allocate %d bytes. Not enought memory", size);
        }
        usleep(10000);
    }
    return res;
}

void *obstack_chunk_alloc(int len) {
    void *res = malloc(len);
    ANIMPL2(res, "No enought memory");
    return res;
}

void obstack_chunk_free(void *ptr) {
    free(ptr);
}
