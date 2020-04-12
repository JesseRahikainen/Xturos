#include "imageSheets.h"

#include <SDL_rwops.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "images.h"
#include "gfxUtil.h"

#include "../Utils/stretchyBuffer.h"
#include "../System/platformLog.h"
#include "../Utils/helpers.h"

// TODO: Convert this from a text format to binary.

typedef enum {
	RS_VERSION,
	RS_FILENAME,
	RS_SPRITES,
	RS_FINISHED
} ReadState;

/*
This opens up the sprite sheet file and loads all the images, putting the ids into imgOutArray. The returned array
 uses the stretchy buffer file, so you can use that to find the size, but you shouldn't do anything that modifies
 the size of it.
 Returns the number of images loaded if it was successful, otherwise returns -1.
*/
int img_LoadSpriteSheet( char* fileName, ShaderType shaderType, int** imgOutArray )
{
	int returnVal = 0;
	Vector2* sbMins = NULL;
	Vector2* sbMaxes = NULL;
	char** sbIDs = NULL;
	char* fileText = NULL;

	char buffer[512];
	SDL_RWops* rwopsFile = SDL_RWFromFile( fileName, "r" );
	if( rwopsFile == NULL ) {
		returnVal = -1;
		llog( LOG_ERROR, "Unable to open sprite sheet definition file: %s", fileName );
		goto clean_up;
	}

	// format version 2
	//  image file name
	//  spriteID rect.x rect.y rect.w rect.h
	//  final blank line
	int version;
	int numSpritesRead = 0;

	ReadState currentState = RS_VERSION;

	// first read in the text from the file, should never be too large
	int readAmt;
	while( ( readAmt = SDL_RWread( rwopsFile, (void*)buffer, sizeof( char ), ( sizeof( buffer ) / sizeof( buffer[0] ) ) ) ) != 0 ) {
		char* c = sb_Add( fileText, readAmt );
		for( int i = 0; i < readAmt; ++i ) {
			*c++ = buffer[i];
		}
	}
	sb_Push( fileText, 0 );

	const char* delim = "\r\n";
	char* line = strtok( fileText, delim );
	char* fileNameLoc;
	char* idEndLoc;
	char* rectStart;
	char workingBuffer[256] = { 0 };
	size_t idLen;
	char* id;
	Vector2 min, max;

	// now go through individual lines, parsing stuff as necessary
	while( line != NULL ) {
		switch( currentState ) {
		case RS_VERSION:
			version = SDL_strtol( line, NULL, 10 );
			currentState = RS_FILENAME;
			break;
		case RS_FILENAME:
			// assuming the file name will be local the .ss file, so we need to rip the directory off it and
			//  append the file name to it
			// line will be the file name for the image, attempt to create the texture
			SDL_strlcpy( workingBuffer, fileName, ARRAY_SIZE( workingBuffer ) );
			fileNameLoc = SDL_strrchr( workingBuffer, '/' );

			if( fileNameLoc != NULL ) {
				++fileNameLoc;
				(*fileNameLoc) = 0; // move the null terminator
				SDL_strlcat( workingBuffer, line, ARRAY_SIZE( workingBuffer ) );
			}
			currentState = RS_SPRITES;
			break;
		case RS_SPRITES:
			// id
			idEndLoc = SDL_strchr( line, ' ' );
			idLen = (uintptr_t)idEndLoc - (uintptr_t)line + 1;
			id = mem_Allocate( idLen );
			SDL_strlcpy( id, line, idLen );

			sb_Push( sbIDs, id );

			rectStart = idEndLoc + 1;

			// x, y, w, h
			min.x = (float)SDL_strtol( rectStart, &rectStart, 10 );
			++rectStart;
			min.y = (float)SDL_strtol( rectStart, &rectStart, 10 );
			++rectStart;
			max.x = min.x + ( (float)SDL_strtol( rectStart, &rectStart, 10 ) );
			++rectStart;
			max.y = min.y + ( (float)SDL_strtol( rectStart, &rectStart, 10 ) );

			sb_Push( sbMins, min );
			sb_Push( sbMaxes, max );

			++numSpritesRead;

			break;
		}
		line = strtok( NULL, delim );
	}

	sb_Add( *imgOutArray, numSpritesRead );

	// now go through and create all the images
	if( img_SplitImageFile( workingBuffer, numSpritesRead, shaderType, sbMins, sbMaxes, sbIDs, ( *imgOutArray ) ) < 0 ) {
		sb_Release( *imgOutArray );
		returnVal = -1;
		llog( LOG_ERROR, "Problem splitting image for sprite sheet definition file: %s", fileName );
		goto clean_up;
	}

	returnVal = numSpritesRead;

clean_up:

	sb_Release( fileText );

	sb_Release( sbMins );
	sb_Release( sbMaxes );
	for( size_t i = 0; i < sb_Count( sbIDs ); ++i ) {
		mem_Release( sbIDs[i] );
	}
	sb_Release( sbIDs );

	if( rwopsFile != NULL ) {
		SDL_RWclose( rwopsFile );
	}

	return returnVal;
}

/*
Cleans up all the images created from img_LoadSpriteSheet( ). The pointer passed in will be invalid after this
 is called.
*/
void img_UnloadSpriteSheet( int* imgArray )
{
	// free all the images
	int length = sb_Count( imgArray );
	for( int i = 0; i < length; ++i ) {
		img_Clean( imgArray[i] );
	}

	// free the array
	sb_Release( imgArray );
}