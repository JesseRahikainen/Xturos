#include "glDebugging.h"

#include <SDL_log.h>
#include "../Others/glew.h"

// some error check dumping, returns < 0 if there was an error
int checkAndLogErrors( const char* extraInfo )
{
#ifdef _DEBUG
	char* errorMsg;
#endif
	int ret = 0;
	GLenum error = glGetError( );

	while( error != GL_NO_ERROR ) {
#ifdef _DEBUG
		switch( error ) {
		case GL_INVALID_ENUM:
			errorMsg = "Invalid enumeration";
			break;
		case GL_INVALID_VALUE:
			errorMsg = "Invalid value";
			break;
		case GL_INVALID_OPERATION:
			errorMsg = "Invalid operation";
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			errorMsg = "Invalid framebuffer operation";
			break;
		case GL_OUT_OF_MEMORY:
			errorMsg = "Out of memory";
			break;
		default:
			errorMsg = "Unknown error";
			break;
		}

		if( extraInfo == NULL ) {
			SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error: %s", errorMsg );
		} else {
			SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error: %s at %s", errorMsg, extraInfo );
		}
#endif
		ret = -1;
		error = glGetError( );
	}

	return ret;
}