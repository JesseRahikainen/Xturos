#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "edtaa3func.c"

#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )

#define IDX( x, y, w, p, e ) ( ( p ) + ( ( e ) * ( ( x ) + ( ( y ) * ( w ) ) ) ) )

// TODO:
//  - Better image handling, stb_image is good but we could use a more capable library for this
//  - Handle non-monochrome images
//  - More sizing options
//  - Single channel mask image to better handle transparent images

// returns if strToTest ends with strValue
bool endsWith( const char* strToTest, const char* strValue )
{
	size_t valLen = strlen( strValue );
	size_t testLen = strlen( strToTest );

	if( valLen > testLen ) {
		return false;
	}

	for( size_t offset = 0; offset <= valLen; ++offset ) {
		if( strToTest[testLen - offset] != strValue[valLen - offset] ) {
			return false;
		}
	}

	return true;
}

int main( int argc, char** argv )
{
	// check to see if the output path is a .png file, if it isn't then append .png to the end
	const char* png = ".png";

	char* inputFilePath = NULL;
	char* outputFilePath = NULL;
	int ret = 0;
	int addX = 0;
	int addY = 0;

	unsigned char* loadedImage = NULL;
	double* dblImg = NULL;
	double* outside = NULL;
	double* inside = NULL;
	double* xGradients = NULL;
	double* yGradients = NULL;
	short* xDists = NULL;
	short* yDists = NULL;
	unsigned char* expandedImage = NULL;

	for( int i = 1; i < argc; ++i ) {
		if( strcmp( "-h", argv[i] ) == 0 ) {
			fprintf( stdout, "Used to generate a monochrome signed distance field from an image.\n" );
			fprintf( stdout, "Will create a four channel PNG with the alpha set to the distance, where 128 is on the edge.\n" );
			fprintf( stdout, "Useage: SDFImageGenerator -xa x -ya y source_file output_file\n" );
		} else if( strcmp( "-xa", argv[i] ) == 0 ) {
			// add to x size
			++i;
			if( i >= argc ) {
				fprintf( stderr, "No parameter after -xa." );
				ret = 3;
				goto clean_up;
			} else {
				addX = strtol( argv[i], NULL, 10 );
			}
		} else if( strcmp( "-ya", argv[i] ) == 0 ) {
			// add to y size
			++i;
			if( i >= argc ) {
				fprintf( stderr, "No parameter after -ya." );
				ret = 4;
				goto clean_up;
			} else {
				addY = strtol( argv[i], NULL, 10 );
			}
		} else {
			// one of the files to use
			if( inputFilePath == NULL ) {
				inputFilePath = argv[i];
			} else if( outputFilePath == NULL ) {
				outputFilePath = malloc( strlen( argv[i] ) + 1 + 4 );
				if( outputFilePath == NULL ) {
					fprintf( stderr, "Unable to allocate data for output file string name." );
					ret = 2;
					goto clean_up;
				}
				strcpy( outputFilePath, argv[i] );

				if( !endsWith( outputFilePath, png ) ) {
					strcat( outputFilePath, png );
				}
			} else {
				fprintf( stderr, "Too many files supplied." );
			}
		}
	}

	// load the image
	int w;
	int h;
	int comp;
	loadedImage = stbi_load( inputFilePath, &w, &h, &comp, 4 );
	if( loadedImage == NULL ) {
		fprintf( stderr, "Unable to load image file: %s\n", stbi_failure_reason( ) );
		ret = 3;
		goto clean_up;
	}

	// expand the image by the desired amount
	int ew = w + ( 2 * addX );
	int eh = h + ( 2 * addY );
	expandedImage = malloc( sizeof( unsigned char ) * 4 * ew * eh );
	memset( expandedImage, 0, sizeof( unsigned char ) * 4 * ew * eh );

	for( int y = 0; y < h; ++y ) {
		for( int x = 0; x < w; ++x ) {
			for( int e = 0; e < 4; ++e ) {
				expandedImage[IDX( x + addX, y + addY, ew, e, 4 )] = loadedImage[IDX( x, y, w, e, 4 )];
			}
		}
	}

	// unload the image
	stbi_image_free( loadedImage );
	loadedImage = NULL;

	// create the sdf
	//  first make it the image doubles
	dblImg = malloc( sizeof( double ) * ew * eh );
	outside = malloc( sizeof( double ) * ew * eh );
	inside = malloc( sizeof( double ) * ew * eh );
	xGradients = malloc( sizeof( double ) * ew * eh );
	yGradients = malloc( sizeof( double ) * ew * eh );
	xDists = malloc( sizeof( short ) * ew * eh );
	yDists = malloc( sizeof( short ) * ew * eh );

	//  map it into the range [0, 1]
	for( int i = 0; i < ( ew * eh ); ++i ) {
		dblImg[i] = expandedImage[( i * 4 ) + 3] / 255.0;
	}

	//  compute outside
	computegradient( dblImg, ew, eh, xGradients, yGradients );
	edtaa3( dblImg, xGradients, yGradients, ew, eh, xDists, yDists, outside );
	for( int i = 0; i < ( ew * eh ); ++i ) {
		outside[i] = MAX( outside[i], 0.0 );
	}

	//  compute inside
	for( int i = 0; i < ( ew * eh ); ++i ) {
		dblImg[i] = 1.0 - dblImg[i];
	}
	computegradient( dblImg, ew, eh, xGradients, yGradients );
	edtaa3( dblImg, xGradients, yGradients, ew, eh, xDists, yDists, inside );
	for( int i = 0; i < ( ew * eh ); ++i ) {
		inside[i] = MAX( inside[i], 0.0 );
	}

	//  merge inside and outside and convert to an image, we'll resuse the loadedImage data
	for( int i = 0; i < ( ew * eh ); ++i ) {
		int imgIdx = i * 4;
		expandedImage[imgIdx + 0] = 255;
		expandedImage[imgIdx + 1] = 255;
		expandedImage[imgIdx + 2] = 255;

		outside[i] -= inside[i];
		outside[i] = 128 + ( outside[i] * 16 );
		outside[i] = MAX( outside[i], 0.0 );
		outside[i] = MIN( outside[i], 255.0 );

		expandedImage[imgIdx + 3] = 255 - (unsigned char)outside[i];
	}

	// save out image
	if( stbi_write_png( outputFilePath, ew, eh, 4, expandedImage, 0 ) == 0 ) {
		fprintf( stderr, "Unable to save image file.\n" );
		ret = 4;
		goto clean_up;
	}

	fprintf( stdout, "SDF image created.\n" );

clean_up:

	free( dblImg );
	free( outside );
	free( inside );
	free( xGradients );
	free( yGradients );
	free( xDists );
	free( yDists );
	free( outputFilePath );
	free( expandedImage );
	stbi_image_free( loadedImage );

	return ret;
}