#include "CfgParser.h"
#include <SDL_rwops.h>
#include <stretchy_buffer.h>
#include <string.h>

// TODO: make this better overall, this is just a quick hack to test some stuff

#define READ_BUFFER_SIZE 512

typedef struct {
	char name[64];
	int value;
} CFGAttribute;

typedef struct {
	CFGAttribute* attributes;
} CFGFile;

// opens the file, returns NULL if it fails.
void* cfg_OpenFile( const char* fileName )
{
	CFGFile* newFile = (CFGFile*)SDL_malloc( sizeof( CFGFile ) );
	if( newFile == NULL ) {
		return NULL;
	}
	newFile->attributes = NULL;

	SDL_RWops* rwopsFile = SDL_RWFromFile( fileName, "r" );
	if( rwopsFile == NULL ) {
		return NULL;
	}

	// TODO: change this so everything is happening in place and there are no allocations
	// parse what this configuration file currently has in it
	char buffer[READ_BUFFER_SIZE];
	size_t numRead;
	char* fileText = NULL;
	while( ( numRead = SDL_RWread( rwopsFile, (void*)buffer, sizeof( char ), sizeof( buffer ) ) ) != 0 ) {
		char* c = sb_add( fileText, (int)numRead );
		for( size_t i = 0; i < numRead; ++i ) {
			*c++ = buffer[i];
		}
	}
	sb_push( fileText, 0 ); // make this c-string compatible

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
			SDL_strlcpy( attr.name, token, sizeof( attr.name ) - 1 );
			attr.name[sizeof( attr.name ) - 1] = 0;
			gettingAttrName = 0;
		} else {
			attr.value = SDL_atoi( token );
			sb_push( newFile->attributes, attr );
			gettingAttrName = 1;
		}

		token = strtok( NULL, delimiters );
	}

	sb_free( fileText );
	SDL_RWclose( rwopsFile );

	return newFile;
}

// Closes the file and cleans up after it. Should always be called.
void cfg_CloseFile( void* cfgFile )
{
	CFGFile* data = (CFGFile*)cfgFile;
	if( ( data == NULL ) || ( data->attributes == NULL  ) ) {
		return;
	}

	sb_free( data->attributes );
	SDL_free( data );
}

// Accessors. Pass them a valid config file pointer, the name of the attribute you want, the default value for the
//  attribute if it can't find it, and a spot to put the value if it does. Returns 0 if it finds the value, a
//  negative number if it doesn't.
int cfg_GetInt( void* cfgFile, const char* attrName, int defaultVal, int* retVal )
{
	CFGFile* data = (CFGFile*)cfgFile;
	if( ( data == NULL ) || ( data->attributes == NULL  ) ) {
		(*retVal) = defaultVal;
		return -1;
	}

	int size = sb_count( data->attributes );
	int result = defaultVal;
	int i = 0;
	while( ( i < size ) && ( SDL_strncasecmp( attrName, data->attributes[i].name, sizeof( data->attributes[i].name ) - 1 ) != 0 ) ) {
		++i;
	}

	if( i < size ) {
		result = data->attributes[i].value;
	}
	(*retVal) = result;

	return 0;
}