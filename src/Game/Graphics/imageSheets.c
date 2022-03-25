#include "imageSheets.h"

#include <SDL_rwops.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "images.h"
#include "gfxUtil.h"
#include "Graphics/Platform/graphicsPlatform.h"

#include "Utils/stretchyBuffer.h"
#include "System/platformLog.h"
#include "Utils/helpers.h"
#include "System/jobQueue.h"

// TODO: Convert this from a text format to binary.

typedef enum {
	RS_VERSION,
	RS_FILENAME,
	RS_SPRITES,
	RS_FINISHED
} ReadState;

typedef struct {
	Vector2* sbMins;
	Vector2* sbMaxes;
	char** sbIDs;
	int numSpritesRead;
	char imageFileName[256];
} TempSpriteSheetData;

static void cleanTempSpriteSheetData( TempSpriteSheetData* data )
{
	sb_Release( data->sbMins );
	sb_Release( data->sbMaxes );
	for( size_t i = 0; i < sb_Count( data->sbIDs ); ++i ) {
		mem_Release( data->sbIDs[i] );
	}
	sb_Release( data->sbIDs );
}

static bool loadSpriteSheetData( const char* fileName, TempSpriteSheetData* outData )
{
	assert( fileName != NULL );
	assert( outData != NULL );

	bool ret = false;

	char* fileText = NULL;

	char buffer[512];
	SDL_RWops* rwopsFile = SDL_RWFromFile( fileName, "r" );
	if( rwopsFile == NULL ) {
		llog( LOG_ERROR, "Unable to open sprite sheet definition file: %s", fileName );
		goto clean_up;
	}

	// format version 2
	//  image file name
	//  spriteID rect.x rect.y rect.w rect.h
	//  final blank line
	//  lines starting with # are ignored
	int version;
	outData->numSpritesRead = 0;

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
	size_t idLen;
	char* id;
	Vector2 min, max;

	// now go through individual lines, parsing stuff as necessary
	while( line != NULL ) {
		if( line[0] != '#' ) {
			switch( currentState ) {
			case RS_VERSION:
				version = SDL_strtol( line, NULL, 10 );
				assert( version == 2 );
				currentState = RS_FILENAME;
				break;
			case RS_FILENAME:
				// assuming the file name will be local to the .ss file, so we need to rip the directory off it and
				//  append the file name to it
				// line will be the file name for the image, attempt to create the texture
				SDL_strlcpy( outData->imageFileName, fileName, ARRAY_SIZE( outData->imageFileName ) );
				fileNameLoc = SDL_strrchr( outData->imageFileName, '/' );

				if( fileNameLoc != NULL ) {
					++fileNameLoc;
					( *fileNameLoc ) = 0; // move the null terminator
					SDL_strlcat( outData->imageFileName, line, ARRAY_SIZE( outData->imageFileName ) );
				}
				currentState = RS_SPRITES;
				break;
			case RS_SPRITES:
				// id
				idEndLoc = SDL_strchr( line, ' ' );
				idLen = (uintptr_t)idEndLoc - (uintptr_t)line + 1;
				id = mem_Allocate( idLen );
				SDL_strlcpy( id, line, idLen );

				sb_Push( outData->sbIDs, id );

				rectStart = idEndLoc + 1;
				while( ( *rectStart ) == ' ' ) ++rectStart;

				// x, y, w, h
				min.x = (float)SDL_strtol( rectStart, &rectStart, 10 );
				while( (*rectStart) == ' ' ) ++rectStart;
				min.y = (float)SDL_strtol( rectStart, &rectStart, 10 );
				while( ( *rectStart ) == ' ' ) ++rectStart;
				max.x = min.x + ( (float)SDL_strtol( rectStart, &rectStart, 10 ) );
				while( ( *rectStart ) == ' ' ) ++rectStart;
				max.y = min.y + ( (float)SDL_strtol( rectStart, &rectStart, 10 ) );

				sb_Push( outData->sbMins, min );
				sb_Push( outData->sbMaxes, max );

				++( outData->numSpritesRead );

				break;
			}
		}
		line = strtok( NULL, delim );
	}

	ret = true;

clean_up:
	sb_Release( fileText );

	if( rwopsFile != NULL ) {
		SDL_RWclose( rwopsFile );
	}

	return ret;
}

/*
This opens up the sprite sheet file and loads all the images, putting the ids into imgOutArray. The returned array
 uses the stretchy buffer file, so you can use that to find the size, but you shouldn't do anything that modifies
 the size of it.
 Returns the number of images loaded if it was successful, otherwise returns -1.
*/
int img_LoadSpriteSheet( char* fileName, ShaderType shaderType, int** imgOutArray )
{
	TempSpriteSheetData tempData;

	int returnVal = 0;

	tempData.sbIDs = NULL;
	tempData.sbMaxes = NULL;
	tempData.sbMins = NULL;
	tempData.numSpritesRead = 0;
	tempData.imageFileName[0] = '\0';

	if( !loadSpriteSheetData( fileName, &tempData ) ) {
		return -1;
	}

	sb_Add( *imgOutArray, tempData.numSpritesRead );

	Texture texture;
	if( gfxUtil_LoadTexture( tempData.imageFileName, &texture ) < 0 ) {
		llog( LOG_ERROR, "Unable to load texture for sprite sheet: %s", fileName );
		returnVal = -1;
		goto clean_up;
	}

	// now go through and create all the images
	if( img_SplitTexture( &texture, tempData.numSpritesRead, shaderType, tempData.sbMins, tempData.sbMaxes, tempData.sbIDs, ( *imgOutArray ) ) < 0 ) {
		sb_Release( *imgOutArray );
		returnVal = -1;
		llog( LOG_ERROR, "Problem splitting image for sprite sheet definition file: %s", fileName );
		goto clean_up;
	}

	returnVal = tempData.numSpritesRead;

clean_up:
	cleanTempSpriteSheetData( &tempData );

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


//****************************************************************************
typedef struct {
	const char* fileName;
	ShaderType shaderType;

	//Texture loadedTexture;
	LoadedImage loadedImage;
	TempSpriteSheetData tempSheetData;

	void (*onLoadDone)( int );
} ThreadedSpriteSheetLoadData;

static void bindSpriteSheetJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_ERROR, "No data, unable to properly bind sprite sheet load" );
		return;
	}

	ThreadedSpriteSheetLoadData* loadData = (ThreadedSpriteSheetLoadData*)data;

	//llog( LOG_DEBUG, "Done loading %s", loadData->fileName );

	Texture texture;
	if( gfxPlatform_CreateTextureFromLoadedImage( TF_RGBA, &( loadData->loadedImage ), &texture ) < 0 ) {
		llog( LOG_DEBUG, "Unable to create texture for %s", loadData->fileName );
		goto clean_up;
	}

	int ret = img_SplitTexture( &texture, loadData->tempSheetData.numSpritesRead, loadData->shaderType,
		loadData->tempSheetData.sbMins, loadData->tempSheetData.sbMaxes, loadData->tempSheetData.sbIDs, NULL );

	if( loadData->onLoadDone != NULL ) loadData->onLoadDone( ret );

clean_up:
	cleanTempSpriteSheetData( &( loadData->tempSheetData ) );
	gfxUtil_ReleaseLoadedImage( &( loadData->loadedImage ) );
	mem_Release( data );
}

static void loadSpriteSheetFailedJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_ERROR, "No data, unable to properly fail sprite sheet load" );
		return;
	}

	ThreadedSpriteSheetLoadData* loadData = (ThreadedSpriteSheetLoadData*)data;

	if( loadData->onLoadDone != NULL ) loadData->onLoadDone( -1 );

	mem_Release( data );
}

static void loadSpriteSheetJob( void* data )
{
	if( data == NULL ) {
		llog( LOG_ERROR, "No data, unable to load sprite sheet" );
		return;
	}

	ThreadedSpriteSheetLoadData* loadData = (ThreadedSpriteSheetLoadData*)data;

	// there are two things we have to load here, the file data from the sprite sheet file, and the image
	if( !loadSpriteSheetData( loadData->fileName, &( loadData->tempSheetData ) ) ) {
		goto error;
	}

	if( gfxUtil_LoadImage( loadData->tempSheetData.imageFileName, &( loadData->loadedImage ) ) < 0 ) {
		goto error;
	}

	jq_AddMainThreadJob( bindSpriteSheetJob, data );

	return;

error:
	cleanTempSpriteSheetData( &( loadData->tempSheetData ) );
	jq_AddMainThreadJob( loadSpriteSheetFailedJob, data );
}

// assumes we'll be using img_GetExistingByID() to retrieve them after the load is done
void img_ThreadedLoadSpriteSheet( const char* fileName, ShaderType shaderType, void (*onLoadDone)( int ) )
{
	ThreadedSpriteSheetLoadData* loadData = mem_Allocate( sizeof( ThreadedSpriteSheetLoadData ) );
	if( loadData == NULL ) {
		llog( LOG_ERROR, "Unable to allocated data storage in img_ThreadedLoadSpriteSheet" );
		if( onLoadDone != NULL ) onLoadDone( -1 );
		return;
	}

	loadData->fileName = fileName;
	loadData->shaderType = shaderType;
	loadData->onLoadDone = onLoadDone;
	memset( &( loadData->tempSheetData ), 0, sizeof( loadData->tempSheetData ) );

	jq_AddJob( loadSpriteSheetJob, (void*)loadData );
}
