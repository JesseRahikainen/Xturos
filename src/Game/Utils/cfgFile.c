#include "cfgFile.h"
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_log.h>
#include <string.h>

#include "Utils/stretchyBuffer.h"
#include "helpers.h"

#include "System/platformLog.h"

// TODO: make this better overall, this is just a quick hack to test some stuff
// TODO: include a way to have strings with spaces in them

#define READ_BUFFER_SIZE 512
#define FILE_PATH_LEN 256

#define ATTRIBUTE_NAME_LEN 64
#define VALUE_LEN 64

// we'll store the strings for saving and loading, then when getting the values attempt to parse them, the type won't be enforced since we're more concerned on human readability
typedef struct {
	char attrName[ATTRIBUTE_NAME_LEN];
	char value[VALUE_LEN];
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

	SDL_IOStream* ioStream = SDL_IOFromFile( fileName, "r" );
	if( ioStream == NULL ) {
		// file doesn't exist, just create a new empty configuration file to use
		return newFile;
	}

	// TODO: change this so everything is happening in place and there are no allocations
	// parse what this configuration file currently has in it
	char buffer[READ_BUFFER_SIZE];
	size_t numRead;
	char* fileText = NULL;
	//llog( LOG_INFO, "Stream size: %i", (int)SDL_RWsize( rwopsFile ) );
	while( ( numRead = SDL_ReadIO( ioStream, (void*)buffer, sizeof( char ) * sizeof( buffer ) ) ) != 0 ) {
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
			if( SDL_strlen( token ) > ARRAY_SIZE( attr.attrName ) ) {
				llog( LOG_ERROR, "Attribute name \"%s\" in config file %s is too long, truncating.", token, fileName );
			}
			SDL_strlcpy( attr.attrName, token, sizeof( attr.attrName ) - 1 );
			attr.attrName[sizeof( attr.attrName ) - 1] = 0;
			gettingAttrName = 0;
		} else {
			if( SDL_strlen( token ) > ARRAY_SIZE( attr.attrName ) ) {
				llog( LOG_ERROR, "Attribute value \"%s\" in config file %s is too long, truncating.", token, fileName );
			}
			SDL_strlcpy( attr.value, token, sizeof( attr.value ) - 1 );
			attr.value[sizeof( attr.value ) - 1] = 0;
			sb_Push( newFile->sbAttributes, attr );
			gettingAttrName = 1;
		}

		token = strtok( NULL, delimiters );
	}

	sb_Release( fileText );
	SDL_CloseIO( ioStream );

	return newFile;
}

// Saves out the file.
bool cfg_SaveFile( void* cfgFile )
{
	SDL_assert( cfgFile != NULL );
	bool success = true;

	CFGFile* file = (CFGFile*)cfgFile;
	char* outBuffer = NULL;
	SDL_IOStream* ioStream = NULL;

	int writeNewLine = 0;
	size_t count = sb_Count( file->sbAttributes );
	for( size_t i = 0; i < count; ++i ) {
		char* c = NULL;
		if( writeNewLine ) {
			c = sb_Add( outBuffer, 1 );
			(*c) = '\n';
		}
		writeNewLine = 1;

		// write out attribute name
		size_t attrLen = SDL_strlen( file->sbAttributes[i].attrName );
		c = sb_Add( outBuffer, attrLen );
		SDL_memcpy( c, file->sbAttributes[i].attrName, attrLen );

		// write out separator
		c = sb_Add( outBuffer, 3 );
		SDL_memcpy( c, " = ", 3 );

		// write out value
		size_t valueLen = SDL_strlen( file->sbAttributes[i].value );
		c = sb_Add( outBuffer, valueLen );
		SDL_memcpy( c, file->sbAttributes[i].value, valueLen );
	}

	ioStream = SDL_IOFromFile( file->filePath, "w" );
	if( !SDL_WriteIO( ioStream, outBuffer, sizeof( char ) * sb_Count( outBuffer ) ) ) {
		llog( LOG_ERROR, "Error writing out to file: %s", SDL_GetError( ) );
		success = false;
	}

	SDL_CloseIO( ioStream );
	sb_Release( outBuffer );
	return success;
}

// Closes the file and cleans up after it. Should always be called.
void cfg_CloseFile( void* cfgFile )
{
	SDL_assert( cfgFile != NULL );
	CFGFile* data = (CFGFile*)cfgFile;
	if( ( data == NULL ) || ( data->sbAttributes == NULL  ) ) {
		return;
	}

	sb_Release( data->sbAttributes );
	mem_Release( data );
}

// Returns the index of the attribute given the name. Returns -1 if it isn't found.
static int AttributeIndex( CFGFile* fileData, const char* attrName )
{
	int idx = -1;

	size_t count = sb_Count( fileData->sbAttributes );
	for( size_t i = 0; ( i < count ) && ( idx < 0 ); ++i ) {
		if( SDL_strncasecmp( attrName, fileData->sbAttributes[i].attrName, sizeof( fileData->sbAttributes[i].attrName ) - 1 ) == 0 ) {
			idx = (int)i;
		}
	}

	return idx;
}

// For getters pass them a valid config file pointer, the name of the attribute you want, and the default value for the
//  attribute if it can't find it.
// For setters pass a valid config file pointer, the name of the attribute, and the value you want to set. If the attribute
//  doesn't exist it will create a new one. Returns if the setting was successful.
static bool addNewAttribute( CFGFile* data, const char* attrName, const char* value )
{
	ASSERT_AND_IF_NOT( attrName != NULL ) return false;
	ASSERT_AND_IF_NOT( value != NULL ) return false;
	ASSERT_AND_IF_NOT( data != NULL ) return false;

	CFGAttribute newAttr;
	// verify that the name and value are valid
	size_t attrNameLen = SDL_strlen( attrName );
	if( ( attrNameLen + 1 ) > ARRAY_SIZE( newAttr.attrName ) ) {
		llog( LOG_ERROR, "Attribute name \"%s\" too long, truncating.", attrName );
	}

	if( attrNameLen <= 0 ) {
		llog( LOG_ERROR, "Attribute name empty. That is not allowed." );
		return false;
	}

	for( size_t i = 0; i < attrNameLen; ++i ) {
		if( SDL_isblank( attrName[i] ) ) {
			llog( LOG_ERROR, "Attribute name \"%s\" has a space. That is not allowed.", attrName );
			return false;
		}
	}

	size_t valueLen = SDL_strlen( value );
	if( ( valueLen + 1 ) > ARRAY_SIZE( newAttr.value ) ) {
		llog( LOG_ERROR, "Value \"%s\" too long, truncating.", value );
	}

	if( valueLen <= 0 ) {
		llog( LOG_ERROR, "Value empty. That is not allowed." );
		return false;
	}

	for( size_t i = 0; i < valueLen; ++i ) {
		if( SDL_isblank( value[i] ) ) {
			llog( LOG_ERROR, "Value \"%s\" has a space. That is not allowed.", value );
			return false;
		}
	}

	SDL_strlcpy( newAttr.attrName, attrName, ARRAY_SIZE( newAttr.attrName ) );
	SDL_strlcpy( newAttr.value, value, ARRAY_SIZE( newAttr.value ) );

	sb_Push( data->sbAttributes, newAttr );

	return true;
}

int cfg_GetInt( void* cfgFile, const char* attrName, int defaultVal )
{
	SDL_assert( cfgFile != NULL );

	CFGFile* data = (CFGFile*)cfgFile;
	int idx = AttributeIndex( data, attrName );
	if( idx >= 0 ) {
		return SDL_atoi( data->sbAttributes[idx].value );
	}
	return defaultVal;
}

bool cfg_SetInt( void* cfgFile, const char* attrName, int val )
{
	ASSERT_AND_IF_NOT( cfgFile != NULL ) return false;
	ASSERT_AND_IF_NOT( attrName != NULL ) return false;

	CFGFile* data = (CFGFile*)cfgFile;

	char valueStr[VALUE_LEN];
	SDL_snprintf( valueStr, ARRAY_SIZE( valueStr ), "%i", val );

	int idx = AttributeIndex( data, attrName );
	bool success = true;
	if( idx >= 0 ) {
		SDL_strlcpy( data->sbAttributes[idx].value, valueStr, ARRAY_SIZE( data->sbAttributes[idx].value ) );
	} else {
		success = addNewAttribute( data, attrName, valueStr );
	}

	return success;
}

float cfg_GetFloat( void* cfgFile, const char* attrName, float defaultVal )
{
	SDL_assert( cfgFile != NULL );

	CFGFile* data = (CFGFile*)cfgFile;
	int idx = AttributeIndex( data, attrName );
	if( idx >= 0 ) {
		return (float)SDL_atof( data->sbAttributes[idx].value );
	}
	return defaultVal;
}

bool cfg_SetFloat( void* cfgFile, const char* attrName, float val )
{
	ASSERT_AND_IF_NOT( cfgFile != NULL ) return false;
	ASSERT_AND_IF_NOT( attrName != NULL ) return false;

	CFGFile* data = (CFGFile*)cfgFile;

	char valueStr[VALUE_LEN];
	SDL_snprintf( valueStr, ARRAY_SIZE( valueStr ), "%f", val );

	int idx = AttributeIndex( data, attrName );
	bool success = true;
	if( idx >= 0 ) {
		SDL_strlcpy( data->sbAttributes[idx].value, valueStr, ARRAY_SIZE( data->sbAttributes[idx].value ) );
	} else {
		success = addNewAttribute( data, attrName, valueStr );
	}

	return success;
}
