#include "editorHelpers.h"

#include <SDL.h>

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
#include "Graphics/images.h"
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
	return NULL;
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
	char* filePathToken = SDL_strtokr( *sbFilePath, tokens, &filePathTokSave );

	char* rootPathTokSave = NULL;
	char* rootPathToken = SDL_strtokr( sbRoot, tokens, &rootPathTokSave );

	// eat through the tokens until we get something that doesn't match
	bool tokensMatch = ( strcmp( filePathToken, rootPathToken ) == 0 );
	while(( filePathToken != NULL ) && ( rootPathToken != NULL ) && tokensMatch) {
		filePathToken = SDL_strtokr( NULL, tokens, &filePathTokSave );
		rootPathToken = SDL_strtokr( NULL, tokens, &rootPathTokSave );

		if(( filePathToken != NULL ) && ( rootPathToken != NULL )) {
			tokensMatch = ( strcmp( filePathToken, rootPathToken ) == 0 );
		}
	}

	char* sbLocalFilePath = NULL;

	// get all the directories from the path that we didn't hit in the root path and add ../ to the starfor them
	while( rootPathToken != NULL ) {
		rootPathToken = SDL_strtokr( NULL, tokens, &rootPathTokSave );
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

		filePathToken = SDL_strtokr( NULL, tokens, &filePathTokSave );
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
int* editor_loadSpriteSheetFile( const char* filePath )
{
	SDL_assert( filePath != NULL );
	llog( LOG_DEBUG, "Loading sprite sheet file %s...", filePath );

	int* sbImgs = NULL;
	if(img_LoadSpriteSheet( filePath, ST_DEFAULT, &sbImgs ) < 0) {
		hub_CreateDialog( "Error", "Unable to load sprite sheet. Check log file for details.", DT_ERROR, 1, "OK", NULL );
	}
	return sbImgs;
}

#ifdef WIN32
static void handleDialogError( DWORD errorCode )
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
}

typedef enum {
	BROWSER_OPEN,
	BROWSER_SAVE
} BrowserType;

typedef enum {
	BRT_FALSE,
	BRT_TRUE,
	BRT_INVALID
} BrowserReturnType;

static void fileBrowser( const char* fileTypeDesc, const char* fileExtension, BrowserType type, bool multiSelect, void (*callback)(const char*) )
{
	char filter[128];
	int result = SDL_snprintf( filter, ARRAY_SIZE( filter ), "All (*.*)\n*.*\n%s (*.%s)\n*.%s\n", fileTypeDesc, fileExtension, fileExtension );
	if(result < 0)
	{
		llog( LOG_ERROR, "Error building filter." );
		return;
	}
	if(result >= ARRAY_SIZE( filter )) {
		llog( LOG_ERROR, "Unable to build filter, increase array size." );
		return;
	}
	wchar_t wideFilter[ARRAY_SIZE( filter )];
	int toWideResult = (int)mbstowcs( wideFilter, filter, ARRAY_SIZE( filter ) );
	if(result < 0) {
		llog( LOG_ERROR, "Error converting filter." );
		return;
	}
	// we use \n as placeholders for the \0 needed so the string operations work correctly
	for(size_t i = 0; i < (size_t)toWideResult; ++i) {
		if(wideFilter[i] == L'\n') wideFilter[i] = L'\0';
	}

	#define FILE_PATH_BUFFER_LEN 512
	wchar_t* fileName = mem_Allocate( sizeof( wchar_t ) * FILE_PATH_BUFFER_LEN );

	OPENFILENAME ofn;
	ZeroMemory( &ofn, sizeof( ofn ) );
	ofn.lStructSize = sizeof( ofn );
	ofn.hwndOwner = NULL;
	ofn.lpstrFile = fileName;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = FILE_PATH_BUFFER_LEN;
	ofn.lpstrFilter = wideFilter;
	ofn.nFilterIndex = 2;
	ofn.lpstrCustomFilter = NULL;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	if( multiSelect ) ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;

	BrowserReturnType retVal = BRT_INVALID;

	switch(type) {
	case BROWSER_OPEN:
		retVal = BRT_FALSE;
		if(GetOpenFileName( &ofn )) retVal = BRT_TRUE;
		break;
	case BROWSER_SAVE:
		retVal = BRT_FALSE;
		if(GetSaveFileName( &ofn )) retVal = BRT_TRUE;
		break;
	}

	if(retVal == BRT_TRUE) {
		// if multiple are returned then ofn.nFileOffset points to the first file name in ofn.lpstrFile, and ofn.lpstr
		//  the beginning of ofn.lpstrFile is the path, then all the files separated by /0, with the last one ending with /0/0 (yay for fucking with string functions)
		// if the character before the file is NULL then we have gotten multiple files
		if( ofn.lpstrFile[ofn.nFileOffset - 1] == 0 ) {
			// multiple files selected

			// grab the directory path and create the string for it
			wchar_t* dirPath = ofn.lpstrFile;
			size_t dirPathLen = SDL_wcslen( dirPath );

			wchar_t* selectedFileName = &ofn.lpstrFile[ofn.nFileOffset];
			while( SDL_wcslen( selectedFileName ) > 0 ) {
				size_t fileNameLen = SDL_wcslen( selectedFileName );

				size_t wholeFilePathLen = dirPathLen + fileNameLen + 2; // includes divider and null
				wchar_t* wholeFilePath = mem_Allocate( sizeof( wchar_t ) * ( wholeFilePathLen ) );
				SDL_wcslcpy( wholeFilePath, dirPath, wholeFilePathLen );
				SDL_wcslcat( wholeFilePath, L"\\", wholeFilePathLen );
				SDL_wcslcat( wholeFilePath, selectedFileName, wholeFilePathLen );

				// how do we handle all the file names?
				char* utf8Filepath = wideCharToUTF8SB( wholeFilePath );
				toLocalPath( &utf8Filepath );
				if( callback != NULL ) callback( utf8Filepath );
				sb_Release( utf8Filepath );

				mem_Release( wholeFilePath );

				// get the next file name
				selectedFileName = &selectedFileName[SDL_wcslen( selectedFileName ) + 1];
			}
		} else {
			// one file selected
			char* utf8Str = wideCharToUTF8SB( ofn.lpstrFile );
			toLocalPath( &utf8Str );
			if( callback != NULL ) callback( utf8Str );
			sb_Release( utf8Str );
		}
	} else if( retVal == BRT_FALSE ) {
		DWORD error = CommDlgExtendedError( );
		handleDialogError( error );
	} else { // BRT_INVALID
		hub_CreateDialog( "Error", "Invalid file browser type.", DT_ERROR, 1, "OK", NULL );
	}

	mem_Release( fileName );
}

// returns a stretchy buffer, be sure to release it
void editor_chooseLoadFileLocation( const char* fileTypeDesc, const char* fileExtension, bool multiSelect, void ( *callback )( const char* ) )
{
	fileBrowser( fileTypeDesc, fileExtension, BROWSER_OPEN, multiSelect, callback );
}

// returns a stretchy buffer, be sure to release it
void editor_chooseSaveFileLocation( const char* fileTypeDesc, const char* fileExtension, void ( *callback )( const char* ) )
{
	fileBrowser( fileTypeDesc, fileExtension, BROWSER_SAVE, false, callback );
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
char* editor_chooseLoadFileLocation( char* )
{
	SDL_assert( false && "Not supported on this platform." );
	return NULL;
}

// returns a stretchy buffer, be sure to release it
char* editor_chooseSaveFileLocation( void )
{
	SDL_assert( false && "Not supported on this platform." );
	return NULL;
}

static char* getWorkingDirectory( void )
{
	SDL_assert( false && "Not supported on this platform." );
	return NULL;
}
#endif