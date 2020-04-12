#include "glDebugging.h"

#include "../System/platformLog.h"

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
			llog( LOG_ERROR, "Error: %s", errorMsg );
		} else {
			llog( LOG_ERROR, "Error: %s at %s", errorMsg, extraInfo );
		}
#endif
		ret = -1;
		error = glGetError( );
	}

	return ret;
}

// checks to see if the frame buffer is complete, returns < 0 if there was an error
int checkAndLogFrameBufferCompleteness( GLenum target, const char* extraInfo )
{
#ifdef _DEBUG
	char* errorMsg;
#endif
	GLenum status; 
	int error;

	GLRC( status, error, glCheckFramebufferStatus( target ) );

	if( error < 0 ) {
		return -1;
	}

	if( status == GL_FRAMEBUFFER_COMPLETE ) {
		return 0;
	}

#ifdef _DEBUG
	switch( status ) {
	case GL_FRAMEBUFFER_UNDEFINED:
		// GL_FRAMEBUFFER_UNDEFINED is returned if target is the default framebuffer, but the default framebuffer does not exist.
		errorMsg = "Framebuffer Undefined";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		// GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT is returned if any of the framebuffer attachment points are framebuffer incomplete.
		errorMsg = "Framebuffer Incomplete Attachment";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		// GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT is returned if the framebuffer does not have at least one image attached to it.
		errorMsg = "Framebuffer Incomplete Missing Attachment";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		// GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER is returned if the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE
		//  is GL_NONE for any color attachment point(s) named by GL_DRAW_BUFFERi.        
		errorMsg = "Framebuffer Incomplete Draw Buffer";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		// GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER is returned if GL_READ_BUFFER is not GL_NONE
		//  and the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for the color attachment point named
		//  by GL_READ_BUFFER.
		errorMsg = "Framebuffer Incomplete Read Buffer";
		break;
	case GL_FRAMEBUFFER_UNSUPPORTED:
		// GL_FRAMEBUFFER_UNSUPPORTED is returned if the combination of internal formats of the attached images violates
		//  an implementation-dependent set of restrictions.
		errorMsg = "Framebuffer Unsupported";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
		// GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE is returned if the value of GL_RENDERBUFFER_SAMPLES is not the same
		//  for all attached renderbuffers; if the value of GL_TEXTURE_SAMPLES is the not same for all attached textures; or, if the attached
		//  images are a mix of renderbuffers and textures, the value of GL_RENDERBUFFER_SAMPLES does not match the value of
		//  GL_TEXTURE_SAMPLES.
		errorMsg = "Framebuffer Incomplete Multisample";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
		// GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS is returned if any framebuffer attachment is layered, and any populated attachment is not layered,
		//  or if all populated color attachments are not from textures of the same target.
		errorMsg = "Framebuffer Incomplete Layer Targets";
		break;
	default:
		errorMsg = "Unknown Error Type";
		break;
	}

	if( extraInfo == NULL ) {
		llog( LOG_ERROR, "FB Status: %s", errorMsg );
	} else {
		llog( LOG_ERROR, "FB Status: %s at %s", errorMsg, extraInfo );
	}
#endif
	return -1;
}