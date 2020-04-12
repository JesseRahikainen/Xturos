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

// TODO:
//  - Better image handling, stb_image is good but we could use a more capable library for this
//  - Handle non-monochrome images
//  - Sizing options
//  - Single channel mask image to better handle transparent images
//  - Adding padding to the output image
//  - Different methods for generating signed distance fields

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
	// we'll be expecting an input file name and an output file name
	if( argc <= 1 ) {
		fprintf( stderr, "Insufficient number of arguments, use -h to get help.\n" );
		return 1;
	}

	if( argc == 2 ) {
		if( strcmp( "-h", argv[1] ) == 0 ) {
			fprintf( stdout, "Used to generate a monochrome signed distance field from an image.\n" );
			fprintf( stdout, "Will create a four channel PNG with the alpha set to the distance, where 128 is on the edge.\n" );
			fprintf( stdout, "Useage: SDFImageGenerator source_file output_file\n" );
		} else {
			fprintf( stderr, "Invalid arguments, use -h to get help.\n" );
		}

		return 1;
	}

	if( argc >= 4 ) {
		fprintf( stderr, "Too many arguments, use -h to get help.\n" );
		return 1;
	}

	int ret = 0;

	char* outputFilePath = NULL;
	unsigned char* loadedImage = NULL;
	double* dblImg = NULL;
	double* outside = NULL;
	double* inside = NULL;
	double* xGradients = NULL;
	double* yGradients = NULL;
	short* xDists = NULL;
	short* yDists = NULL;


	const char* inputFilePath = argv[1];

	// we'll want to copy the output file in case we need to add .png to the end
	size_t outPathLen = strlen( argv[2] ) + 4 + 1;
	outputFilePath = malloc( outPathLen );
	if( outputFilePath == NULL ) {
		fprintf( stderr, "Unable to allocate data for output file string name." );
		ret = 2;
		goto clean_up;
	}
	strcpy( outputFilePath, argv[2] );

	// check to see if the output path is a .png file, if it isn't then append .png to the end
	const char* png = ".png";

	if( !endsWith( outputFilePath, png ) ) {
		strcat( outputFilePath, png );
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

	// create the sdf
	//  first make it the image doubles
	dblImg = malloc( sizeof( double ) * w * h );
	outside = malloc( sizeof( double ) * w * h );
	inside = malloc( sizeof( double ) * w * h );
	xGradients = malloc( sizeof( double ) * w * h );
	yGradients = malloc( sizeof( double ) * w * h );
	xDists = malloc( sizeof( short ) * w * h );
	yDists = malloc( sizeof( short ) * w * h );

	//  map it into the range [0, 1]
	for( int i = 0; i < ( w * h ); ++i ) {
		dblImg[i] = loadedImage[( i * 4 ) + 3] / 255.0;
	}

	//  compute outside
	computegradient( dblImg, w, h, xGradients, yGradients );
	edtaa3( dblImg, xGradients, yGradients, w, h, xDists, yDists, outside );
	for( int i = 0; i < ( w * h ); ++i ) {
		outside[i] = MAX( outside[i], 0.0 );
	}

	//  compute inside
	for( int i = 0; i < ( w * h ); ++i ) {
		dblImg[i] = 1.0 - dblImg[i];
	}
	computegradient( dblImg, w, h, xGradients, yGradients );
	edtaa3( dblImg, xGradients, yGradients, w, h, xDists, yDists, inside );
	for( int i = 0; i < ( w * h ); ++i ) {
		inside[i] = MAX( inside[i], 0.0 );
	}

	//  merge inside and outside and convert to an image, we'll resuse the loadedImage data
	for( int i = 0; i < ( w * h ); ++i ) {
		int imgIdx = i * 4;
		loadedImage[imgIdx + 0] = 255;
		loadedImage[imgIdx + 1] = 255;
		loadedImage[imgIdx + 2] = 255;

		outside[i] -= inside[i];
		outside[i] = 128 + ( outside[i] * 16 );
		outside[i] = MAX( outside[i], 0.0 );
		outside[i] = MIN( outside[i], 255.0 );

		loadedImage[imgIdx + 3] = 255 - (unsigned char)outside[i];
	}

	// save out image
	if( stbi_write_png( outputFilePath, w, h, 4, loadedImage, 0 ) == 0 ) {
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
	stbi_image_free( loadedImage );

	return ret;
}