#include "cfgFile.h"
#include <SDL_rwops.h>
#include <SDL_log.h>
#include <string.h>

#include "../Utils/stretchyBuffer.h"
#include "helpers.h"

#include "../System/platformLog.h"

// TODO: make this better overall, this is just a quick hack to test some stuff

#define READ_BUFFER_SIZE 512
#define FILE_PATH_LEN 128

typedef struct {
	char fileName[64];
	int value;
} CFGAttribute;

typedef struct {
	char filePath[FILE_PATH_LEN];
	CFGAttribute* sbAttributes;
} CFGFile;

// opens the file, returns NULL if it fails.
void* cfg_OpenFile( const char* fileName )
{
	if( SDL_strlen( fileName ) >= ( FILE_PATH_LEN - 1 ) ) {
		llog( LOG_ERROR, "Configuration file path too long" );
		return NULL;
	}

	CFGFile* newFile = (CFGFile*)mem_Allocate( sizeof( CFGFile ) );
	if( newFile == NULL ) {
		llog( LOG_INFO, "Unable to open configuration file." );
		return NULL;
	}
	newFile->sbAttributes = NULL;
	SDL_strlcpy( newFile->filePath, fileName, FILE_PATH_LEN - 1 );
	newFile->filePath[FILE_PATH_LEN-1] = 0;

	SDL_RWops* rwopsFile = SDL_RWFromFile( fileName, "r" );
	if( rwopsFile == NULL ) {
		// file doesn't exist, just create a new empty configuration file to use
		return newFile;
	}

	// TODO: change this so everything is happening in place and there are no allocations
	// parse what this configuration file currently has in it
	char buffer[READ_BUFFER_SIZE];
	size_t numRead;
	char* fileText = NULL;
	llog( LOG_INFO, "Stream size: %i", (int)SDL_RWsize( rwopsFile ) );
	while( ( numRead = SDL_RWread( rwopsFile, (void*)buffer, sizeof( char ), sizeof( buffer ) ) ) != 0 ) {
		char* c = sb_Add( fileText, (int)numRead );
		for( size_t i = 0; i < numRead; ++i ) {
			*c++ = buffer[i];
		}
	}
	sb_Push( fileText, 0 ); // make this c-string compatible

	// got the entire file text, now tokenize and parse
	//  only tokens we're worried about are '=' and '/r/n'
	//  everything before the '=' is the attribute name, everything
	//  after is the attribute value, all white space should be cut
	//  off of each end
	int gettingAttrName = 1;
	const char* delimiters = "\f\v\t =\r\n";
	char* token = strtok( fileText, delimiters );
	CFGAttribute attr;

	while( token != NULL ) {

		// cut off white space, don't care about preserving memory
		if( gettingAttrName ) {
			SDL_strlcpy( attr.fileName, token, sizeof( attr.fileName ) - 1 );
			attr.fileName[sizeof( attr.fileName ) - 1] = 0;
			gettingAttrName = 0;
		} else {
			attr.value = SDL_atoi( token );
			sb_Push( newFile->sbAttributes, attr );
			llog( LOG_INFO, "New attribute: %s  %i", attr.fileName, attr.value );
			gettingAttrName = 1;
		}

		token = strtok( NULL, delimiters );
	}

	sb_Release( fileText );
	SDL_RWclose( rwopsFile );

	return newFile;
}

// Saves out the file.
int cfg_SaveFile( void* cfgFile )
{
	assert( cfgFile != NULL );
	int success = 0;

	CFGFile* file = (CFGFile*)cfgFile;
	char* outBuffer = NULL;
	SDL_RWops* rwopsFile = NULL;

	char strVal[32];

	int writeNewLine = 0;
	int count = sb_Count( file->sbAttributes );
	for( int i = 0; i < count; ++i ) {
		char* c = NULL;
		if( writeNewLine ) {
			c = sb_Add( outBuffer, 1 );
			(*c) = '\n';
		}
		writeNewLine = 1;

		// write out attribute name
		size_t attrLen = SDL_strlen( file->sbAttributes[i].fileName );
		c = sb_Add( outBuffer, attrLen );
		SDL_memcpy( c, file->sbAttributes[i].fileName, attrLen );

		// write out separator
		c = sb_Add( outBuffer, 3 );
		SDL_memcpy( c, " = ", 3 );

		// write out value
		int pl = SDL_snprintf( strVal, sizeof( strVal ), "%i", file->sbAttributes[i].value );
		if( pl >= ARRAY_SIZE( strVal ) ) {
			llog( LOG_ERROR, "Problem writing out configuration file value, value to long. File: %s   Name: %s", file->filePath, file->sbAttributes[i].fileName );
			goto clean_up;
		} else {
			c = sb_Add( outBuffer, pl );
			SDL_memcpy( c, strVal, pl );
		}
	}

	rwopsFile = SDL_RWFromFile( file->filePath, "w" );
	SDL_RWwrite( rwopsFile, outBuffer, sizeof( char ), sb_Count( outBuffer ) );

clean_up:
	SDL_RWclose( rwopsFile );
	sb_Release( outBuffer );
	return success;
}

// Closes the file and cleans up after it. Should always be called.
void cfg_CloseFile( void* cfgFile )
{
	assert( cfgFile != NULL );
	CFGFile* data = (CFGFile*)cfgFile;
	if( ( data == NULL ) || ( data->sbAttributes == NULL  ) ) {
		return;
	}

	sb_Release( data->sbAttributes );
	mem_Release( data );
}

// Returns the index of the attribute given the name. Returns -1 if it isn't found.
int AttributeIndex( CFGFile* fileData, const char* attrName )
{
	int idx = -1;

	size_t count = sb_Count( fileData->sbAttributes );
	for( size_t i = 0; ( i < count ) && ( idx < 0 ); ++i ) {
		if( SDL_strncasecmp( attrName, fileData->sbAttributes[i].fileName, sizeof( fileData->sbAttributes[i].fileName ) - 1 ) == 0 ) {
			idx = (int)i;
		}
	}

	return idx;
}

// Accessors. Pass them a valid config file pointer, the name of the attribute you want, the default value for the
//  attribute if it can't find it, and a spot to put the value if it does. Returns 0 if it finds the value, a
//  negative number if it doesn't.
int cfg_GetInt( void* cfgFile, const char* attrName, int defaultVal, int* retVal )
{
	assert( cfgFile != NULL );

	CFGFile* data = (CFGFile*)cfgFile;

	int result = defaultVal;
	int ret = -1;

	int idx = AttributeIndex( data, attrName );
	if( idx >= 0 ) {
		result = data->sbAttributes[idx].value;
		ret = 0;
	}
	(*retVal) = result;

	return ret;
}

// If the attribute already exists it sets the associated value, if it doesn't exist it is added.
void cfg_SetInt( void* cfgFile, const char* attrName, int val )
{
	assert( cfgFile != NULL );

	CFGFile* data = (CFGFile*)cfgFile;

	int idx = AttributeIndex( data, attrName );
	if( idx >= 0 ) {
		data->sbAttributes[idx].value = val;
	} else {
		CFGAttribute newAttr;

		SDL_strlcpy( newAttr.fileName, attrName, sizeof( newAttr.fileName ) - 1 );
		newAttr.fileName[sizeof( newAttr.fileName ) - 1] = 0;
		newAttr.value = val;

		sb_Push( data->sbAttributes, newAttr );
	}
}