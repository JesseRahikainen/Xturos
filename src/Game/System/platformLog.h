#ifndef PLATFORM_LOG
#define PLATFORM_LOG

// right now this is very basic, just chooses where to output it to based on the platform
#if defined( __ANDROID__ )
/*
typedef enum android_LogPriority {
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,    // only for SetMinPriority()
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT,     // only for SetMinPriority(); must be last
} android_LogPriority;
*/

#include <android/log.h>
#define LOG_VERBOSE (int)ANDROID_LOG_VERBOSE
#define LOG_DEBUG (int)ANDROID_LOG_DEBUG
#define LOG_INFO (int)ANDROID_LOG_INFO
#define LOG_WARN (int)ANDROID_LOG_WARN
#define LOG_ERROR (int)ANDROID_LOG_ERROR
#define LOG_CRITICAL (int)ANDROID_LOG_FATAL
#else
/*
    SDL_LOG_PRIORITY_VERBOSE = 1,
    SDL_LOG_PRIORITY_DEBUG,
    SDL_LOG_PRIORITY_INFO,
    SDL_LOG_PRIORITY_WARN,
    SDL_LOG_PRIORITY_ERROR,
    SDL_LOG_PRIORITY_CRITICAL,
*/

#include <SDL_log.h>
#define LOG_VERBOSE (int)SDL_LOG_PRIORITY_VERBOSE
#define LOG_DEBUG (int)SDL_LOG_PRIORITY_DEBUG
#define LOG_INFO (int)SDL_LOG_PRIORITY_INFO
#define LOG_WARN (int)SDL_LOG_PRIORITY_WARN
#define LOG_ERROR (int)SDL_LOG_PRIORITY_ERROR
#define LOG_CRITICAL (int)SDL_LOG_PRIORITY_CRITICAL
#endif

void llog( int priority, const char* fmt, ... );

#endif // inclusion guard