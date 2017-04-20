#include "glPlatform.h"

#include "../System/platformLog.h"

int glInit( void )
{
#if defined( WIN32 )

	int loadVal = ogl_LoadFunctions( );
	if( loadVal == ogl_LOAD_FAILED ) {
		llog( LOG_ERROR, "Error in initRendering while calling ogl_LoadFunctions." );
		return -1;
	}
	if( loadVal > ogl_LOAD_SUCCEEDED ) {
		llog( LOG_ERROR, "ogl_LoadFunctions( ) succeeded but had %i failures.", ( loadVal - ogl_LOAD_SUCCEEDED ) );
	}
	return 0;

#elif defined( __ANDROID__ )
	return 0;
#elif defined( __EMSCRIPTEN__ )
	return 0;
#else
	llog( LOG_ERROR, "OpenGL not setup for this platform." );
	return -1;
#endif
}