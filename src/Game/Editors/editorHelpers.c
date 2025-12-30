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
	SDL_assert( false && "Not supported on this platform." );
}
#endif

static char* sbRootDirectory = NULL;
static void getWorkingDirectory( void );

// convert a full path to a local path
static void toLocalPath( char** sbFilePath )
{
	getWorkingDirectory( );

	SDL_assert( sbRootDirectory != NULL );
	SDL_assert( sbFilePath != NULL );

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
	SDL_assert( filePath != NULL );
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
	SDL_assert( filePath != NULL );
	llog( LOG_DEBUG, "Loading sprite sheet file %s...", filePath );

	int packageID = -1;
	if( ( packageID = img_LoadSpriteSheet( filePath, ST_DEFAULT, outImgSB ) ) < 0) {
		hub_CreateDialog( "Error", "Unable to load sprite sheet. Check log file for details.", DT_ERROR, 1, "OK", NULL );
	}
	return packageID;
}

#ifdef WIN32
/*static void handleDialogError( DWORD errorCode )
{
	if(errorCode == 0) {
		// cancelled out, no error to show
		return;
	}

	// from here: https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-commdlgextendederror
	const char* errorText = "Unknown error.";
	switch(errorCode) {
		// general dialog errors
	case CDERR_DIALOGFAILURE:
		errorText = "Common dialog box call failed.";
		break;
	case CDERR_FINDRESFAILURE:
		errorText = "Unable to find a specified resource.";
		break;
	case CDERR_INITIALIZATION:
		errorText = "Failed during initialization.";
		break;
	case CDERR_LOADRESFAILURE:
		errorText = "Failed to load a specified resource.";
		break;
	case CDERR_LOADSTRFAILURE:
		errorText = "Failed to load a specified string.";
		break;
	case CDERR_LOCKRESFAILURE:
		errorText = "Failed to lock a specified resource.";
		break;
	case CDERR_MEMALLOCFAILURE:
		errorText = "Unable to allocate memory for internal structures.";
		break;
	case CDERR_MEMLOCKFAILURE:
		errorText = "Unable to lock the memory associated with a handle.";
		break;
	case CDERR_NOHINSTANCE:
		errorText = "ENABLETEMPLATE flag was set but no corresponding instance handle was provided.";
		break;
	case CDERR_NOHOOK:
		errorText = "ENABLEHOOK flag was set but no pointer to a corresponding hook procedure.";
		break;
	case CDERR_NOTEMPLATE:
		errorText = "ENABLETEMPALTE flag was set but no corresponding template was provided.";
		break;
	case CDERR_REGISTERMSGFAIL:
		errorText = "RegisterWindowMessage returned an error when it was called.";
		break;
	case CDERR_STRUCTSIZE:
		errorText = "lStructSize was invalid.";
		break;
		// load and save dialog specific errors
	case FNERR_BUFFERTOOSMALL:
		// buffer was too small, could increase the size if needed here, first two bytes of the file string will have the needed size
		errorText = "String buffer was too short for file name.";
		break;
	case FNERR_INVALIDFILENAME:
		errorText = "File name invalid.";
		break;
	case FNERR_SUBCLASSFAILURE:
		errorText = "Subclass failure, insufficient memory.";
		break;
	}

	hub_CreateDialog( "Error", errorText, DT_ERROR, 1, "OK", NULL );
}//*/

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
	SDL_assert( false && "Not supported on this platform." );
}

// returns a stretchy buffer, be sure to release it
void editor_chooseSaveFileLocation( const char* fileTypeDesc, const char* fileExtension, void ( *callback )( const char* ) )
{
	SDL_assert( false && "Not supported on this platform." );
}

static void getWorkingDirectory( void )
{
	SDL_assert( false && "Not supported on this platform." );
}
#endif