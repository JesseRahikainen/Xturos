#include <stdio.h>
#include <Windows.h>
#include <stdbool.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "stretchyBuffer.h"

// simple command line program to generate sprite sheets for Xturos, uses the nothings library for most everything
//  interface: SpriteSheetGenerator outputName -f fileToAdd -d directoryToAdd
// -f and -d can be used numerous times


typedef struct {
	char* filePath;
} SpriteInfo;


int paddingX = 1;
int paddingY = 1;

stbrp_rect* sbRects = NULL;
SpriteInfo* sbSpriteInfos = NULL;

// returns a new string, you will have to free() it yourself
char* getFileName( char* filePath )
{
	assert( filePath != NULL );

	// find the last '\' in filePath
	size_t len = strlen( filePath );
	size_t start = len;
	while( ( start > 0 ) && !( ( filePath[start] == '\\' ) || ( filePath[start] == '/' ) ) ) {
		--start;
	}

	if( ( filePath[start] == '\\' ) || ( filePath[start] == '/' ) ) {
		++start;
	}

	size_t nameLen = ( len - start );
	char* fileName = malloc( sizeof( char ) * ( nameLen + 1 ) );
	memset( fileName, 0, nameLen + 1 );

	strncpy( fileName, filePath + start, nameLen );

	return fileName;	
}

void addFile( char* filePath )
{
	// get just the file name without the path included
	SpriteInfo newSpriteInfo;
	
	newSpriteInfo.filePath = filePath;
	newSpriteInfo.filePath = malloc( sizeof( char ) * ( strlen( filePath ) + 1 ) );
	strcpy( newSpriteInfo.filePath, filePath );

	// attempt to open the file to make sure it's an image, as well was getting the width and height
	int w, h, comp;
	unsigned char* data = stbi_load( newSpriteInfo.filePath, &w, &h, &comp, 4 );
	if( data == NULL ) {
		fprintf( stderr, "File %s is probably not an image: %s", newSpriteInfo.filePath, stbi_failure_reason( ) );
		free( newSpriteInfo.filePath );
		return;
	}
	stbi_image_free( data );

	stbrp_rect newRect;
	newRect.w = w + paddingX;
	newRect.h = h + paddingY;
	newRect.id = sb_Count( sbSpriteInfos );
	newRect.was_packed = 0;

	sb_Push( sbRects, newRect );
	sb_Push( sbSpriteInfos, newSpriteInfo );
}

void addDirectory( char* directoryPath )
{
	char* searchPath = malloc( sizeof( char ) * strlen( directoryPath ) + 3 );
	strcpy( searchPath, directoryPath );
	strcat( searchPath, "\\*" );
	// go through each file in the directory and add them
	WIN32_FIND_DATAA findFileData;
	HANDLE dirIterHandle = FindFirstFileA( (LPCSTR)searchPath, &findFileData );

	if( dirIterHandle == INVALID_HANDLE_VALUE ) {
		fprintf( stderr, "Unable to open directory %s, error: %d", directoryPath, GetLastError( ) );
		goto clean_up;
	}

	do {
		if( findFileData.dwFileAttributes & ( FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE ) ) {

			char* fullPath = malloc( sizeof( char ) * ( strlen( directoryPath ) + 1 + strlen( findFileData.cFileName ) + 1 ) );
			strcpy( fullPath, directoryPath );
			strcat( fullPath, "\\" );
			strcat( fullPath, findFileData.cFileName );

			addFile( fullPath );

			free( fullPath );
		}
	} while( FindNextFileA( dirIterHandle, &findFileData ) != 0 );

clean_up:
	free( searchPath );
}

int nextHighestPowerOfTwo( int v )
{
	v += ( v == 0 ); // fixes case where v == 0 returns 0

	//v--;
	// fill everything to the right of the highest set bit with 1
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	// add 1 to get the next highest power of 2
	v++;

	return v;
}

void insertImage( uint8_t* imageData, int imgWidth, int imgHeight, size_t rectIdx )
{
	int w, h, comp;
	uint8_t* data = stbi_load( sbSpriteInfos[sbRects[rectIdx].id].filePath, &w, &h, &comp, 4 );

	int x = sbRects[rectIdx].x;
	int y = sbRects[rectIdx].y;

	// copy each row
	for( int r = 0; r < h; ++r ) {
		uint8_t* base = imageData + ( ( x + ( imgWidth * ( r + y ) ) ) * 4 );
		memcpy( base, data + ( w * r * 4 ), 4 * w );
	}
}

int main( int argc, char** argv )
{
	int ret = 0;

	char* outputName = NULL;

	// loop through arguments
	//  first should be output name
	//  -f means to add this individual file
	//  -d means to add all files in this directory
	for( int i = 1; i < argc; ++i ) {
		if( strcmp( argv[i], "-h" ) == 0 ) {
			fprintf( stdout, "Used to generate a sprite sheet.\n" );
			fprintf( stdout, "Will create two files, a png and ss file. The ss file will define the sprite sheet, listing the name, position and size of all the sprites. The png will have the actual images.\n" );
			fprintf( stdout, "This will accept any number of files to include. You can also include all the files in a directory.\n" );
			fprintf( stdout, "Useage: SpriteSheetGenerator outname -xp 1 -yp 1 -f file_to_include -d directory_to_include\n" );
			fprintf( stdout, "Generates outname.ss and outname.png.\n" );
			fprintf( stdout, "-f to add a file to process.\n" );
			fprintf( stdout, "-d to add all the files in a directory.\n" );
			fprintf( stdout, "-xp to set the padding on the x dimension. Defaults to 1.\n" );
			fprintf( stdout, "-yp to set the padding on the y dimension. Defaults to 1.\n" );
		} else if( strcmp( argv[i], "-f" ) == 0 ) {
			++i;
			if( i < argc ) {
				addFile( argv[i] );
			}
		} else if( strcmp( argv[i], "-d" ) == 0 ) {
			++i;
			if( i < argc ) {
				addDirectory( argv[i] );
			}
		} else if( strcmp( argv[i], "-xp" ) == 0 ) {
			++i;
			if( i < argc ) {
				paddingX = atoi( argv[i] );
				if( paddingX < 0 ) paddingX = 0;
			}
		} else if( strcmp( argv[i], "-yp" ) == 0 ) {
			paddingY = atoi( argv[i] );
			if( paddingY < 0 ) paddingY = 0;
		} else {
			outputName = argv[i];
		}
	}

	stbrp_context rpContext;

	// for the starting size we'll find the largest along each dimension and choose the
	//  next highest power of 2
	int maxW = -1;
	int maxH = -1;
	for( size_t i = 0; i < sb_Count( sbRects ); ++i ) {
		if( sbRects[i].w > maxW ) {
			maxW = sbRects[i].w;
		}

		if( sbRects[i].h > maxH ) {
			maxH = sbRects[i].h;
		}
	}

	int width = nextHighestPowerOfTwo( maxW );
	int height = nextHighestPowerOfTwo( maxH );
	bool increaseWidth = true;

	// while not everything is packed attempt to pack, if not everything was packed increase the lowest
	//  of the width and height to the next power of 2
	// not optimal, but packing is NP-hard and this isn't meant to run real time
	int success = 0;
	stbrp_node* sbNodes = NULL;
	do {
		printf( "trying %i x %i\n", width, height );
		sb_Clear( sbNodes );
		sb_Add( sbNodes, width );

		stbrp_init_target( &rpContext, width, height, sbNodes, sb_Count( sbNodes ) );
		success = stbrp_pack_rects( &rpContext, sbRects, sb_Count( sbRects ) );

		if( !success ) {
			if( width < height ) {
				width = width << 1;
			} else {
				height = height << 1;
			}
		}
	} while( !success );
	sb_Release( sbNodes );

	// every rectangle should now be packed, create the image and the sprite sheet files

	// first create the image
	size_t imgSize = sizeof( uint8_t ) * 4 * width * height;
	uint8_t* imageData = malloc( imgSize );
	memset( imageData, 0, imgSize );
	for( size_t i = 0; i < sb_Count( sbRects ); ++i ) {
		insertImage( imageData, width, height, i );
	}
	char* imageFileName = malloc( sizeof( char ) * strlen( outputName ) + 4 + 1 );
	strcpy( imageFileName, outputName );
	strcat( imageFileName, ".png" );

	stbi_write_png( imageFileName, width, height, 4, imageData, 0 );

	/*
	Format for layout text:
	2
	imageFileName
	spriteName x y w h
	repeat above line for every sprite in the sheet
	*/
	char* layoutFileName = malloc( sizeof( char ) * strlen( outputName ) + 3 + 1 );
	strcpy( layoutFileName, outputName );
	strcat( layoutFileName, ".ss" );

	FILE* layoutFile = fopen( layoutFileName, "w" );
	char* fileName = strrchr( imageFileName, '\\' );
	if( fileName ) ++fileName;
	fprintf( layoutFile, "2\n%s", fileName );
	for( size_t i = 0; i < sb_Count( sbRects ); ++i ) {
		fileName = strrchr( sbSpriteInfos[sbRects[i].id].filePath, '\\' );
		if( fileName ) ++fileName;
		fprintf( layoutFile, "\n%s %i %i %i %i", fileName, sbRects[i].x, sbRects[i].y, sbRects[i].w - paddingX, sbRects[i].h - paddingY );
	}
	fclose( layoutFile );

	free( layoutFileName );
	free( imageFileName );

	return ret;
}