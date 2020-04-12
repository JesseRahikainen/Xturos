#include "aStar.h"

#include <assert.h>
#include <float.h>
#include <math.h>

#include "../System/memory.h"
#include "stretchyBuffer.h"

// used if they don't give us a move or heuristic cost function
static float defaultCost( void* graph, int fromNode, int toNode )
{
	return 0.0f;
}

// creates a search state we can send to process and extract, we should call aStar_CleanUpSearchState
//  after we're done with it to clean up any memory we have allocated here
void aStar_CreateSearchState( void* graph, size_t nodeCount, int startNodeID, int targetNodeID,
	AStar_CostFunc moveCost, AStar_CostFunc heuristic, AStar_GetNextNeighborFunc nextNeighbor,
	AStarSearchState* outState )
{
	assert( outState != NULL );
	assert( nextNeighbor != NULL );

	outState->graph = graph;
	outState->moveCost = ( moveCost == NULL ) ? defaultCost : moveCost;
	outState->heuristic = ( heuristic == NULL ) ? defaultCost : heuristic;
	outState->nextNeighbor = nextNeighbor;

	outState->sbPath = NULL;
	outState->sbFrontier = NULL;

	if( nodeCount == 0 ) {
		return;
	}

	sb_Add( outState->sbPath, nodeCount );
	for( size_t i = 0; i < sb_Count( outState->sbPath ); ++i ) {
		outState->sbPath[i].from = -1;
		outState->sbPath[i].cost = INFINITY;
	}

	AStarFrontierData initASD;
	initASD.loc = startNodeID;
	initASD.cost = 0.0f;
	sb_Reserve( outState->sbFrontier, nodeCount ); // just reserve some data
	sb_Push( outState->sbFrontier, initASD );

	outState->sbPath[startNodeID].from = startNodeID;
	outState->sbPath[startNodeID].cost = 0.0f;

	outState->targetNodeID = targetNodeID;
	outState->startNodeID = startNodeID;
}

// in case we need to test for this, if it's invalid aStar_ProcessPath will handle it by returning
//  an empty path
bool aStar_IsValid( AStarSearchState* state )
{
	if( state->sbPath == NULL ) {
		return false;
	}

	if( state->sbFrontier == NULL ) {
		return false;
	}

	return true;
}

// process numSteps nodes in the search state, pass in -1 for numSteps to go until the end
//  returns whether processing is done and puts the resulting path into a stretchy buffer in sbOutPaths
bool aStar_ProcessPath( AStarSearchState* state, int numSteps, int** sbOutPaths )
{
	if( !aStar_IsValid( state ) ) {
		return true;
	}

	// everything should already be setup
	bool done = false;
	bool processingDone = false;
	while( !( done || processingDone ) ) {
		if( sb_Count( state->sbFrontier ) <= 0 ) {
			// no path found
			processingDone = true;
		} else {
		
			AStarFrontierData front = sb_Pop( state->sbFrontier );
			if( front.loc == state->targetNodeID ) {
				// found the path, generate it and put it into sbOutPaths and stop processing
				if( sbOutPaths != NULL ) {
					int current = state->targetNodeID;
					while( current != state->startNodeID ) {
						sb_Push( (*sbOutPaths), current );
						current = state->sbPath[current].from;
					}
				}
				processingDone = true;
			} else {

				int neighbor = state->nextNeighbor( state->graph, front.loc, -1 );
				while( neighbor != -1 ) {
					float newCost = state->sbPath[front.loc].cost + state->moveCost( state->graph, front.loc, neighbor );
					// no set check because we're initializing the values to FLT_MAX
					if( newCost < state->sbPath[neighbor].cost ) {
						assert( neighbor < (int)sb_Count( state->sbPath ) );

						state->sbPath[neighbor].cost = newCost;
						state->sbPath[neighbor].from = front.loc;

						AStarFrontierData asfd;
						asfd.loc = neighbor;

						asfd.cost = newCost + state->heuristic( state->graph, neighbor, state->targetNodeID );

						// insertion sort into frontier, simulate a priority queue
						// TODO: Create a proper priority queue if speed ever becomes an issue here
						size_t idx = 0;
						while( ( idx < sb_Count( state->sbFrontier ) ) && ( asfd.cost < state->sbFrontier[idx].cost ) ) {
							++idx;
						}
						sb_Insert( state->sbFrontier, idx, asfd );
					}
					neighbor = state->nextNeighbor( state->graph, front.loc, neighbor );
				}
			}
		}


		if( numSteps > 0 ) --numSteps;
		done = ( numSteps == 0 );
	}

	return processingDone;
}

// cleans up all the extra data created by the search state
void aStar_CleanUpSearchState( AStarSearchState* state )
{
	sb_Release( state->sbFrontier );
	state->sbFrontier = NULL;

	sb_Release( state->sbPath );
	state->sbPath = NULL;
}