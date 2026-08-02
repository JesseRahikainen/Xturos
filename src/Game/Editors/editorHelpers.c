#include "editorHelpers.h"

#include <SDL3/SDL.h>

#include <stdbool.h>

#ifdef WIN32
#include <Windows.h>
#include <commdlg.h>
#include <stringapiset.h>
#endif

#include "editorHub.h"
#include "System/memory.h"
#include "System/platformLog.h"
#include "Utils/helpers.h"
#include "Utils/stretchyBuffer.h"
#include "Graphics/imageSheets.h"

#ifdef WIN32
static void convertDirDividers( char** sbFilePath )
{
	for(size_t i = 0; i < sb_Count( *sbFilePath ); ++i) {
		if(( *sbFilePath )[i] == '\\') ( *sbFilePath )[i] = '/';
	}
}
#else
static void convertDirDividers( char** sbFilePath )
{
	ASSERT( false && "Not supported on this platform." );
}
#endif

static char* sbRootDirectory = NULL;
static void getWorkingDirectory( void );

// convert a full path to a local path
static void toLocalPath( char** sbFilePath )
{
	getWorkingDirectory( );

	ASSERT( sbRootDirectory != NULL );
	ASSERT( sbFilePath != NULL );

	// create local copy of root directory
	size_t rootPathSize = sb_Count( sbRootDirectory );
	char* sbRoot = NULL;
	sb_Add( sbRoot, rootPathSize );
	memcpy( sbRoot, sbRootDirectory, rootPathSize );

	// first replace all instances of '\\' with '/'
	convertDirDividers( sbFilePath );

	// now track forward along both the root directory string and the passed in file path
	//  until they don't match, cut of everything before that, want to divide it up into
	//  '/' chunks
	size_t filePathSize = sb_Count( *sbFilePath );

	const char* tokens = "/";

	char* filePathTokSave = NULL;
	char* filePathToken = SDL_strtok_r( *sbFilePath, tokens, &filePathTokSave );
	

	char* rootPathTokSave = NULL;
	char* rootPathToken = SDL_strtok_r( sbRoot, tokens, &rootPathTokSave );

	// eat through the tokens until we get something that doesn't match
	bool tokensMatch = ( strcmp( filePathToken, rootPathToken ) == 0 );
	while(( filePathToken != NULL ) && ( rootPathToken != NULL ) && tokensMatch) {
		filePathToken = SDL_strtok_r( NULL, tokens, &filePathTokSave );
		rootPathToken = SDL_strtok_r( NULL, tokens, &rootPathTokSave );

		if(( filePathToken != NULL ) && ( rootPathToken != NULL )) {
			tokensMatch = ( strcmp( filePathToken, rootPathToken ) == 0 );
		}
	}

	char* sbLocalFilePath = NULL;

	// get all the directories from the path that we didn't hit in the root path and add ../ to the starfor them
	while( rootPathToken != NULL ) {
		rootPathToken = SDL_strtok_r( NULL, tokens, &rootPathTokSave );
		sb_Push( sbLocalFilePath, '.' );
		sb_Push( sbLocalFilePath, '.' );
		sb_Push( sbLocalFilePath, '/' );
	}

	// grab the rest of the tokens from sbFilePath and append them to a new string
	while(filePathToken != NULL) {
		if(sbLocalFilePath != NULL) sb_Push( sbLocalFilePath, '/' );
		size_t tokenLen = strlen( filePathToken );
		char* strAddSpot = sb_Add( sbLocalFilePath, tokenLen );
		memcpy( strAddSpot, filePathToken, sizeof( char ) * tokenLen );

		filePathToken = SDL_strtok_r( NULL, tokens, &filePathTokSave );
	}
	// add terminating null
	sb_Push( sbLocalFilePath, 0 );

	// release the old sbFilePath and give it the new one
	sb_Release( *sbFilePath );
	*sbFilePath = sbLocalFilePath;

	// clean up local file root
	sb_Release( sbRoot );
}

// returns the image id loaded
int editor_loadImageFile( const char* filePath )
{
	ASSERT( filePath != NULL );
	llog( LOG_DEBUG, "Loading image file %s...", filePath );

	int loadedImage = img_Load( filePath, ST_DEFAULT );
	if(loadedImage < 0) {
		hub_CreateDialog( "Error", "Unable to load image. Check log file for details.", DT_ERROR, 1, "OK", NULL );
	}
	return loadedImage;
}

// returns a stretchybuffer of image ids, you'll need to manage the memory here
int editor_loadSpriteSheetFile( const char* filePath, ImageID** outImgSB )
{
	ASSERT( filePath != NULL );
	llog( LOG_DEBUG, "Loading sprite sheet file %s...", filePath );

	int packageID = -1;
	if( ( packageID = img_LoadSpriteSheet( filePath, ST_DEFAULT, outImgSB ) ) < 0) {
		hub_CreateDialog( "Error", "Unable to load sprite sheet. Check log file for details.", DT_ERROR, 1, "OK", NULL );
	}
	return packageID;
}

#ifdef WIN32
// adapt to the SDL file dialogs where they return a list of things
void sdlFileDialogCallback( void* userData, const char* const* fileList, int filter )
{
	void ( *callback )( const char* ) = ( void( * )( const char* ) )userData;

	if( fileList == NULL ) {
		hub_CreateDialog( "Error", SDL_GetError(), DT_ERROR, 1, "OK", NULL );
	}

	if( *fileList == NULL ) {
		return;
	}

	while( *fileList ) {
		if( callback != NULL ) {
			// create a local copy of the string and make it local
			char* copy = createStretchyStringCopy( *fileList );
			toLocalPath( &copy );
			callback( copy );
			sb_Release( copy );
		}
		++fileList;
	}
}

// returns a stretchy buffer, be sure to release it
void editor_chooseLoadFileLocation( const char* fileTypeDesc, const char* fileExtension, bool multiSelect, void ( *callback )( const char* ) )
{
	char* fullDescTemplate = "%s (*.%s)";
	size_t totalLen = sizeof( char ) * ( SDL_strlen( fileTypeDesc ) + SDL_strlen( fileExtension ) + 6 );
	char* fullDesc = mem_Allocate( sizeof( char ) * totalLen );
	SDL_snprintf( fullDesc, totalLen, fullDescTemplate, fileTypeDesc, fileExtension );
	const SDL_DialogFileFilter filters[] = {
		{ fullDesc, fileExtension },
		{ "All files (*.*)", "*" }
	};

	SDL_ShowOpenFileDialog( sdlFileDialogCallback, (void*)callback, NULL, filters, ARRAY_SIZE( filters ), NULL, multiSelect );
	
	mem_Release( fullDesc );
}

// returns a stretchy buffer, be sure to release it
void editor_chooseSaveFileLocation( const char* fileTypeDesc, const char* fileExtension, void ( *callback )( const char* ) )
{
	char* fullDescTemplate = "%s (*.%s)";
	size_t totalLen = sizeof( char ) * ( SDL_strlen( fileTypeDesc ) + SDL_strlen( fileExtension ) + 6 );
	char* fullDesc = mem_Allocate( sizeof( char ) * totalLen );
	SDL_snprintf( fullDesc, totalLen, fullDescTemplate, fileTypeDesc, fileExtension );
	const SDL_DialogFileFilter filters[] = {
		{ fullDesc, fileExtension },
		{ "All files (*.*)", "*" }
	};

	SDL_ShowSaveFileDialog( sdlFileDialogCallback, (void*)callback, NULL, filters, ARRAY_SIZE( filters ), NULL );

	mem_Release( fullDesc );
}

static void getWorkingDirectory( void )
{
	if(sbRootDirectory == NULL) {
		wchar_t testDirectory[MAX_PATH] = { 0 };
		GetCurrentDirectory( MAX_PATH, testDirectory );

		sbRootDirectory = wideCharToUTF8SB( testDirectory );
		convertDirDividers( &sbRootDirectory );
	}
}
#else
// returns a stretchy buffer, be sure to release it
void editor_chooseLoadFileLocation( const char* fileTypeDesc, const char* fileExtension, bool multiSelect, void ( *callback )( const char* ) )
{
	ASSERT( false && "Not supported on this platform." );
}

// returns a stretchy buffer, be sure to release it
void editor_chooseSaveFileLocation( const char* fileTypeDesc, const char* fileExtension, void ( *callback )( const char* ) )
{
	ASSERT( false && "Not supported on this platform." );
}

static void getWorkingDirectory( void )
{
	ASSERT( false && "Not supported on this platform." );
}
#endif