#include "spineGfx.h"

#include <SDL.h>
#include <assert.h>
#include <spine/extension.h>

#include "triRendering.h"
#include "debugRendering.h"
#include "gfxUtil.h"
#include "../System/memory.h"

// templates
typedef struct {
	spSkeletonData* skeletonData;
	spAtlas* atlas;
	spAnimationStateData* stateData;
} SpineTemplate;

#define MAX_TEMPLATES 256
static SpineTemplate templates[MAX_TEMPLATES];

// instances
typedef struct {
	int templateIdx;
	int cameraFlags;
	Vector2 startPos;
	Vector2 endPos;
	spSkeleton* skeleton;
	spAnimationState* state;
	char depth;
} SpineInstance;

#define MAX_INSTANCES 2048
static SpineInstance instances[MAX_INSTANCES];
static int lastInstance = -1;

// working memory
#define MAX_SPINE_VERTS 1000
static float spineVertices[MAX_SPINE_VERTS];

// helper functions
// these have to be not static
void _spAtlasPage_createTexture( spAtlasPage* self, const char* path )
{
	// TODO: Get the memory allocation working and set up here, or is there some way to do this without allocation?
	//   Do we really want to use the whole Texture struct? we only need the texture object and whether it's transparent
	Texture* newTexture = mem_Allocate( sizeof( Texture ) );
	if( newTexture == NULL ) {
		self->rendererObject = NULL;
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error allocating memory for spine atlas %s.", path );
		return;
	}

	if( gfxUtil_LoadTexture( path, newTexture ) < 0 ) {
		self->rendererObject = NULL;
		SDL_LogError( SDL_LOG_CATEGORY_VIDEO, "Error loading spine atlas %s.", path );
		return;
	}
	self->rendererObject = (void*)newTexture;
	self->width = newTexture->width;
	self->height = newTexture->height;
}

void _spAtlasPage_disposeTexture( spAtlasPage* self )
{
	gfxUtil_UnloadTexture( (Texture*)self->rendererObject );
	mem_Release( self->rendererObject );
}

char* _spUtil_readFile( const char* path, int* length )
{
	return _readFile(path, length);
}

void* Allocate_Spine( size_t size )
{
	return mem_Allocate_Data( size, __FILE__, __LINE__ );
}

void Release_Spine( void* data )
{
	mem_Release_Data( data, __FILE__, __LINE__ );
}

// general system functions
void spine_Init( void )
{
	memset( templates, 0, sizeof( templates ) );
	memset( instances, 0, sizeof( instances ) );

	_setMalloc( Allocate_Spine );
	_setFree( Release_Spine );

	lastInstance = -1;
}

void spine_CleanEverything( void )
{
	spine_CleanAllInstances( );
	for( int i = 0; i < MAX_TEMPLATES; ++i ) {
		spine_CleanTemplate( i );
	}
}

// template handling
/*
Loads a set of spine files. Assumes there's three files: fileNameBase.json, fileNameBase.atlas, and fileNameBase.png.
The template is created from these three files.
Returns the index of the template if the loading was successfull, -1 if it was not.
*/
int spine_LoadTemplate( const char* fileNameBase )
{
	char atlasName[256];
	char pngName[256];
	char jsonName[256];
	spSkeletonJson* json;

	int idx;
	for( idx = 0; ( templates[idx].skeletonData != NULL ) && ( idx < MAX_TEMPLATES ); ++idx ) ;
	if( idx >= MAX_TEMPLATES ) {
		return -1;
	}

	SDL_snprintf( atlasName, sizeof( atlasName ), "%s.atlas", fileNameBase );
	SDL_snprintf( jsonName, sizeof( jsonName ), "%s.json", fileNameBase );
	SDL_snprintf( pngName, sizeof( pngName ), "%s.png", fileNameBase );

	templates[idx].atlas = spAtlas_createFromFile( atlasName, 0 );
	if( templates[idx].atlas == NULL ) {
		SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Unable to load atlas." );
		return -1;
	}

	json = spSkeletonJson_create( templates[idx].atlas );
	if( json == NULL ) {
		SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Unable to create skeleton JSON." );

		spAtlas_dispose( templates[idx].atlas );
		templates[idx].atlas = NULL;

		return -1;
	}

	json->scale = 1.0f;

	templates[idx].skeletonData = spSkeletonJson_readSkeletonDataFile( json, jsonName );
	spSkeletonJson_dispose( json );
	if( templates[idx].skeletonData == NULL ) {
		SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Unable to create skeleton data." );

		spAtlas_dispose( templates[idx].atlas );
		templates[idx].atlas = NULL;

		return -1;
	}
	

	templates[idx].stateData = spAnimationStateData_create( templates[idx].skeletonData );
	if( templates[idx].stateData == NULL ) {
		SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Unable to create animation state data." );

		spSkeletonData_dispose( templates[idx].skeletonData );
		templates[idx].skeletonData = NULL;

		spAtlas_dispose( templates[idx].atlas );
		templates[idx].atlas = NULL;

		return -1;
	}

	return idx;
}

/*
Cleans up a template, freeing it's spot.
 Also checks to see if there are any existing instances using this template.
*/
void spine_CleanTemplate( int idx )
{
	assert( idx >= 0 );
	assert( idx < MAX_TEMPLATES );

	if( templates[idx].stateData != NULL ) {
		spAnimationStateData_dispose( templates[idx].stateData );
		templates[idx].stateData = NULL;
	}
	if( templates[idx].skeletonData != NULL ) {
		spSkeletonData_dispose( templates[idx].skeletonData );
		templates[idx].skeletonData = NULL;
	}
	if( templates[idx].atlas != NULL ) {
		spAtlas_dispose( templates[idx].atlas );
		templates[idx].atlas = NULL;
	}

	for( int i = 0; i < lastInstance; ++i ) {
		if( instances[i].templateIdx == idx ) {
			SDL_LogError( SDL_LOG_CATEGORY_RENDER, "Found a spine instance using a freed template." );
		}
	}
}

/*
Sets the duration of the blending between the fromName to the toName animation. The templateIdx passed in should be a
value returned from spine_LoadTemplate that hasn't been cleaned up yet.
*/
void spine_SetTemplateMix( int templateIdx, const char* fromName, const char* toName, float duration )
{
	assert( templateIdx >= 0 );
	assert( templateIdx < MAX_TEMPLATES );

	if( templates[templateIdx].stateData == NULL ) {
		return;
	}

	spAnimationStateData_setMixByName( templates[templateIdx].stateData, fromName, toName, duration );
}

// instance handling
/*
Creates an instance of a template. The templateIdx passed in should be a value returns from spine_LoadTemplate that
 hasn't been cleaned up.
Returns an id to use in other functions. Returns -1 if there's a problem.
*/
int spine_CreateInstance( int templateIdx, Vector2 pos, int cameraFlags, char depth, spAnimationStateListener listener )
{
	int idx;
	for( idx = 0; ( instances[idx].skeleton != NULL ) && ( idx < MAX_INSTANCES ); ++idx ) ;
	if( idx >= MAX_INSTANCES ) {
		return -1;
	}

	if( idx >= lastInstance ) {
		lastInstance = idx;
	}

	SpineInstance* charState = &( instances[idx] );

	charState->skeleton = spSkeleton_create( templates[templateIdx].skeletonData );
	if( charState->skeleton == NULL ) {
		SDL_LogError( SDL_LOG_CATEGORY_RENDER, "Unable to create skeleton." );
		return -1;
	}

	charState->state = spAnimationState_create( templates[templateIdx].stateData );
	if( charState->state == NULL ) {
		SDL_LogError( SDL_LOG_CATEGORY_RENDER, "Unable to create animation state." );

		spSkeleton_dispose( charState->skeleton );
		charState->skeleton = NULL;

		return -1;
	}
	
	charState->startPos = pos;
	charState->endPos = pos;
	charState->skeleton->x = pos.x;
	charState->skeleton->y = pos.y;

	charState->skeleton->flipX = 0;
	charState->skeleton->flipY = 1;
	spSkeleton_setToSetupPose( charState->skeleton );

	charState->state->listener = listener;

	charState->templateIdx = templateIdx;
	charState->cameraFlags = cameraFlags;
	charState->depth = depth;

	return idx;
}

/*
Cleans up a spine instance.
*/
void spine_CleanInstance( int idx )
{
	assert( idx >= 0 );
	assert( idx < MAX_INSTANCES );

	SpineInstance* charState = &( instances[idx] );

	if( charState->skeleton == NULL ) {
		return;
	}

	spAnimationState_dispose( charState->state );
	spSkeleton_dispose( charState->skeleton );

	charState->state = NULL;
	charState->skeleton = NULL;

	for( int i = lastInstance; ( i >= 0 ) && ( instances[idx].skeleton != NULL ); --i ) {
		lastInstance = i;
	}
}

/*
Cleans up all instances.
*/
void spine_CleanAllInstances( void )
{
	for( int i = 0; i < MAX_INSTANCES; ++i ) {
		spine_CleanInstance( i );
	}
}

/*
Sets the future position of the spine instance.
*/
void spine_SetInstancePosition( int id, const Vector2* pos )
{
	assert( id >= 0 );
	assert( id < MAX_INSTANCES );

	SpineInstance* charState = &( instances[id] );
	if( charState->skeleton == NULL ) {
		return;
	}

	charState->endPos = *pos;
}

/*
Flips the positions used for rendering the instances.
*/
void spine_FlipInstancePositions( void )
{
	for( int i = 0; i <= lastInstance; ++i ) {
		if( instances[i].skeleton == NULL ) {
			continue;
		}
		
		instances[i].startPos = instances[i].endPos;
	}
}

/*
Returns the skeleton of the spine instance, if there's an issue returns NULL.
 Note: Adjustments to the skeletons x and y are overwritten in spine_RenderInstances( ).
*/
spSkeleton* spine_GetInstanceSkeleton( int idx )
{
	return instances[idx].skeleton;
}

/*
Returns the animation state of the spine instance, if there's an issue returns NULL.
*/
spAnimationState* spine_GetInstanceAnimState( int idx )
{
	return instances[idx].state;
}

/*
Updates all the instance animations.
*/
void spine_UpdateInstances( float dt )
{
	for( int i = 0; i <= lastInstance; ++i ) {
		if( instances[i].skeleton == NULL ) {
			continue;
		}

		SpineInstance* instance = &( instances[i] );

		spSkeleton_update( instance->skeleton, dt );
		spAnimationState_update( instance->state, dt );
		spAnimationState_apply( instance->state, instance->skeleton );
		spSkeleton_updateWorldTransform( instance->skeleton );
	}
}

static void drawCharacter( SpineInstance* spine )
{
	// just draw bone positions to start with
	//GLuint texture;
	Color col;
	int camFlags = spine->cameraFlags;
	char depth = spine->depth;
	Texture* texture;

	for( int i = 0; i < spine->skeleton->slotsCount; ++i ) {
		spSlot* slot = spine->skeleton->drawOrder[i];
		spAttachment* attachment = slot->attachment;
		if( attachment == NULL ) {
			continue;
		}

		col.r = spine->skeleton->r * slot->r;
		col.g = spine->skeleton->g * slot->g;
		col.b = spine->skeleton->b * slot->b;
		col.a = spine->skeleton->a * slot->a;

		switch( attachment->type ) {
		case SP_ATTACHMENT_REGION: {
				float vertices[8];
				spRegionAttachment* regionAttachment = (spRegionAttachment*)attachment;
				spRegionAttachment_computeWorldVertices( regionAttachment, slot->bone, vertices );

				Vector2 positions[4];
				Vector2 uvs[4];

				for( int i = 0; i < 4; ++i ) {
					positions[i].x = vertices[2*i];
					positions[i].y = vertices[(2*i)+1];

					uvs[i].s = regionAttachment->uvs[2*i];
					uvs[i].t = regionAttachment->uvs[(2*i)+1];
				}

				texture = (Texture*)((spAtlasRegion*)regionAttachment->rendererObject)->page->rendererObject;

				triRenderer_Add( positions[0], positions[1], positions[2], uvs[0], uvs[1], uvs[2], ST_DEFAULT, texture->textureID, col, camFlags, depth, texture->flags & TF_IS_TRANSPARENT );
				triRenderer_Add( positions[0], positions[2], positions[3], uvs[0], uvs[2], uvs[3], ST_DEFAULT, texture->textureID, col, camFlags, depth, texture->flags & TF_IS_TRANSPARENT );
			} break;
		/*case SP_ATTACHMENT_BOUNDING_BOX: {
				// if we're debugging 
				spBoundingBoxAttachment* boundingBoxAttachment = (spBoundingBoxAttachment*)attachment;
				assert( boundingBoxAttachment->verticesCount < MAX_SPINE_VERTS );

				spBoundingBoxAttachment_computeWorldVertices( boundingBoxAttachment, slot->bone, spineVertices );

				for( int i = 0; i <	boundingBoxAttachment->verticesCount; ++i ) {
					Vector2 pos0, pos1;
					int j = ( i + 1 ) % boundingBoxAttachment->verticesCount;
					pos0.x = spineVertices[i*2];
					pos0.y = spineVertices[(i*2)+1];
					pos1.x = spineVertices[j*2];
					pos1.y = spineVertices[(j*2)+1];

					dbgDraw_Line( pos0, pos1, CLR_GREEN );
				}
			} break;*/
		case SP_ATTACHMENT_MESH: {
				spMeshAttachment* meshAttachment = (spMeshAttachment*)attachment;
				assert( meshAttachment->verticesCount < MAX_SPINE_VERTS );

				spMeshAttachment_computeWorldVertices( meshAttachment, slot, spineVertices );

				texture = (Texture*)((spAtlasRegion*)meshAttachment->rendererObject)->page->rendererObject;

				for( int i = 0; i < meshAttachment->trianglesCount; i+=3 ) {
					Vector2 verts[3];
					Vector2 uvs[3];
					
					for( int j = 0; j < 3; ++j ) {
						int baseIndex = meshAttachment->triangles[i+j] * 2;
						verts[j].x = spineVertices[baseIndex];
						verts[j].y = spineVertices[baseIndex+1];

						uvs[j].x = meshAttachment->uvs[baseIndex];
						uvs[j].y = meshAttachment->uvs[baseIndex+1];
					}

					triRenderer_AddVertices( verts, uvs, ST_DEFAULT, texture->textureID, col, camFlags, depth, texture->flags & TF_IS_TRANSPARENT );
				}
			} break;
		case SP_ATTACHMENT_SKINNED_MESH: {
				spSkinnedMeshAttachment* skinnedMeshAttachment = (spSkinnedMeshAttachment*)attachment;
				spSkinnedMeshAttachment_computeWorldVertices( skinnedMeshAttachment, slot, spineVertices );

				texture = (Texture*)((spAtlasRegion*)skinnedMeshAttachment->rendererObject)->page->rendererObject;

				for( int i = 0; i < skinnedMeshAttachment->trianglesCount; i+=3 ) {
					Vector2 verts[3];
					Vector2 uvs[3];

					for( int j = 0; j < 3; ++j ) {
						int baseIndex = skinnedMeshAttachment->triangles[i+j] * 2;
						verts[j].x = spineVertices[baseIndex];
						verts[j].y = spineVertices[baseIndex+1];

						uvs[j].x = skinnedMeshAttachment->uvs[baseIndex];
						uvs[j].y = skinnedMeshAttachment->uvs[baseIndex+1];
					}

					triRenderer_AddVertices( verts, uvs, ST_DEFAULT, texture->textureID, col, camFlags, depth, texture->flags & TF_IS_TRANSPARENT );
				}
			} break;
		default:
			SDL_LogDebug( SDL_LOG_CATEGORY_VIDEO, "Unknown attachment type.\n" );
			break;
		}
	}
}

/*
Draws all the spine instances.
*/
void spine_RenderInstances( float normTimeElapsed )
{
	for( int i = 0; i <= lastInstance; ++i ) {
		if( instances[i].skeleton == NULL ) {
			continue;
		}

		Vector2 pos;
		vec2_Lerp( &( instances[i].startPos ), &( instances[i].endPos ), normTimeElapsed, &pos );
		instances[i].skeleton->x = pos.x;
		instances[i].skeleton->y = pos.y;

		drawCharacter( &( instances[i] ) );
	}
}