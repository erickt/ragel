/*
 *  Copyright 2001-2007 Adrian Thurston <thurston@complang.org>
 */

/*  This file is part of Ragel.
 *
 *  Ragel is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  Ragel is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with Ragel; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include "ragel.h"
#include "dotcodegen.h"
#include "gendata.h"
#include "inputdata.h"
#include "rlparse.h"
#include "rlscan.h"

using std::istream;
using std::ifstream;
using std::ostream;
using std::ios;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;

/* Override this so that write statement processing is ignored */
void GraphvizDotGen::writeStatement( InputLoc &, int, char ** )
{
}

std::ostream &GraphvizDotGen::KEY( Key key )
{
	if ( displayPrintables && key.isPrintable() ) {
		// Output values as characters, ensuring we escape the quote (") character
		char cVal = (char) key.getVal();
		switch ( cVal ) {
			case '"': case '\\':
				out << "'\\" << cVal << "'";
				break;
			case '\a':
				out << "'\\\\a'";
				break;
			case '\b':
				out << "'\\\\b'";
				break;
			case '\t':
				out << "'\\\\t'";
				break;
			case '\n':
				out << "'\\\\n'";
				break;
			case '\v':
				out << "'\\\\v'";
				break;
			case '\f':
				out << "'\\\\f'";
				break;
			case '\r':
				out << "'\\\\r'";
				break;
			case ' ':
				out << "SP";
				break;
			default:	
				out << "'" << cVal << "'";
				break;
		}
	}
	else {
		if ( keyOps->isSigned )
			out << key.getVal();
		else
			out << (unsigned long) key.getVal();
	}

	return out;
}

std::ostream &GraphvizDotGen::TRANS_ACTION( RedStateAp *fromState, RedTransAp *trans )
{
	int n = 0;
	RedAction *actions[3];

	if ( fromState->fromStateAction != 0 )
		actions[n++] = fromState->fromStateAction;
	if ( trans->action != 0 )
		actions[n++] = trans->action;
	if ( trans->targ != 0 && trans->targ->toStateAction != 0 )
		actions[n++] = trans->targ->toStateAction;

	if ( n > 0 )
		out << " / ";
	
	/* Loop the existing actions and write out what's there. */
	for ( int a = 0; a < n; a++ ) {
		for ( GenActionTable::Iter actIt = actions[a]->key.first(); actIt.lte(); actIt++ ) {
			GenAction *action = actIt->value;
			out << action->nameOrLoc();
			if ( a < n-1 || !actIt.last() )
				out << ", ";
		}
	}
	return out;
}

std::ostream &GraphvizDotGen::ACTION( RedAction *action )
{
	/* The action. */
	out << " / ";
	for ( GenActionTable::Iter actIt = action->key.first(); actIt.lte(); actIt++ ) {
		GenAction *action = actIt->value;
		if ( action->name != 0 )
			out << action->name;
		else
			out << action->loc.line << ":" << action->loc.col;
		if ( !actIt.last() )
			out << ", ";
	}
	return out;
}

std::ostream &GraphvizDotGen::ONCHAR( Key lowKey, Key highKey )
{
	GenCondSpace *condSpace;
	if ( lowKey > keyOps->maxKey && (condSpace=findCondSpace(lowKey, highKey) ) ) {
		Key values = ( lowKey - condSpace->baseKey ) / keyOps->alphSize();

		lowKey = keyOps->minKey + 
			(lowKey - condSpace->baseKey - keyOps->alphSize() * values.getVal());
		highKey = keyOps->minKey + 
			(highKey - condSpace->baseKey - keyOps->alphSize() * values.getVal());
		KEY( lowKey );
		if ( lowKey != highKey ) {
			out << "..";
			KEY( highKey );
		}
		out << "(";

		for ( GenCondSet::Iter csi = condSpace->condSet; csi.lte(); csi++ ) {
			bool set = values & (1 << csi.pos());
			if ( !set )
				out << "!";
			out << (*csi)->nameOrLoc();
			if ( !csi.last() )
				out << ", ";
		}
		out << ")";
	}
	else {
		/* Output the key. Possibly a range. */
		KEY( lowKey );
		if ( highKey != lowKey ) {
			out << "..";
			KEY( highKey );
		}
	}
	return out;
}

void GraphvizDotGen::writeTransList( RedStateAp *state )
{
	/* Build the set of unique transitions out of this state. */
	RedTransSet stTransSet;
	for ( RedTransList::Iter tel = state->outRange; tel.lte(); tel++ ) {
		/* If we haven't seen the transitions before, the move forward
		 * emitting all the transitions on the same character. */
		if ( stTransSet.insert( tel->value ) ) {
			/* Write out the from and to states. */
			out << "\t" << state->id << " -> ";

			if ( tel->value->targ == 0 )
				out << "err_" << state->id;
			else
				out << tel->value->targ->id;

			/* Begin the label. */
			out << " [ label = \""; 
			ONCHAR( tel->lowKey, tel->highKey );

			/* Walk the transition list, finding the same. */
			for ( RedTransList::Iter mtel = tel.next(); mtel.lte(); mtel++ ) {
				if ( mtel->value == tel->value ) {
					out << ", ";
					ONCHAR( mtel->lowKey, mtel->highKey );
				}
			}

			/* Write the action and close the transition. */
			TRANS_ACTION( state, tel->value );
			out << "\" ];\n";
		}
	}

	/* Write the default transition. */
	if ( state->defTrans != 0 ) {
		/* Write out the from and to states. */
		out << "\t" << state->id << " -> ";

		if ( state->defTrans->targ == 0 )
			out << "err_" << state->id;
		else
			out << state->defTrans->targ->id;

		/* Begin the label. */
		out << " [ label = \"DEF"; 

		/* Write the action and close the transition. */
		TRANS_ACTION( state, state->defTrans );
		out << "\" ];\n";
	}
}

void GraphvizDotGen::writeDotFile( )
{
	out << 
		"digraph " << fsmName << " {\n"
		"	rankdir=LR;\n";
	
	/* Define the psuedo states. Transitions will be done after the states
	 * have been defined as either final or not final. */
	out << "	node [ shape = point ];\n";

	if ( redFsm->startState != 0 )
		out << "	ENTRY;\n";

	/* Psuedo states for entry points in the entry map. */
	for ( EntryIdVect::Iter en = entryPointIds; en.lte(); en++ ) {
		RedStateAp *state = allStates + *en;
		out << "	en_" << state->id << ";\n";
	}

	/* Psuedo states for final states with eof actions. */
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		if ( st->eofTrans != 0 && st->eofTrans->action != 0 )
			out << "	eof_" << st->id << ";\n";
		if ( st->eofAction != 0 )
			out << "	eof_" << st->id << ";\n";
	}

	out << "	node [ shape = circle, height = 0.2 ];\n";

	/* Psuedo states for states whose default actions go to error. */
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		bool needsErr = false;
		if ( st->defTrans != 0 && st->defTrans->targ == 0 )
			needsErr = true;
		else {
			for ( RedTransList::Iter tel = st->outRange; tel.lte(); tel++ ) {
				if ( tel->value->targ == 0 ) {
					needsErr = true;
					break;
				}
			}
		}

		if ( needsErr )
			out << "	err_" << st->id << " [ label=\"\"];\n";
	}

	/* Attributes common to all nodes, plus double circle for final states. */
	out << "	node [ fixedsize = true, height = 0.65, shape = doublecircle ];\n";

	/* List Final states. */
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		if ( st->isFinal )
			out << "	" << st->id << ";\n";
	}

	/* List transitions. */
	out << "	node [ shape = circle ];\n";

	/* Walk the states. */
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ )
		writeTransList( st );

	/* Transitions into the start state. */
	if ( redFsm->startState != 0 ) 
		out << "	ENTRY -> " << redFsm->startState->id << " [ label = \"IN\" ];\n";

	/* Transitions into the entry points. */
	for ( EntryIdVect::Iter en = entryPointIds; en.lte(); en++ ) {
		RedStateAp *state = allStates + *en;
		char *name = entryPointNames[en.pos()];
		out << "	en_" << state->id << " -> " << state->id <<
				" [ label = \"" << name << "\" ];\n";
	}

	/* Out action transitions. */
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		if ( st->eofTrans != 0 && st->eofTrans->action != 0 ) {
			out << "	" << st->id << " -> eof_" << 
					st->id << " [ label = \"EOF"; 
			ACTION( st->eofTrans->action ) << "\" ];\n";
		}
		if ( st->eofAction != 0 ) {
			out << "	" << st->id << " -> eof_" << 
					st->id << " [ label = \"EOF"; 
			ACTION( st->eofAction ) << "\" ];\n";
		}
	}

	out <<
		"}\n";
}

void GraphvizDotGen::finishRagelDef()
{
	/* For dot file generation we want to pick default transitions. */
	redFsm->chooseDefaultSpan();
}

std::ostream &InputData::KEY( ostream &out, Key key )
{
	if ( displayPrintables && key.isPrintable() ) {
		// Output values as characters, ensuring we escape the quote (") character
		char cVal = (char) key.getVal();
		switch ( cVal ) {
			case '"': case '\\':
				out << "'\\" << cVal << "'";
				break;
			case '\a':
				out << "'\\\\a'";
				break;
			case '\b':
				out << "'\\\\b'";
				break;
			case '\t':
				out << "'\\\\t'";
				break;
			case '\n':
				out << "'\\\\n'";
				break;
			case '\v':
				out << "'\\\\v'";
				break;
			case '\f':
				out << "'\\\\f'";
				break;
			case '\r':
				out << "'\\\\r'";
				break;
			case ' ':
				out << "SP";
				break;
			default:	
				out << "'" << cVal << "'";
				break;
		}
	}
	else {
		if ( keyOps->isSigned )
			out << key.getVal();
		else
			out << (unsigned long) key.getVal();
	}

	return out;
}


std::ostream &InputData::ONCHAR( ostream &out, Key lowKey, Key highKey, CondSpace *condSpace, long condVals )
{
	/* Output the key. Possibly a range. */
	KEY( out, lowKey );
	if ( highKey != lowKey ) {
		out << "..";
		KEY( out, highKey );
	}

	if ( condSpace != 0 ) {
		out << "(";
		for ( CondSet::Iter csi = condSpace->condSet; csi.lte(); csi++ ) {
			bool set = condVals & (1 << csi.pos());
			if ( !set )
				out << "!";
			(*csi)->actionName( out );
			if ( !csi.last() )
				out << ", ";
		}
		out << ")";
	}

	return out;
}


std::ostream &InputData::TRANS_ACTION( ostream &out, StateAp *fromState, CondAp *trans )
{
	int n = 0;
	ActionTable *actionTables[3] = { 0, 0, 0 };

	if ( fromState->fromStateActionTable.length() != 0 )
		actionTables[n++] = &fromState->fromStateActionTable;
	if ( trans->actionTable.length() != 0 )
		actionTables[n++] = &trans->actionTable;
	if ( trans->toState != 0 && trans->toState->toStateActionTable.length() != 0 )
		actionTables[n++] = &trans->toState->toStateActionTable;

	if ( n > 0 )
		out << " / ";
	
	/* Loop the existing actions and write out what's there. */
	for ( int a = 0; a < n; a++ ) {
		for ( ActionTable::Iter actIt = actionTables[a]->first(); actIt.lte(); actIt++ ) {
			Action *action = actIt->value;
			action->actionName( out );
			if ( a < n-1 || !actIt.last() )
				out << ", ";
		}
	}
	return out;
}

std::ostream &InputData::ACTION( ostream &out, ActionTable *actionTable )
{
	/* The action. */
	out << " / ";
	for ( ActionTable::Iter actIt = actionTable->first(); actIt.lte(); actIt++ ) {
		Action *action = actIt->value;
		action->actionName( out );
		if ( !actIt.last() )
			out << ", ";
	}
	return out;
}

void InputData::writeTransList( ostream &out, StateAp *state )
{
	/* Build the set of unique transitions out of this state. */
	RedTransSet stTransSet;
	for ( TransList::Iter tel = state->outList; tel.lte(); tel++ ) {
		for ( CondTransList::Iter ctel = tel->ctList; ctel.lte(); ctel++ ) {
			/* Write out the from and to states. */
			out << "\t" << state->alg.stateNum << " -> ";

			if ( ctel->toState == 0 )
				out << "err_" << state->alg.stateNum;
			else
				out << ctel->toState->alg.stateNum;

			/* Begin the label. */
			out << " [ label = \""; 
			ONCHAR( out, tel->lowKey, tel->highKey, tel->condSpace, ctel->lowKey.getVal() );

			/* Write the action and close the transition. */
			TRANS_ACTION( out, state, ctel );
			out << "\" ];\n";
		}
	}
}

bool InputData::makeNameInst( std::string &res, NameInst *nameInst )
{
	bool written = false;
	if ( nameInst->parent != 0 )
		written = makeNameInst( res, nameInst->parent );
	
	if ( nameInst->name != 0 ) {
		if ( written )
			res += '_';
		res += nameInst->name;
		written = true;
	}

	return written;
}

void InputData::writeDot( ostream &out )
{
//	static_cast<GraphvizDotGen*>(dotGenParser->pd->cgd)->writeDotFile();

	ParseData *pd = dotGenParser->pd;
	FsmAp *graph = pd->sectionGraph;

	out << 
		"digraph " << pd->sectionName << " {\n"
		"	rankdir=LR;\n";
	
	/* Define the psuedo states. Transitions will be done after the states
	 * have been defined as either final or not final. */
	out << "	node [ shape = point ];\n";

	if ( graph->startState != 0 )
		out << "	ENTRY;\n";

	/* Psuedo states for entry points in the entry map. */
	for ( EntryMap::Iter en = graph->entryPoints; en.lte(); en++ ) {
		StateAp *state = en->value;
		out << "	en_" << state->alg.stateNum << ";\n";
	}

	/* Psuedo states for final states with eof actions. */
	for ( StateList::Iter st = graph->stateList; st.lte(); st++ ) {
		//if ( st->eofTrans != 0 && st->eofTrans->action != 0 )
		//	out << "	eof_" << st->id << ";\n";
		if ( st->eofActionTable.length() > 0 )
			out << "	eof_" << st->alg.stateNum << ";\n";
	}

	out << "	node [ shape = circle, height = 0.2 ];\n";

	/* Psuedo states for states whose default actions go to error. */
	for ( StateList::Iter st = graph->stateList; st.lte(); st++ ) {
		bool needsErr = false;
		for ( TransList::Iter tel = st->outList; tel.lte(); tel++ ) {
			for ( CondTransList::Iter ctel = tel->ctList; ctel.lte(); ctel++ ) {
				if ( ctel->toState == 0 ) {
					needsErr = true;
					break;
				}
			}
		}

		if ( needsErr )
			out << "	err_" << st->alg.stateNum << " [ label=\"\"];\n";
	}

	/* Attributes common to all nodes, plus double circle for final states. */
	out << "	node [ fixedsize = true, height = 0.65, shape = doublecircle ];\n";

	/* List Final states. */
	for ( StateList::Iter st = graph->stateList; st.lte(); st++ ) {
		if ( st->isFinState() )
			out << "	" << st->alg.stateNum << ";\n";
	}

	/* List transitions. */
	out << "	node [ shape = circle ];\n";

	/* Walk the states. */
	for ( StateList::Iter st = graph->stateList; st.lte(); st++ )
		writeTransList( out, st );

	/* Transitions into the start state. */
	if ( graph->startState != 0 ) 
		out << "	ENTRY -> " << graph->startState->alg.stateNum << " [ label = \"IN\" ];\n";

	for ( EntryMap::Iter en = graph->entryPoints; en.lte(); en++ ) {
		NameInst *nameInst = pd->nameIndex[en->key];
		std::string name;
		makeNameInst( name, nameInst );
		StateAp *state = en->value;
		out << "	en_" << state->alg.stateNum << 
				" -> " << state->alg.stateNum <<
				" [ label = \"" << name << "\" ];\n";
	}

	/* Out action transitions. */
	for ( StateList::Iter st = graph->stateList; st.lte(); st++ ) {
		if ( st->eofActionTable.length() != 0 ) {
			out << "	" << st->alg.stateNum << " -> eof_" << 
					st->alg.stateNum << " [ label = \"EOF"; 
			ACTION( out, &st->eofActionTable ) << "\" ];\n";
		}
	}

	out <<
		"}\n";
}