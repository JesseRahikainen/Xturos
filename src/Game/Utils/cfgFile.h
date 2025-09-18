#ifndef CFG_PARSER_H
#define CFG_PARSER_H

#include <stdbool.h>

// quick and dirty cfg file parser
//  simple format: 
//   attrName = value
//  each on it's own line, ignores white space

// opens the file, returns NULL if it fails.
void* cfg_OpenFile( const char* fileName );

// Closes the file and cleans up after it. Should always be called.
void cfg_CloseFile( void* cfgFile );

// Saves out the file.
bool cfg_SaveFile( void* cfgFile );

// For getters pass them a valid config file pointer, the name of the attribute you want, and the default value for the
//  attribute if it can't find it.
// For setters pass a valid config file pointer, the name of the attribute, and the value you want to set. If the attribute
//  doesn't exist it will create a new one. Returns if the setting was successful.

int cfg_GetInt( void* cfgFile, const char* attrName, int defaultVal );
bool cfg_SetInt( void* cfgFile, const char* attrName, int val );

float cfg_GetFloat( void* cfgFile, const char* attrName, float defaultVal );
bool cfg_SetFloat( void* cfgFile, const char* attrName, float val );

#endif // inclusion guard