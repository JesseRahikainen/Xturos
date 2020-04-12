#include "imageSheets.h"

#include <SDL_rwops.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "images.h"
#include "gfxUtil.h"

#include "../Utils/stretchyBuffer.h"
#include "../System/platformLog.h"

// TODO: Convert this from a text format to binary.

typedef enum {
	RS_VERSION,
	RS_FILENAME,
	RS_COUNT,
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
	Vector2* mins = NULL;
	Vector2* maxes = NULL;
	char* fileText = NULL;

	char buffer[512];
	SDL_RWops* rwopsFile = SDL_RWFromFile( fileName, "r" );
	if( rwopsFile == NULL ) {
		returnVal = -1;
		llog( LOG_ERROR, "Unable to open sprite sheet definition file: %s", fileName );
		goto clean_up;
	}

	// format version 1
	//  version#
	//  image file name
	//  sprite count
	//  sprite rectangles
	//  final blank line
	int version;
	int numSprites = 0;
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
	char imgFileName[256] = { 0 };

	// now go through individual lines, parsing stuff as necessary
	while( line != NULL ) {
		switch( currentState ) {
		case RS_VERSION:
			version = strtol( line, NULL, 10 );
			currentState = RS_FILENAME;
			break;
		case RS_FILENAME:
			// assuming the file name will be local the .ss file, so we need to rip the directory off it and
			//  append the file name to it
			// line will be the file name for the image, attempt to create the texture
			strncpy( imgFileName, fileName, ( sizeof( imgFileName ) / sizeof( imgFileName[0] ) ) - 1 );
			fileNameLoc = strrchr( imgFileName, '/' );

			if( fileNameLoc != NULL ) {
				++fileNameLoc;
				strncpy( fileNameLoc, line, ( ( imgFileName + 255 ) - fileNameLoc ) );
			}
			currentState = RS_COUNT;
			break;
		case RS_COUNT:
			numSprites = strtol( line, NULL, 10 );
			if( numSprites == 0 ) {
				llog( LOG_ERROR, "Invalid number of sprites in sprite sheet definition file: %s", fileName );
				returnVal = -1;
				goto clean_up;
			} else {
				if( ( mins = mem_Allocate( sizeof( Vector2 ) * numSprites ) ) == NULL ) {
					llog( LOG_ERROR, "Unable to allocate minimums array for sprite sheet definition file: %s", fileName );
					returnVal = -1;
					goto clean_up;
				}
				
				if( ( maxes = mem_Allocate( sizeof( Vector2 ) * numSprites ) ) == NULL ) {
					llog( LOG_ERROR, "Unable to allocate maximums array for sprite sheet definition file: %s", fileName );
					returnVal = -1;
					goto clean_up;
				}
				
				sb_Add( *imgOutArray, numSprites );
				if( imgOutArray == NULL ) {
					llog( LOG_ERROR, "Unable to allocate image IDs array for sprite sheet definition file: %s", fileName );
					returnVal = -1;
					goto clean_up;
				}
			}
			numSpritesRead = 0;
			currentState = RS_SPRITES;
			break;
		case RS_SPRITES:
			// x, y, w, h
			mins[numSpritesRead].x = (float)strtol( line, &line, 10 );
			mins[numSpritesRead].y = (float)strtol( line, &line, 10 );
			maxes[numSpritesRead].x = mins[numSpritesRead].x + ( (float)strtol( line, &line, 10 ) );
			maxes[numSpritesRead].y = mins[numSpritesRead].y + ( (float)strtol( line, &line, 10 ) );
			++numSpritesRead;

			if( numSpritesRead >= numSprites ) {
				currentState = RS_FINISHED;
			}
			break;
		}
		line = strtok( NULL, delim );
	}

	if( currentState != RS_FINISHED ) {
		sb_Release( *imgOutArray );
		returnVal = -1;
		llog( LOG_ERROR, "Problem reading sprite sheet definition file: %s", fileName );
		goto clean_up;
	}

	// now go through and create all the images
	if( img_SplitImageFile( imgFileName, numSpritesRead, shaderType, mins, maxes, (*imgOutArray) ) < 0 ) {
		sb_Release( *imgOutArray );
		returnVal = -1;
		llog( LOG_ERROR, "Problem splitting image for sprite sheet definition file: %s", fileName );
		goto clean_up;
	}

	returnVal = numSpritesRead;

clean_up:

	sb_Release( fileText );

	mem_Release( mins );
	mem_Release( maxes );

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