#include "imageSheets.h"

#include <SDL_rwops.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "images.h"
#include "gfxUtil.h"
#include "Graphics/Platform/graphicsPlatform.h"

#include "Utils/stretchyBuffer.h"
#include "System/platformLog.h"
#include "Utils/helpers.h"
#include "System/jobQueue.h"
#include "Math/mathUtil.h"

#include "Others/cmp.h"

#include <stb_rect_pack.h>

typedef struct {
	Vector2* sbMins;
	Vector2* sbMaxes;
	char** sbIDs;
	uint32_t numSpritesRead;
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

static bool loadSpriteSheetData( const char* fileName, TempSpriteSheetData** outSBData )
{
	assert( fileName != NULL );
	assert( outSBData != NULL );

	bool ret = false;

	cmp_ctx_t cmp;
	SDL_RWops* rwopsFile = openRWopsCMPFile( fileName, "r", &cmp );
	if( rwopsFile == NULL ) goto clean_up;

#define CMP_READ( val, read, desc ) \
	if( !read( &cmp, &(val) ) ) { \
		llog( LOG_ERROR, "Unable to read %s for sprite sheet: %s", (desc), cmp_strerror( &cmp ) ); \
		goto clean_up; }

#define CMP_READ_STR( val, bufferSize, desc ) \
	if( !cmp_read_str( &cmp, (val), &(bufferSize) ) ) { \
		llog( LOG_ERROR, "Unable to read %s for sprite sheet: %s", (desc), cmp_strerror( &cmp ) ); \
		goto clean_up; }

	int version;
	CMP_READ( version, cmp_read_int, "version number" );
	if( version != 3 ) {
		llog( LOG_ERROR, "Unknown version for sprite sheet %s", fileName );
		goto clean_up;
	}

	uint32_t numGroups;
	CMP_READ( numGroups, cmp_read_array, "group count" );
	for( uint32_t i = 0; i < numGroups; ++i ) {
		TempSpriteSheetData data;
		data.sbMins = NULL;
		data.sbMaxes = NULL;
		data.sbIDs = NULL;

		uint32_t imageFileNameSize = ARRAY_SIZE( data.imageFileName );
		CMP_READ_STR( data.imageFileName, imageFileNameSize, "group image file" );

		CMP_READ( data.numSpritesRead, cmp_read_array, "sprite count" );
		for( uint32_t a = 0; a < data.numSpritesRead; ++a ) {
			char id[256];
			int x, y, w, h;

			uint32_t spriteIDSize = ARRAY_SIZE( id );
			CMP_READ_STR( id, spriteIDSize, "sprite id" );
			CMP_READ( x, cmp_read_int, "sprite x-coordinate" );
			CMP_READ( y, cmp_read_int, "sprite y-coordinate" );
			CMP_READ( w, cmp_read_int, "sprite width" );
			CMP_READ( h, cmp_read_int, "sprite height" );

			// copy the id and push it onto the data
			char* idCopy = createStringCopy( id );

			// add the mins and maxes for cutting up the sprites
			Vector2 min = vec2( (float)x, (float)y );
			Vector2 max = vec2( (float)( x + w ), (float)( y + h ) );

			sb_Push( data.sbMins, min );
			sb_Push( data.sbMaxes, max );
			sb_Push( data.sbIDs, idCopy );
		}

		sb_Push( *outSBData, data );
	}

	ret = true;

clean_up:

	SDL_RWclose( rwopsFile );

	if( !ret ) {
		// we've failed but have some stuff to clean up
		for( size_t i = 0; i < sb_Count( *outSBData ); ++i ) {
			cleanTempSpriteSheetData( &( ( *outSBData )[i] ) );
		}
		sb_Release( *outSBData );
	}

	return ret;
}

// This opens up the sprite sheet file and loads all the images, putting the ids into imgOutArray. The returned array
//  uses the stretchy buffer file, so you can use that to find the size, but you shouldn't do anything that modifies
//   the size of it. imgOutArray is optional, if you don't pass it in you'll have to retrieve the images by id.
// Returns the number of images loaded if it was successful, otherwise returns -1.
int img_LoadSpriteSheet( const char* fileName, ShaderType shaderType, int** imgOutArray )
{
	TempSpriteSheetData* tempData = NULL;

	int totalImageCount = 0;
	int packageID = -1;

	if( !loadSpriteSheetData( fileName, &tempData ) ) {
		return -1;
	}

	for( size_t i = 0; i < sb_Count( tempData ); ++i ) {
		int* outStart = NULL;
		if( imgOutArray != NULL ) {
			outStart = sb_Add( *imgOutArray, tempData[i].numSpritesRead );
		}

		Texture texture;
		if( gfxUtil_LoadTexture( tempData[i].imageFileName, &texture ) < 0 ) {
			llog( LOG_ERROR, "Unable to load texture for sprite sheet: %s", fileName );
			totalImageCount = -1;
			goto clean_up;
		}

		int* tempImgOutArray = NULL;
		sb_Add( tempImgOutArray, tempData[i].numSpritesRead );
		if( ( packageID = img_SplitTexture( &texture, tempData[i].numSpritesRead, shaderType, tempData[i].sbMins, tempData[i].sbMaxes, tempData[i].sbIDs, packageID, tempImgOutArray ) ) < 0 ) {
			totalImageCount = -1;
			llog( LOG_ERROR, "Problem splitting image for sprite sheet definition file: %s", fileName );
			goto clean_up;
		}

		totalImageCount += tempData[i].numSpritesRead;
		if( outStart != NULL ) {
			SDL_memcpy( outStart, tempImgOutArray, sizeof( int ) * sb_Count( tempImgOutArray ) );
		}
		sb_Release( tempImgOutArray );
	}

clean_up:
	if( totalImageCount < 0 ) {
		// clean up invalid stuff
		img_CleanPackage( packageID );
		sb_Release( *imgOutArray );
	}

	sb_Release( tempData );

	return totalImageCount;
}

void generateRects( SpriteSheetEntry* sbSpriteInfos, stbrp_rect** sbRects, int maxSize, int xPadding, int yPadding )
{
	for( size_t i = 0; i < sb_Count( sbSpriteInfos ); ++i ) {
		// create the rect for the image
		LoadedImage img;
		if( gfxUtil_LoadImageInfo( sbSpriteInfos[i].sbPath, &img ) < 0 ) {
			continue;
		}

		stbrp_rect newRect;
		newRect.w = img.width + xPadding;
		newRect.h = img.height + yPadding;
		newRect.id = (int)sb_Count( *sbRects );
		newRect.was_packed = 0;

		if( newRect.w > maxSize || newRect.h > maxSize ) {
			llog( LOG_ERROR, "File %s is too big and won't fit into the size limits. File will be ignored.", sbSpriteInfos[i].sbPath );
		} else {
			sb_Push( *sbRects, newRect );
		}
	}
}

typedef struct {
	char* fileName;
	stbrp_rect* sbRects;
	int width;
	int height;
} RectSet;

static void insertImage( char* filePath, stbrp_rect* rect, uint8_t* imageData, int imgWidth, int imgHeight, int topPadding, int leftPadding )
{
	LoadedImage img;
	if( gfxUtil_LoadImage( filePath, &img ) < 0 ) {
		llog( LOG_ERROR, "Unable to load image %s for inserting into sheet.", filePath );
		return;
	}
	
	int x = rect->x + leftPadding;
	int y = rect->y + topPadding;

	// copy each row
	for( int r = 0; r < img.height; ++r ) {
		uint8_t* base = imageData + ( ( x + ( imgWidth * ( r + y ) ) ) * 4 );
		SDL_memcpy( base, img.data + ( img.width * r * 4 ), 4 * img.width );
	}
}

static bool saveSpriteSheetDefinition( const char* fileName, SpriteSheetEntry* entries, size_t numEntries, RectSet* rectSets, size_t numRectSets )
{
	SDL_assert( numEntries == numRectSets );

	bool done = false;

	cmp_ctx_t cmp;
	SDL_RWops* rwops = openRWopsCMPFile( fileName, "w", &cmp );
	if( rwops == NULL ) {
		goto clean_up;
	}

#define CMP_WRITE( val, write, desc ) \
	if( !write( &cmp, (val) ) ) { \
		llog( LOG_ERROR, "Unable to write %s for sprite sheet: %s", (desc), cmp_strerror( &cmp ) ); \
		goto clean_up; }

#define CMP_WRITE_STR( val, desc ) \
	if( !cmp_write_str( &cmp, (val), (uint32_t)SDL_strlen((val)) ) ) { \
		llog( LOG_ERROR, "Unable to write %s for sprite sheet: %s", (desc), cmp_strerror( &cmp ) ); \
		goto clean_up; }

	// write out version number
	CMP_WRITE( 3, cmp_write_int, "version number" );

	//  write out all the image names
	uint32_t numImages = (uint32_t)numRectSets;
	CMP_WRITE( numImages, cmp_write_array, "rect set array size" );
	for( uint32_t i = 0; i < numImages; ++i ) {
		RectSet* set = &( rectSets[i] );

		CMP_WRITE_STR( set->fileName, "rect set image file name" );

		// write out the entries for each sprite stored in this image
		//  want to right out the id and rect
		uint32_t numSprites = (uint32_t)sb_Count( set->sbRects );
		CMP_WRITE( numSprites, cmp_write_array, "rect set sprite count" );
		for( uint32_t a = 0; a < numSprites; ++a ) {
			stbrp_rect* rect = &( set->sbRects[a] );
			char* id = SDL_strrchr( entries[rect->id].sbPath, '/' );
			if( id == NULL ) {
				id = entries[rect->id].sbPath;
			} else {
				++id; // advance past the '/'
			}
			CMP_WRITE_STR( id, "sprite id" );

			CMP_WRITE( rect->x, cmp_write_int, "sprite x-coordinate" );
			CMP_WRITE( rect->y, cmp_write_int, "sprite y-coordinate" );
			CMP_WRITE( rect->w, cmp_write_int, "sprite width" );
			CMP_WRITE( rect->h, cmp_write_int, "sprite height" );
		}
	}

#undef CMP_WRITE
#undef CMP_WRITE_STR

	done = true;

clean_up:

	if( SDL_RWclose( rwops ) < 0 ) {
		llog( LOG_ERROR, "Error flushing out file %s: %s", fileName, SDL_GetError( ) );
		done = false;
	}

	if( !done ) {
		remove( fileName );
	}

	return done;
}

// Takes in a list of file names and generates the sprite sheet and saves it out to fileName.
bool img_SaveSpriteSheet( const char* fileName, SpriteSheetEntry* sbEntries, int maxSize, int xPadding, int yPadding )
{
	bool done = false;
	SDL_RWops* rwops = NULL;

	int leftPadding = xPadding / 2;
	int topPadding = yPadding / 2;

	// first create the layouts, we may have to use mulitple image files to fit everything
	stbrp_rect* sbBaseRects = NULL;
	generateRects( sbEntries, &sbBaseRects, maxSize, xPadding, yPadding );

	if( sbBaseRects == NULL ) {
		llog( LOG_ERROR, "No images found when trying to save sprite sheet." );
		return false;
	}

	stbrp_context rpContext;

	// create the set of rect sets that we'll use to define where things are stored in the image
	//  we'll want to be able to use multiple images for one sprite sheet
	//  use increasing powers of 2 to generate the size, alternate between increasing width and height
	//  if we reach maximum size on both dimensions then grab everything in the current one and set it
	//   as a valid set that will be it's own image, remove all of those from the current rect set and
	//   continue
	RectSet* sbRectSets = NULL;
	
	while( sb_Count( sbBaseRects ) > 0 ) {

		// choose initial size, find largest on each dimension and then find the next highest power of two
		int maxWidth = -1;
		int maxHeight = -1;

		for( size_t i = 0; i < sb_Count( sbBaseRects ); ++i ) {
			if( sbBaseRects[i].w > maxWidth ) maxWidth = sbBaseRects[i].w;
			if( sbBaseRects[i].h > maxHeight ) maxHeight = sbBaseRects[i].h;
		}
		int nextHighestWidth = nextHighestPowerOfTwo( maxWidth );
		int nextHighestHeight = nextHighestPowerOfTwo( maxHeight );
		maxWidth = MIN( maxSize, nextHighestWidth );
		maxHeight = MIN( maxSize, nextHighestHeight );

		// try to find a tight packing for the images
		int packSuccess = 0;
		stbrp_node* sbNodes = NULL;
		while( !( packSuccess || ( ( maxWidth >= maxSize ) && ( maxHeight >= maxSize ) ) ) ) {

			sb_Clear( sbNodes );
			sb_Add( sbNodes, maxWidth );

			stbrp_init_target( &rpContext, maxWidth, maxHeight, sbNodes, (int)sb_Count( sbNodes ) );
			packSuccess = stbrp_pack_rects( &rpContext, sbBaseRects, (int)sb_Count( sbBaseRects ) );

			if( !packSuccess ) {
				if( maxWidth > maxHeight ) {
					maxHeight <<= 1;
				} else {
					maxWidth <<= 1;
				}
			}
		}

		// create the new RectSet, removing rects that have been packed
		RectSet newRectSet = { NULL, NULL, maxWidth, maxHeight };
		for( size_t i = 0; i < sb_Count( sbBaseRects ); ++i ) {
			if( !sbBaseRects[i].was_packed ) continue;

			sb_Push( newRectSet.sbRects, sbBaseRects[i] );
			sb_Remove( sbBaseRects, i );
			--i;
		}
		sb_Push( sbRectSets, newRectSet );
	}

	// for each RectSet create an image
	for( size_t i = 0; i < sb_Count( sbRectSets ); ++i ) {
		// add in length of underscore, .png, and terminator
		size_t fileNameLen = SDL_strlen( fileName ) + digitsInU32( (uint32_t)i ) + 6;
		sbRectSets[i].fileName = mem_Allocate( fileNameLen );
		int ret = SDL_snprintf( sbRectSets[i].fileName, fileNameLen, "%s_%i.png", fileName, (int)i );
		if( ( ret < 0 ) || ( ret >= fileNameLen ) ) {
			llog( LOG_ERROR, "Error creating file name for image. %s", ret >= 0 ? "String is too long." : "Encoding error." );
			goto clean_up;
		}

		size_t imgSize = sizeof( uint8_t ) * 4 * sbRectSets[i].width * sbRectSets[i].height;
		uint8_t* imageData = mem_Allocate( imgSize );
		SDL_memset( imageData, 0, imgSize );
		for( size_t a = 0; a < sb_Count( sbRectSets[i].sbRects ); ++a ) {
			insertImage( sbEntries[sbRectSets[i].sbRects[a].id].sbPath, &( sbRectSets[i].sbRects[a] ), imageData, sbRectSets[i].width, sbRectSets[i].height, topPadding, leftPadding );
		}
		gfxUtil_SaveImage( sbRectSets[i].fileName, imageData, sbRectSets[i].width, sbRectSets[i].height, 4 );
		mem_Release( imageData );
	}

	// adjust all the rectangles for padding
	for( size_t i = 0; i < sb_Count( sbRectSets ); ++i ) {
		RectSet* set = &( sbRectSets[i] );
		for( size_t a = 0; a < sb_Count( set->sbRects ); ++a ) {
			stbrp_rect* rect = &( set->sbRects[a] );
			rect->x += leftPadding;
			rect->y += topPadding;
			rect->w -= xPadding;
			rect->h -= yPadding;
		}
	}

	// now save out the sprite sheet definition
	done = saveSpriteSheetDefinition( fileName, sbEntries, sb_Count( sbEntries ), sbRectSets, sb_Count( sbRectSets ) );

clean_up:
	if( SDL_RWclose( rwops ) < 0 ) {
		llog( LOG_ERROR, "Error flushing out file %s: %s", fileName, SDL_GetError( ) );
		done = false;
	}

	if( !done ) {
		// something wrong happened, delete the files and let the user know
		llog( LOG_ERROR, "Error creating sprite sheet, cleaning up invalid files." );

		for( size_t i = 0; i < sb_Count( sbRectSets ); ++i ) {
			if( sbRectSets[i].fileName != NULL ) {
				llog( LOG_ERROR, "Removing invalid sprite sheet file %s.", sbRectSets[i].fileName );
				remove( sbRectSets[i].fileName );
			}
		}
	}

	for( size_t i = 0; i < sb_Count( sbRectSets ); ++i ) {
		mem_Release( sbRectSets[i].fileName );
		sb_Release( sbRectSets[i].sbRects );
	}
	sb_Release( sbRectSets );
	sb_Release( sbBaseRects );

	return done;
}

bool img_Save3x3( const char* fileName, const char* imageFileName, int width, int height, int left, int right, int top, int bottom )
{
	// images are assumed to start at the top left, first horizontal then vertical
	RectSet rectSets[1];
	rectSets[0].sbRects = NULL;
	rectSets[0].fileName = createStringCopy( imageFileName );
	rectSets[0].width = width;
	rectSets[0].height = height;
	sb_Add( rectSets[0].sbRects, 9 );

	SpriteSheetEntry entries[1];
	entries[0].sbPath = createStretchyStringCopy( imageFileName );

#define SET_RECT(i, xp, yp, wd, hg) rectSets[0].sbRects[i].x = xp; rectSets[0].sbRects[i].y = yp; rectSets[0].sbRects[i].w = wd; rectSets[0].sbRects[i].h = hg; rectSets[0].sbRects[i].was_packed = 1; rectSets[0].sbRects[i].id = 0;

	SET_RECT( 0, 0, 0, left, top );
	SET_RECT( 1, left, 0, right - left, top );
	SET_RECT( 2, right, 0, width - right, top );

	SET_RECT( 3, 0, top, left, bottom - top );
	SET_RECT( 4, left, top, right - left, bottom - top );
	SET_RECT( 5, right, top, width - right, bottom - top );

	SET_RECT( 6, 0, bottom, left, height - bottom );
	SET_RECT( 7, left, bottom, right - left, height - bottom );
	SET_RECT( 8, right, bottom, width - right, height - bottom );

	bool success = saveSpriteSheetDefinition( fileName, entries, 1, rectSets, 1 );

	mem_Release( rectSets[0].fileName );
	sb_Release( entries[0].sbPath );
	sb_Release( rectSets[0].sbRects );

	return success;
}

// Cleans up all the images created from img_LoadSpriteSheet( ). The pointer passed in will be invalid after this is called.
void img_UnloadSpriteSheet( int* imgArray )
{
	// free all the images
	size_t length = sb_Count( imgArray );
	for( size_t i = 0; i < length; ++i ) {
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
	LoadedImage* sbLoadedImages;
	TempSpriteSheetData* sbTempSheetData;

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
	int ret = 0;
	int packageID = -1;
	for( size_t i = 0; i < sb_Count( loadData->sbTempSheetData ); ++i ) {
		Texture texture;
		if( gfxPlatform_CreateTextureFromLoadedImage( TF_RGBA, &( loadData->sbLoadedImages[i] ), &texture ) < 0 ) {
			llog( LOG_DEBUG, "Unable to create texture for %s", loadData->fileName );
			goto clean_up;
		}

		packageID = img_SplitTexture( &texture, loadData->sbTempSheetData[i].numSpritesRead, loadData->shaderType,
			loadData->sbTempSheetData[i].sbMins, loadData->sbTempSheetData[i].sbMaxes, loadData->sbTempSheetData[i].sbIDs, packageID, NULL );
		if( packageID == -1 ) {
			llog( LOG_DEBUG, "Unable to split texture for %s", loadData->fileName );
			goto clean_up;
		}
	}

	if( loadData->onLoadDone != NULL ) loadData->onLoadDone( ret );

clean_up:
	for( size_t i = 0; i < sb_Count( loadData->sbTempSheetData ); ++i ) {
		cleanTempSpriteSheetData( &( loadData->sbTempSheetData[i] ) );
		gfxUtil_ReleaseLoadedImage( &( loadData->sbLoadedImages[i] ) );
	}
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
	if( !loadSpriteSheetData( loadData->fileName, &( loadData->sbTempSheetData ) ) ) {
		goto error;
	}

	LoadedImage* start = sb_Add( loadData->sbLoadedImages, sb_Count( loadData->sbTempSheetData ) );
	SDL_memset( start, 0, sizeof( loadData->sbLoadedImages[0] ) * sb_Count( loadData->sbTempSheetData ) );
	for( size_t i = 0; i < sb_Count( loadData->sbTempSheetData ); ++i ) {
		if( gfxUtil_LoadImage( loadData->sbTempSheetData[i].imageFileName, &( loadData->sbLoadedImages[i] ) ) < 0 ) {
			goto error;
		}
	}

	jq_AddMainThreadJob( bindSpriteSheetJob, data );

	return;

error:
	for( size_t i = 0; i < sb_Count( loadData->sbTempSheetData ); ++i ) {
		cleanTempSpriteSheetData( &( loadData->sbTempSheetData[i] ) );
		if( loadData->sbLoadedImages[i].data != NULL ) {
			gfxUtil_ReleaseLoadedImage( &( loadData->sbLoadedImages[i] ) );
		}
	}
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
	loadData->sbTempSheetData = NULL;
	loadData->sbLoadedImages = NULL;

	jq_AddJob( loadSpriteSheetJob, (void*)loadData );
}
