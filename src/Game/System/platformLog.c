#include "platformLog.h"

#if defined( __ANDROID__ )
	#define ANDROID_LOG_TAG "Game Name"
	#define LOG( priority, str, ap ) __android_log_vprint( ( priority ), ANDROID_LOG_TAG, ( str ), ( ap ) )
#elif defined( __EMSCRIPTEN__ )
	#define LOG( priority, str, ap ) vprintf( ( str ), ( ap ) ); printf( "\n" )
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