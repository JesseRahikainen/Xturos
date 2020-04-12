#ifndef CFG_PARSER_H
#define CFG_PARSER_H

// quick and dirty cfg file parser
//  simple format: 
//   attrName = value
//  each on it's own line, ignores white space

// opens the file, returns NULL if it fails.
void* cfg_OpenFile( const char* fileName );

// Closes the file and cleans up after it. Should always be called.
void cfg_CloseFile( void* cfgFile );

// Saves out the file.
int cfg_SaveFile( void* cfgFile );

// Accessors. Pass them a valid config file pointer, the name of the attribute you want, the default value for the
//  attribute if it can't find it, and a spot to put the value if it does. Returns 0 if it finds the value, a
//  negative number if it doesn't.
int cfg_GetInt( void* cfgFile, const char* attrName, int defaultVal, int* retVal );

// If the attribute already exists it sets the associated value, if it doesn't exist it is added.
void cfg_SetInt( void* cfgFile, const char* attrName, int val );

#endif // inclusion guard