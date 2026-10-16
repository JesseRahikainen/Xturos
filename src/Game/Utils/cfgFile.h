#ifndef CFG_PARSER_H
#define CFG_PARSER_H

#include <stdbool.h>

// quick and dirty cfg file parser
//  simple format: 
//   attrName = value
//  each on it's own line, ignores white space

// opens the file, returns NULL if it fails.
void* cfg_OpenFile( const char* fileName );

// closes the file and cleans up after it. Should always be called.
void cfg_CloseFile( void* cfgFile );

// saves out the file.
bool cfg_SaveFile( void* cfgFile );

// for getters pass them a valid config file pointer, the name of the attribute you want, and the default value for the
//  attribute if it can't find it.
// for setters pass a valid config file pointer, the name of the attribute, and the value you want to set. If the attribute
//  doesn't exist it will create a new one. Returns if the setting was successful.
// cache the values you get if you're going to be using them a lot

int cfg_GetInt( void* cfgFile, const char* attrName, int defaultVal );
bool cfg_SetInt( void* cfgFile, const char* attrName, int val );

float cfg_GetFloat( void* cfgFile, const char* attrName, float defaultVal );
bool cfg_SetFloat( void* cfgFile, const char* attrName, float val );

bool cfg_GetBool( void* cfgFile, const char* attrName, bool defaultVal );
bool cfg_SetBool( void* cfgFile, const char* attrName, bool val );

#endif // inclusion guard