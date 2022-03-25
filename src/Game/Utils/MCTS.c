#include <assert.h>
#include <math.h>
#include <stddef.h>

#include "../Utils/stretchyBuffer.h"
#include "../System/jobQueue.h"
#include "../System/random.h"

// Implementation of the Monte Carlo Search Tree algorithm
//  Before inclusion of this file you'll need to #define a few things:
//   #define MAX_PLAYERS # => how many players the game can handle at once, defaults to 2
//   #define MTSC_MOVE typeName => a type defining the moves that a game can make, defaults to int
//   #define MTCS_BOARD_STATE typeName => a type defining the state of the game's board, defaults to int
//   #define MCTS_MAX_ROLL_OUT_DEPTH # => how far the roll out will go before stopping, defaults to 512
//   #define MCTS_MAX_STEPS # => how many simulation steps will be run, defaults to 1000
//   #define MCTS_OPEN_LOOP => use an open loop variation of the MCTS, only stores the actions and not the states which are regenerated
//                             every time it needs to be accessed, use with stochastic games, is more processor intensive but uses less memory
//  Once included this will create the MCTS implementation. Note that all functions here are
//   static, so they won't be accessible outside where they're defined. This allows you to
//   have multiple implementations of this if necessary. This is done because C doesn't
//   support templates.

// TODO: Improvements
//  store the tree between runs, that way we can grow the tree out as play continues, also prune the tree for branches that aren't taken
//  improve caching of game states to avoid redudant stored states, doing this may also help reduce some of the issues with the interface
//  abstract out the test to see if it's done so we can base it on time, number of iterations, or any other desired condition
//  # improve the interface for this, having it included with the defines works, but it's messy
//    - for this what can we do?
//       values we're currently defining can be stored in a structure
//       some things we're doing to make sure it runs fast are also bound into this, for instance storing the scores for each player (since
//        we're not assuming the number of players or what order their turns can go in)
//     the move, state, and number of players won't work with storing this because they also modify the definition of structures used
//     with just straight C i'm not Cing a way to do this, so we'll just leave that be for right now
//  improve multi-threading so we can have multiple threads working on the same tree, possibly divide it into some task based thing

#ifndef MCTS_MOVE
#define MCTS_MOVE int
#endif

#ifndef MCTS_BOARD_STATE
#define MCTS_BOARD_STATE int
#endif

#ifndef MCTS_NUM_PLAYERS
#define MCTS_NUM_PLAYERS 2
#endif

#ifndef MCTS_MAX_ROLL_OUT_DEPTH
#define MCTS_MAX_ROLL_OUT_DEPTH 512
#endif

#ifndef MCTS_MAX_STEPS
#define MCTS_MAX_STEPS 1000
#endif

#ifndef MCTS_DEFAULT_SEARCH_CONSTANT
#define MCTS_DEFAULT_SEARCH_CONSTANT 1.4142135637f
#endif

typedef struct {
	// applies the move to the old state, creating a new state
	void ( *applyMove )( MCTS_BOARD_STATE* oldState, MCTS_MOVE* move, MCTS_BOARD_STATE* newStateOut );

	// should create a list of all moves possible from the passed in board state
	void ( *getPossibleMoveList )( MCTS_BOARD_STATE* boardGameState, MCTS_MOVE** sbMoveList_Out );

	// should return the id for the current player, should be an int in the range [0, NUM_PLAYERS)
	int ( *getCurrentPlayer )( MCTS_BOARD_STATE* state );

	// should return winner id, where that is >= 0 and < NUM_PLAYERS, a number >= NUM_PLAYERS if there's a tie, or -1 the game isn't over
	int ( *getWinner )( MCTS_BOARD_STATE* state );
} MCTSGameDefinition;

static RandomGroup mctsRandom;

#define INVALID_NODE SIZE_MAX

typedef struct {
	float scores[MCTS_NUM_PLAYERS];
	float evalCnt;

	size_t parent;
	size_t firstChild;
	size_t nextSibling;

	MCTS_MOVE causingMove; // the move that will result in the parent state moving to this state
#ifndef MCTS_OPEN_LOOP
	MCTS_BOARD_STATE state;
#endif
} MCTreeNode;

typedef struct {
	MCTreeNode* sbNodes;
	size_t rootNode;
	MCTSGameDefinition* gameDefinition;
	float searchConstant;

#ifdef MCTS_OPEN_LOOP
	MCTS_BOARD_STATE initialState;
#endif

} MCTree;

static void assertTree( MCTree* tree )
{
	assert( tree != NULL );

	size_t cnt = sb_Count( tree->sbNodes );
	size_t invalid = INVALID_NODE;
	for( size_t i = 0; i < sb_Count( tree->sbNodes ); ++i ) {
		assert( ( tree->sbNodes[i].parent < sb_Count( tree->sbNodes ) ) || ( ( tree->sbNodes[i].parent == INVALID_NODE ) && ( i == 0 ) ) );
		assert( ( tree->sbNodes[i].firstChild < sb_Count( tree->sbNodes ) ) || ( tree->sbNodes[i].firstChild == INVALID_NODE ) );
		assert( ( tree->sbNodes[i].nextSibling < sb_Count( tree->sbNodes ) ) || ( tree->sbNodes[i].nextSibling == INVALID_NODE ) );
	}
}

static void initNode( MCTree* tree, size_t idx )
{
	assert( tree != NULL );

	tree->sbNodes[idx].evalCnt = 0;
	for( int s = 0; s < MCTS_NUM_PLAYERS; ++s ) {
		tree->sbNodes[idx].scores[s] = 0.0f;
	}

	tree->sbNodes[idx].firstChild = INVALID_NODE;
	tree->sbNodes[idx].nextSibling = INVALID_NODE;
	tree->sbNodes[idx].parent = INVALID_NODE;
}

static void setChild( MCTree* tree, size_t parentIdx, size_t childIdx )
{
	assert( parentIdx != childIdx );
	assert( parentIdx < sb_Count( tree->sbNodes ) );
	assert( childIdx < sb_Count( tree->sbNodes ) );

	tree->sbNodes[childIdx].parent = parentIdx;

	if( tree->sbNodes[parentIdx].firstChild == INVALID_NODE ) {
		tree->sbNodes[parentIdx].firstChild = childIdx;
	} else {
		size_t lastChild = tree->sbNodes[parentIdx].firstChild;
		while( tree->sbNodes[lastChild].nextSibling != INVALID_NODE ) {
			lastChild = tree->sbNodes[lastChild].nextSibling;
		}
		tree->sbNodes[lastChild].nextSibling = childIdx;
	}

	tree->sbNodes[childIdx].nextSibling = INVALID_NODE;

	//assertTree( tree );
}

#ifdef MCTS_OPEN_LOOP
static void addChild( MCTree* tree, size_t parentIdx, MCTS_MOVE* causingMove )
#else
static void addChild( MCTree* tree, size_t parentIdx, MCTS_BOARD_STATE* state, MCTS_MOVE* causingMove )
#endif
{
	sb_Add( tree->sbNodes, 1 );
	size_t ni = ( sb_Count( tree->sbNodes ) - 1 );
	initNode( tree, ni );
	setChild( tree, parentIdx, ni );

#ifndef MCTS_OPEN_LOOP
	tree->sbNodes[ni].state = ( *state );
#endif
	tree->sbNodes[ni].causingMove = ( *causingMove );
}

// we're assuming if two moves are the same they have the same result
static bool hasChildWithMove( MCTree* tree, size_t parentIdx, MCTS_MOVE* causingMove )
{
	size_t childIdx = tree->sbNodes[parentIdx].firstChild;
	while( childIdx != INVALID_NODE ) {
		if( memcmp( &( tree->sbNodes[childIdx].causingMove ), causingMove, sizeof( MCTS_MOVE ) ) == 0 ) {
			return true;
		}
		childIdx = tree->sbNodes[childIdx].nextSibling;
	}

	return false;
}

static void mcts_Init( MCTree* tree, MCTSGameDefinition* gameDefinition, MCTS_BOARD_STATE* initialState )
{
	sb_Clear( tree->sbNodes );
	sb_Reserve( tree->sbNodes, 10000 );

	sb_Add( tree->sbNodes, 1 );
	initNode( tree, 0 );
#ifdef MCTS_OPEN_LOOP
	memcpy( &( tree->initialState ), initialState, sizeof( MCTS_BOARD_STATE ) );
#else
	memcpy( &( tree->sbNodes[0].state ), initialState, sizeof( MCTS_BOARD_STATE ) );
#endif
	tree->rootNode = 0;
	tree->gameDefinition = gameDefinition;
}

// We expand all the actions into child nodes
#ifdef MCTS_OPEN_LOOP
static void mcts_Expand( MCTree* tree, size_t idx, MCTS_BOARD_STATE* idxState )
{
	assert( tree != NULL );

	MCTS_MOVE* moves = NULL;
	tree->gameDefinition->getPossibleMoveList( idxState, &moves );

	for( size_t i = 0; i < sb_Count( moves ); ++i ) {
		// see if the move already exists in this nodes children, if not then add it
		if( !hasChildWithMove( tree, idx, &( moves[i] ) ) ) {
			addChild( tree, idx, &( moves[i] ) );
		}
		//assertTree( tree );
	}

	sb_Release( moves );
}
#else
static void mcts_Expand( MCTree* tree, size_t idx )
{
	assert( tree != NULL );

	MCTS_MOVE* moves = NULL;
	tree->gameDefinition->getPossibleMoveList( &( tree->sbNodes[idx].state ), &moves );

	for( size_t i = 0; i < sb_Count( moves ); ++i ) {
		MCTS_BOARD_STATE newState;
		tree->gameDefinition->applyMove( &( tree->sbNodes[idx].state ), &( moves[i] ), &newState );
		addChild( tree, idx, &newState, &( moves[i] ) );
		//assertTree( tree );
	}

	sb_Release( moves );
}
#endif

// for the chosen node, find the child that has the highest upper bound confidence value
//  we'll use UCB1
// UCB1 = x + C * sqrt( ln( n ) / j )
// x = average of node to check (totalScore / timesVisited)
// C = constant > 0, determines how much the algorithm will search, for games with a final score in the range [0,1], sqrtf( 2.0f ) is a good default
// n = number of times parent of node to check has been evaluated
// j = number of times the node to check has been evaluated
// if j = 0, then UCB1 = infinity
#ifdef MCTS_OPEN_LOOP
static size_t mtcs_BestChild( MCTree* tree, size_t idx, MCTS_BOARD_STATE* idxState, MCTS_BOARD_STATE* outState )
#else
static size_t mtcs_BestChild( MCTree* tree, size_t idx )
#endif
{
	assert( tree != NULL );

	// go through each child and find the best, if there are multiple best choose a random one
	//  uses reservoir sampling
	int numFound = 0;
	size_t best = INVALID_NODE;
	float bestScore = 0.0f;
	size_t curr = tree->sbNodes[idx].firstChild;
	float logCnt = logf( tree->sbNodes[idx].evalCnt );
#ifdef MCTS_OPEN_LOOP
	unsigned int currPlayer = tree->gameDefinition->getCurrentPlayer( idxState ); // get the move that led to this
#else
	unsigned int currPlayer = tree->gameDefinition->getCurrentPlayer( &( tree->sbNodes[idx].state ) ); // get the move that led to this
#endif

	while( curr != INVALID_NODE ) {
		// if the node to check has never been evaluated then it's score is infinity, otherwise use UCB1
		float uctValue = INFINITY;

		if( tree->sbNodes[curr].evalCnt > 0.0f ) {
			uctValue = ( tree->sbNodes[curr].scores[currPlayer] / tree->sbNodes[curr].evalCnt ) +
				( tree->searchConstant * sqrtf( logCnt / tree->sbNodes[curr].evalCnt ) );
		}

		if( ( uctValue > bestScore ) || ( best == INVALID_NODE ) ) {
			numFound = 1;
			best = curr;
			bestScore = uctValue;
		} else if( uctValue == bestScore ) {
			++numFound;
			// TODO: Improve randomness
			if( ( rand_GetU32( &mctsRandom ) % numFound ) == 0 ) {
				best = curr;
				bestScore = uctValue;
			}
		}

		curr = tree->sbNodes[curr].nextSibling;
	}
#ifdef MCTS_OPEN_LOOP
	MCTS_BOARD_STATE newState;
	tree->gameDefinition->applyMove( idxState, &( tree->sbNodes[best].causingMove ), &newState );
	( *outState ) = newState;
#endif

	return best;
}

// Return the node to run the rollout on.
//  We go from the root down, finding the best child at each spot.
//  When we reach a leaf node we then have two choices of action:
//   If it's been visited before we expand and continue
//   If it hasn't then we run from there
#ifdef MCTS_OPEN_LOOP
static size_t mcts_TreePolicy( MCTree* tree, MCTS_BOARD_STATE* outState )
#else
static size_t mcts_TreePolicy( MCTree* tree )
#endif
{
	assert( tree != NULL );

#ifdef MCTS_OPEN_LOOP
	( *outState ) = tree->initialState;
#endif

	size_t idx = tree->rootNode;
	while( tree->sbNodes[idx].firstChild != INVALID_NODE ) {
		
#ifdef MCTS_OPEN_LOOP
		mcts_Expand( tree, idx, outState );
		idx = mtcs_BestChild( tree, idx, outState, outState );
#else
		idx = mtcs_BestChild( tree, idx );
#endif
	}

	MCTS_BOARD_STATE* currState;
#ifdef MCTS_OPEN_LOOP
	currState = outState;
#else
	currState = &( tree->sbNodes[idx].state );
#endif
	// got a leaf
	if( ( tree->sbNodes[idx].evalCnt > 0.0f ) && ( tree->gameDefinition->getWinner( currState ) < 0 ) ) {
		// has been visited and is not a terminal node in the tree, expand
#ifdef MCTS_OPEN_LOOP
		mcts_Expand( tree, idx, outState );
		idx = mtcs_BestChild( tree, idx, outState, outState );
		currState = outState;
#else
		mcts_Expand( tree, idx );
		idx = mtcs_BestChild( tree, idx );
		currState = &( tree->sbNodes[idx].state );
#endif
	}

	return idx;
}

// Choose random moves to advance the state until we reach a terminal state or we've gone
//  on for too long.
static MCTS_MOVE* sbRolloutMoves = NULL; // so we don't have to reallocate every single time we run this
static int minDepth = MCTS_MAX_ROLL_OUT_DEPTH;
static int maxDepth = -1;
static void mcts_RollOut( MCTSGameDefinition* gameDefinition, MCTS_BOARD_STATE* state, float* outScore, int* outScoresIdx )
{
	MCTS_BOARD_STATE nextState;
	MCTS_BOARD_STATE currState = ( *state );
	int depth = 0;
	int winner;

	winner = gameDefinition->getWinner( &currState );
	while( ( depth < MCTS_MAX_ROLL_OUT_DEPTH ) && ( winner == -1 ) ) {
		sb_Clear( sbRolloutMoves );
		gameDefinition->getPossibleMoveList( &currState, &sbRolloutMoves );
		assert( sb_Count( sbRolloutMoves ) > 0 );
		gameDefinition->applyMove( &currState, &( sbRolloutMoves[rand_GetRangeS32( &mctsRandom, 0, sb_Count( sbRolloutMoves ) )] ), &nextState );
		currState = nextState;
		winner = gameDefinition->getWinner( &currState );
		++depth;
	}

	/*if( depth < minDepth ) minDepth = depth;
	if( depth > maxDepth ) maxDepth = depth;
	llog( LOG_DEBUG, "min: %i    max: %i", minDepth, maxDepth );//*/

	( *outScore ) = 1.0f;
	if( ( winner >= MCTS_NUM_PLAYERS ) || ( winner == -1 ) ) {
		( *outScoresIdx ) = -1;
	} else {
		( *outScoresIdx ) = winner;
	}
}

// Go up the tree propagating the score
static void mcts_BackPropagate( MCTree* tree, int idx, int playerIdx, float score )
{
	assert( tree != NULL );

	while( idx != INVALID_NODE ) {
		for( int i = 0; i < MCTS_NUM_PLAYERS; ++i ) {
			if( i == playerIdx ) {
				tree->sbNodes[idx].scores[i] += score;
			} else if( playerIdx < 0 ) {
				// handle ties
				tree->sbNodes[idx].scores[i] += score * 0.5f;
			}
		}
		tree->sbNodes[idx].evalCnt += 1.0f;

		idx = tree->sbNodes[idx].parent;
	}
}

static void mcts_Step( MCTree* tree )
{
	assert( tree != NULL );

	// as of right now only one player wins at a time
	//  if there is some sort of placing system we may want to adjust it so that instead of one score
	//  for a player, we have a score for each player
	float score;
	int player;

#ifdef MCTS_OPEN_LOOP
	MCTS_BOARD_STATE state;
	size_t idx = mcts_TreePolicy( tree, &state );
	mcts_RollOut( tree->gameDefinition, &state, &score, &player );
	mcts_BackPropagate( tree, idx, player, score );
#else
	size_t idx = mcts_TreePolicy( tree );
	mcts_RollOut( tree->gameDefinition, &( tree->sbNodes[idx].state ), &score, &player );
	mcts_BackPropagate( tree, idx, player, score );
#endif
}

static float mcts_Best_ByEvalCount( MCTree* tree, size_t nodeIdx )
{
	return tree->sbNodes[nodeIdx].evalCnt;
}

// so we'll need some way to get which players turn it was during a board state, then update the score for only that
//  player
// changes needed:
//  - getPlayer( GameBoard* state )
//  - backpropagte will have to take player index into account
//  - treePolicy and child choice will have to take player index into account
//  - need an array to store each players scores
static float amtAIDone = 0.0f;
static void monteCarloTreeSearch( MCTSGameDefinition definition, float searchConstant, MCTS_BOARD_STATE* initialState, MCTS_MOVE* outBestMove )
{
	MCTree tree;
	tree.sbNodes = NULL;
	tree.searchConstant = searchConstant;
	mcts_Init( &tree, &definition, initialState );

	for( size_t i = 0; i < MCTS_MAX_STEPS; ++i ) {
		mcts_Step( &tree );
		amtAIDone = (float)i / (float)MCTS_MAX_STEPS;
	}

	// get the best move from the tree, we'll do the most visited one
	size_t node = tree.sbNodes[tree.rootNode].firstChild;
	float bestScore = -1.0f;
	size_t bestIdx = INVALID_NODE;
	while( node != INVALID_NODE ) {
		float score = mcts_Best_ByEvalCount( &tree, node );
		if( score >= bestScore ) {
			bestScore = score;
			bestIdx = node;
		}
		node = tree.sbNodes[node].nextSibling;
	}

	assert( bestIdx != INVALID_NODE );

	( *outBestMove ) = tree.sbNodes[bestIdx].causingMove;

	sb_Release( tree.sbNodes );
}

typedef struct {
	MCTSGameDefinition gameDefinition;
	MCTS_BOARD_STATE* gameState;
	float searchConstant;
} BoardGameThreadData;

static MCTS_MOVE aiChosenMove;
static bool aiChosenMoveReady = false;
static void setMCTSMove( void* data )
{
	// let the main thread know that a move has been chosen
	aiChosenMove = *( (MCTS_MOVE*)data );
	mem_Release( data );
	aiChosenMoveReady = true;
}

static void runMCTSThread( void* data )
{
	MCTS_MOVE* move = (MCTS_MOVE*)mem_Allocate( sizeof( MCTS_MOVE ) );
	BoardGameThreadData* threadData = (BoardGameThreadData*)data;

	monteCarloTreeSearch( threadData->gameDefinition, threadData->searchConstant, threadData->gameState, move );

	mem_Release( threadData->gameState );
	mem_Release( data );

	jq_AddMainThreadJob( setMCTSMove, move );
}

// we're assuming the gameDefinition will not change
static void startMCTSThread( MCTSGameDefinition gameDefinition, float searchConstant, void* currentState )
{
	aiChosenMoveReady = false;

	BoardGameThreadData* threadData = (BoardGameThreadData*)mem_Allocate( sizeof( BoardGameThreadData ) );
	threadData->gameDefinition = gameDefinition;
	threadData->gameState = (MCTS_BOARD_STATE*)mem_Allocate( sizeof( MCTS_BOARD_STATE ) );
	threadData->searchConstant = searchConstant;
	memcpy( threadData->gameState, currentState, sizeof( MCTS_BOARD_STATE ) );

	jq_AddJob( runMCTSThread, (void*)threadData );
}

// clean up
#undef MCTS_MOVE
#undef MCTS_BOARD_STATE
#undef MCTS_NUM_PLAYERS
#undef MCTS_MAX_ROLL_OUT_DEPTH
#undef MCTS_MAX_STEPS