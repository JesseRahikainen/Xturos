#include "platformLog.h"

#if defined( __ANDROID__ )

// TODO: Move this somewhere else
#define ANDROID_LOG_TAG "Tiny Tactics"
#define LOG( priority, str, ap ) __android_log_vprint( ( priority ), ANDROID_LOG_TAG, ( str ), ( ap ) )
#else
#define LOG( priority, str, ap ) SDL_LogMessageV( SDL_LOG_CATEGORY_APPLICATION, ( priority ), ( str ), ( ap ) )
#endif

void llog( int priority, const char* fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	LOG( priority, fmt, ap );
	va_end( ap );
}