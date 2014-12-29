/*
 *  Copyright 2006-2007 Adrian Thurston <thurston@complang.org>
 *            2007 Colin Fleming <colin.fleming@caverock.com>
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
#include "rustcodegen.h"
#include "redfsm.h"
#include "gendata.h"
#include <iomanip>
#include <sstream>

/* Integer array line length. */
#define IALL 12

#define _resume    1
#define _again     2
#define _eof_trans 3
#define _test_eof  4
#define _out       5

using std::setw;
using std::ios;
using std::ostringstream;
using std::string;
using std::cerr;

using std::istream;
using std::ifstream;
using std::ostream;
using std::ios;
using std::setiosflags;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;

void rustLineDirective( ostream &out, const char *fileName, int line )
{
	if ( noLineDirectives )
		return;

	/* Write the line info for to the input file. */
	out << "//#set_loc(" << line << ", \"";
	for ( const char *pc = fileName; *pc != 0; pc++ ) {
		if ( *pc == '\\' )
			out << "\\\\";
		else
			out << *pc;
	}
	out << "\");\n";
}

void RustTabCodeGen::genLineDirective( ostream &out )
{
	std::streambuf *sbuf = out.rdbuf();
	output_filter *filter = static_cast<output_filter*>(sbuf);
	rustLineDirective( out, filter->fileName, filter->line + 1 );
}

void RustTabCodeGen::GOTO( ostream &ret, int gotoDest, bool inFinish )
{
	ret <<
		"{\n"
		"    " << vCS() << " = " << gotoDest << ";\n"
		"    _goto_targ = " << _again << ";\n"
		"    " << CTRL_FLOW() << "continue '_goto;\n"
		"}";
}

void RustTabCodeGen::GOTO_EXPR( ostream &ret, GenInlineItem *ilItem, bool inFinish )
{
	ret <<
		"{\n"
		"    " << vCS() << " = (";

	INLINE_LIST( ret, ilItem->children, 0, inFinish );

	ret <<
		"    );\n"
		"    _goto_targ = " << _again << ";\n"
		"    " << CTRL_FLOW() <<
		"    continue '_goto;\n"
		"}";
}

void RustTabCodeGen::CALL( ostream &ret, int callDest, int targState, bool inFinish )
{
	if ( prePushExpr != 0 ) {
		ret << "{";
		INLINE_LIST( ret, prePushExpr, 0, false );
	}

	ret <<
		"{\n"
		"    " << STACK() << "[" << TOP() << "] = " << vCS() << ";\n"
		"    " << TOP() << " += 1;\n"
		"    " << vCS() << " = " << callDest << ";\n"
		"    _goto_targ = " << _again << ";\n"
		"    " << CTRL_FLOW() <<
		"    continue '_goto;\n"
		"}";

	if ( prePushExpr != 0 )
		ret << "}";
}

void RustTabCodeGen::CALL_EXPR( ostream &ret, GenInlineItem *ilItem, int targState, bool inFinish )
{
	if ( prePushExpr != 0 ) {
		ret << "{";
		INLINE_LIST( ret, prePushExpr, 0, false );
	}

	ret <<
		"{\n"
		"    " << STACK() << "[" << TOP() << "] = " << vCS() << ";\n"
		"    " << TOP() << " += 1;\n"
		"    " << vCS() << " = (";
	INLINE_LIST( ret, ilItem->children, targState, inFinish );
	ret <<
		"    );\n"
		"    _goto_targ = " << _again << ";\n"
		"    " << CTRL_FLOW() <<
		"    continue '_goto;\n"
		"}";

	if ( prePushExpr != 0 )
		ret << "}";
}

void RustTabCodeGen::RET( ostream &ret, bool inFinish )
{
	ret <<
		"{\n"
		"    " << TOP() << " -= 1;\n"
		"    " << vCS() << " = " << STACK() << "[" << TOP() << "] as uint;";

	if ( postPopExpr != 0 ) {
		ret << "    {";
		INLINE_LIST( ret, postPopExpr, 0, false );
		ret << "    }";
	}

	ret <<
		"    _goto_targ = " << _again << ";\n"
		"    " << CTRL_FLOW() <<
		"    continue '_goto;\n"
		"}";
}

void RustTabCodeGen::BREAK( ostream &ret, int targState )
{
	ret <<
		"{\n"
		"    " << P() << " += 1;\n"
		"    _goto_targ = " << _out << ";\n"
		"    " << CTRL_FLOW() <<
		"    continue '_goto;\n"
		"}";
}

void RustTabCodeGen::NEXT( ostream &ret, int nextDest, bool inFinish )
{
	ret << vCS() << " = " << nextDest << ";";
}

void RustTabCodeGen::NEXT_EXPR( ostream &ret, GenInlineItem *ilItem, bool inFinish )
{
	ret << vCS() << " = (";
	INLINE_LIST( ret, ilItem->children, 0, inFinish );
	ret << ");";
}

void RustTabCodeGen::EXEC( ostream &ret, GenInlineItem *item, int targState, int inFinish )
{
	/* The parser gives fexec two children. The double brackets are for D
	 * code. If the inline list is a single word it will get interpreted as a
	 * C-style cast by the D compiler. */
	ret << "{" << P() << " = ((";
	INLINE_LIST( ret, item->children, targState, inFinish );
	ret << "))-1;}";
}

/* Write out an inline tree structure. Walks the list and possibly calls out
 * to virtual functions than handle language specific items in the tree. */
void RustTabCodeGen::INLINE_LIST( ostream &ret, GenInlineList *inlineList,
		int targState, bool inFinish )
{
	for ( GenInlineList::Iter item = *inlineList; item.lte(); item++ ) {
		switch ( item->type ) {
		case GenInlineItem::Text:
			ret << item->data;
			break;
		case GenInlineItem::Goto:
			GOTO( ret, item->targState->id, inFinish );
			break;
		case GenInlineItem::Call:
			CALL( ret, item->targState->id, targState, inFinish );
			break;
		case GenInlineItem::Next:
			NEXT( ret, item->targState->id, inFinish );
			break;
		case GenInlineItem::Ret:
			RET( ret, inFinish );
			break;
		case GenInlineItem::PChar:
			ret << P();
			break;
		case GenInlineItem::Char:
			ret << GET_KEY();
			break;
		case GenInlineItem::Hold:
			ret << P() << "-= 1;";
			break;
		case GenInlineItem::Exec:
			EXEC( ret, item, targState, inFinish );
			break;
		case GenInlineItem::Curs:
			ret << "(_ps)";
			break;
		case GenInlineItem::Targs:
			ret << "(" << vCS() << ")";
			break;
		case GenInlineItem::Entry:
			ret << item->targState->id;
			break;
		case GenInlineItem::GotoExpr:
			GOTO_EXPR( ret, item, inFinish );
			break;
		case GenInlineItem::CallExpr:
			CALL_EXPR( ret, item, targState, inFinish );
			break;
		case GenInlineItem::NextExpr:
			NEXT_EXPR( ret, item, inFinish );
			break;
		case GenInlineItem::LmSwitch:
			LM_SWITCH( ret, item, targState, inFinish );
			break;
		case GenInlineItem::LmSetActId:
			SET_ACT( ret, item );
			break;
		case GenInlineItem::LmSetTokEnd:
			SET_TOKEND( ret, item );
			break;
		case GenInlineItem::LmGetTokEnd:
			GET_TOKEND( ret, item );
			break;
		case GenInlineItem::LmInitTokStart:
			INIT_TOKSTART( ret, item );
			break;
		case GenInlineItem::LmInitAct:
			INIT_ACT( ret, item );
			break;
		case GenInlineItem::LmSetTokStart:
			SET_TOKSTART( ret, item );
			break;
		case GenInlineItem::SubAction:
			SUB_ACTION( ret, item, targState, inFinish );
			break;
		case GenInlineItem::Break:
			BREAK( ret, targState );
			break;
		}
	}
}

string RustTabCodeGen::DATA_PREFIX()
{
	if ( !noPrefix )
		return FSM_NAME() + "_";
	return "";
}

/* Emit the alphabet data type. */
string RustTabCodeGen::ALPH_TYPE()
{
	string ret = keyOps->alphType->data1;
	if ( keyOps->alphType->data2 != 0 ) {
		ret += " ";
		ret += + keyOps->alphType->data2;
	}
	return ret;
}

/* Emit the alphabet data type. */
string RustTabCodeGen::WIDE_ALPH_TYPE()
{
	string ret;
	if ( redFsm->maxKey <= keyOps->maxKey )
		ret = ALPH_TYPE();
	else {
		long long maxKeyVal = redFsm->maxKey.getLongLong();
		HostType *wideType = keyOps->typeSubsumes( keyOps->isSigned, maxKeyVal );
		assert( wideType != 0 );

		ret = wideType->data1;
		if ( wideType->data2 != 0 ) {
			ret += " ";
			ret += wideType->data2;
		}
	}
	return ret;
}



void RustTabCodeGen::COND_TRANSLATE()
{
	out <<
		"            _widec = " << GET_KEY() << ";\n"
		"            _keys = " << CO() << "[" << vCS() << "] as uint * 2\n;"
		"            _klen = " << CL() << "[" << vCS() << "] as uint;\n"
		"            if _klen > 0 {\n"
		"                let mut _lower: uint = _keys\n;"
		"                let mut _mid: uint;\n"
		"                let mut _upper: uint = _keys + (_klen<<1) - 2;\n"
		"                loop {\n"
		"                if _upper < _lower { break; }\n"
		"\n"
		"                _mid = _lower + (((_upper-_lower) >> 1) & !1);\n"
		"                if " << GET_WIDE_KEY() << " < " << CK() << "[_mid] {\n"
		"                    _upper = _mid - 2;\n"
		"                } else if " << GET_WIDE_KEY() << " > " << CK() << "[_mid+1] {\n"
		"                    _lower = _mid + 2;\n"
		"                } else {\n"
		"                    match " << C() << "[" << CO() << "[" << vCS() << "]"
		" + ((_mid - _keys)>>1)] => {\n"
		;

	for ( CondSpaceList::Iter csi = condSpaceList; csi.lte(); csi++ ) {
		GenCondSpace *condSpace = csi;
		out << "                    " << condSpace->condSpaceId << "{\n";
		out << "                      _widec = " << KEY(condSpace->baseKey) <<
				" + (" << GET_KEY() << " - " << KEY(keyOps->minKey) << ");\n";

		for ( GenCondSet::Iter csi = condSpace->condSet; csi.lte(); csi++ ) {
			out << "                    if ";
			CONDITION( out, *csi );
			Size condValOffset = ((1 << csi.pos()) * keyOps->alphSize());
			out << " { _widec += " << condValOffset << "; }\n";
		}

		out <<
			"                    break;\n"
			"                }\n";
	}

	out <<
		"}\n"
		"                      break;\n"
		"                    }\n"
		"                  }\n"
		"                }\n"
		"\n";
}


void RustTabCodeGen::LOCATE_TRANS()
{
	out <<
		"            '_match: loop {\n"
		"                _keys = " << KO() << "[" << vCS() << "] as uint;\n"
		"                _trans = " << IO() << "[" << vCS() << "] as uint;\n"
		"                _klen = " << SL() << "[" << vCS() << "] as uint;\n"
		"                if _klen > 0 {\n"
		"                    let mut _lower: uint = _keys;\n"
		"                    let mut _mid: uint;\n"
		"                    let mut _upper: uint = _keys + _klen - 1;\n"
		"                    loop {\n"
		"                        if _upper < _lower { break; }\n"
		"\n"
		"                        _mid = _lower + ((_upper-_lower) >> 1);\n"
		"                        if " << GET_WIDE_KEY() << " < " << K() << "[_mid] {\n"
		"                            _upper = _mid - 1;\n"
		"                        } else if " << GET_WIDE_KEY() << " > " << K() << "[_mid] {\n"
		"                            _lower = _mid + 1;\n"
		"                        } else {\n"
		"                            _trans += (_mid - _keys);\n"
		"                            break '_match;\n"
		"                        }\n"
		"                    }\n"
		"                    _keys += _klen;\n"
		"                    _trans += _klen;\n"
		"                }\n"
		"\n"
		"                _klen = " << RL() << "[" << vCS() << "] as uint;\n"
		"                if _klen > 0 {\n"
		"                    let mut _lower = _keys;\n"
		"                    let mut _mid: uint;\n"
		"                    let mut _upper = _keys + (_klen<<1) - 2;\n"
		"                    loop {\n"
		"                        if _upper < _lower { break; }\n"
		"\n"
		"                        _mid = _lower + (((_upper-_lower) >> 1) & !1);\n"
		"                        if " << GET_WIDE_KEY() << " < " << K() << "[_mid] {\n"
		"                            _upper = _mid - 2;\n"
		"                        } else if " << GET_WIDE_KEY() << " > " << K() << "[_mid+1] {\n"
		"                            _lower = _mid + 2;\n"
		"                        } else {\n"
		"                            _trans += ((_mid - _keys)>>1);\n"
		"                            break '_match;\n"
		"                        }\n"
		"                    }\n"
		"                    _trans += _klen;\n"
		"                }\n"
		"                break;\n"
		"            }\n"
		"\n";
}

/* Determine if we should use indicies or not. */
void RustTabCodeGen::calcIndexSize()
{
	int sizeWithInds = 0, sizeWithoutInds = 0;

	/* Calculate cost of using with indicies. */
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		int totalIndex = st->outSingle.length() + st->outRange.length() +
				(st->defTrans == 0 ? 0 : 1);
		sizeWithInds += arrayTypeSize(redFsm->maxIndex) * totalIndex;
	}
	sizeWithInds += arrayTypeSize(redFsm->maxState) * redFsm->transSet.length();
	if ( redFsm->anyActions() )
		sizeWithInds += arrayTypeSize(redFsm->maxActionLoc) * redFsm->transSet.length();

	/* Calculate the cost of not using indicies. */
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		int totalIndex = st->outSingle.length() + st->outRange.length() +
				(st->defTrans == 0 ? 0 : 1);
		sizeWithoutInds += arrayTypeSize(redFsm->maxState) * totalIndex;
		if ( redFsm->anyActions() )
			sizeWithoutInds += arrayTypeSize(redFsm->maxActionLoc) * totalIndex;
	}

	/* If using indicies reduces the size, use them. */
	useIndicies = sizeWithInds < sizeWithoutInds;
}

int RustTabCodeGen::TO_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->toStateAction != 0 )
		act = state->toStateAction->location+1;
	return act;
}

int RustTabCodeGen::FROM_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->fromStateAction != 0 )
		act = state->fromStateAction->location+1;
	return act;
}

int RustTabCodeGen::EOF_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->eofAction != 0 )
		act = state->eofAction->location+1;
	return act;
}


int RustTabCodeGen::TRANS_ACTION( RedTransAp *trans )
{
	/* If there are actions, emit them. Otherwise emit zero. */
	int act = 0;
	if ( trans->action != 0 )
		act = trans->action->location+1;
	return act;
}

std::ostream &RustTabCodeGen::TO_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the matches. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numToStateRefs > 0 ) {
			/* Write the match label and the action. */
			out << "                      " << act->actionId << " => {\n";
			ACTION( out, act, 0, false );
			out << "                      }\n";
		}
	}

	genLineDirective( out );
	return out;
}

std::ostream &RustTabCodeGen::FROM_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the matches. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numFromStateRefs > 0 ) {
			/* Write the match label and the action. */
			out << "                      " << act->actionId << " => {\n";
			ACTION( out, act, 0, false );
			out << "                      }\n";
		}
	}

	genLineDirective( out );
	return out;
}

std::ostream &RustTabCodeGen::EOF_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the matches. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numEofRefs > 0 ) {
			/* Write the match label and the action. */
			out << "                      " << act->actionId << " => {\n";
			ACTION( out, act, 0, true );
			out << "                      }\n";
		}
	}

	genLineDirective( out );
	return out;
}


std::ostream &RustTabCodeGen::ACTION_SWITCH()
{
	/* Walk the list of functions, printing the matches. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numTransRefs > 0 ) {
			/* Write the match label and the action. */
			out << "                      " << act->actionId << " => {\n";
			ACTION( out, act, 0, false );
			out << "                      }\n";
		}
	}

	genLineDirective( out );
	return out;
}

std::ostream &RustTabCodeGen::COND_OFFSETS()
{
	int curKeyOffset = 0;
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Write the key offset. */
		ARRAY_ITEM( INT(curKeyOffset), st.last() );

		/* Move the key offset ahead. */
		curKeyOffset += st->stateCondList.length();
	}
	return out;
}

std::ostream &RustTabCodeGen::KEY_OFFSETS()
{
	int curKeyOffset = 0;
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Write the key offset. */
		ARRAY_ITEM( INT(curKeyOffset), st.last() );

		/* Move the key offset ahead. */
		curKeyOffset += st->outSingle.length() + st->outRange.length()*2;
	}
	return out;
}


std::ostream &RustTabCodeGen::INDEX_OFFSETS()
{
	int curIndOffset = 0;
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Write the index offset. */
		ARRAY_ITEM( INT(curIndOffset), st.last() );

		/* Move the index offset ahead. */
		curIndOffset += st->outSingle.length() + st->outRange.length();
		if ( st->defTrans != 0 )
			curIndOffset += 1;
	}
	return out;
}

std::ostream &RustTabCodeGen::COND_LENS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Write singles length. */
		ARRAY_ITEM( INT(st->stateCondList.length()), st.last() );
	}
	return out;
}


std::ostream &RustTabCodeGen::SINGLE_LENS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Write singles length. */
		ARRAY_ITEM( INT(st->outSingle.length()), st.last() );
	}
	return out;
}

std::ostream &RustTabCodeGen::RANGE_LENS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Emit length of range index. */
		ARRAY_ITEM( INT(st->outRange.length()), st.last() );
	}
	return out;
}

std::ostream &RustTabCodeGen::TO_STATE_ACTIONS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Write any eof action. */
		ARRAY_ITEM( INT(TO_STATE_ACTION(st)), st.last() );
	}
	return out;
}

std::ostream &RustTabCodeGen::FROM_STATE_ACTIONS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Write any eof action. */
		ARRAY_ITEM( INT(FROM_STATE_ACTION(st)), st.last() );
	}
	return out;
}

std::ostream &RustTabCodeGen::EOF_ACTIONS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Write any eof action. */
		ARRAY_ITEM( INT(EOF_ACTION(st)), st.last() );
	}
	return out;
}

std::ostream &RustTabCodeGen::EOF_TRANS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Write any eof action. */
		long trans = 0;
		if ( st->eofTrans != 0 ) {
			assert( st->eofTrans->pos >= 0 );
			trans = st->eofTrans->pos+1;
		}

		/* Write any eof action. */
		ARRAY_ITEM( INT(trans), st.last() );
	}
	return out;
}


std::ostream &RustTabCodeGen::COND_KEYS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Loop the state's transitions. */
		for ( GenStateCondList::Iter sc = st->stateCondList; sc.lte(); sc++ ) {
			/* Lower key. */
			ARRAY_ITEM( KEY( sc->lowKey ), false );
			ARRAY_ITEM( KEY( sc->highKey ), false );
		}
	}

	/* Output one last number so we don't have to figure out when the last
	 * entry is and avoid writing a comma. */
	ARRAY_ITEM( INT(0), true );
	return out;
}

std::ostream &RustTabCodeGen::COND_SPACES()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Loop the state's transitions. */
		for ( GenStateCondList::Iter sc = st->stateCondList; sc.lte(); sc++ ) {
			/* Cond Space id. */
			ARRAY_ITEM( KEY( sc->condSpace->condSpaceId ), false );
		}
	}

	/* Output one last number so we don't have to figure out when the last
	 * entry is and avoid writing a comma. */
	ARRAY_ITEM( INT(0), true );
	return out;
}

std::ostream &RustTabCodeGen::KEYS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Loop the singles. */
		for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
			ARRAY_ITEM( KEY( stel->lowKey ), false );
		}

		/* Loop the state's transitions. */
		for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
			/* Lower key. */
			ARRAY_ITEM( KEY( rtel->lowKey ), false );

			/* Upper key. */
			ARRAY_ITEM( KEY( rtel->highKey ), false );
		}
	}

	/* Output one last number so we don't have to figure out when the last
	 * entry is and avoid writing a comma. */
	ARRAY_ITEM( INT(0), true );
	return out;
}

std::ostream &RustTabCodeGen::INDICIES()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Walk the singles. */
		for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
			ARRAY_ITEM( KEY( stel->value->id ), false );
		}

		/* Walk the ranges. */
		for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
			ARRAY_ITEM( KEY( rtel->value->id ), false );
		}

		/* The state's default index goes next. */
		if ( st->defTrans != 0 ) {
			ARRAY_ITEM( KEY( st->defTrans->id ), false );
		}
	}

	/* Output one last number so we don't have to figure out when the last
	 * entry is and avoid writing a comma. */
	ARRAY_ITEM( INT(0), true );
	return out;
}

std::ostream &RustTabCodeGen::TRANS_TARGS()
{
	int totalTrans = 0;
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Walk the singles. */
		for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
			RedTransAp *trans = stel->value;
			ARRAY_ITEM( KEY( trans->targ->id ), false );
			totalTrans++;
		}

		/* Walk the ranges. */
		for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
			RedTransAp *trans = rtel->value;
			ARRAY_ITEM( KEY( trans->targ->id ), false );
			totalTrans++;
		}

		/* The state's default target state. */
		if ( st->defTrans != 0 ) {
			RedTransAp *trans = st->defTrans;
			ARRAY_ITEM( KEY( trans->targ->id ), false );
			totalTrans++;
		}
	}

	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		if ( st->eofTrans != 0 ) {
			RedTransAp *trans = st->eofTrans;
			trans->pos = totalTrans++;
			ARRAY_ITEM( KEY( trans->targ->id ), false );
		}
	}

	/* Output one last number so we don't have to figure out when the last
	 * entry is and avoid writing a comma. */
	ARRAY_ITEM( INT(0), true );
	return out;
}


std::ostream &RustTabCodeGen::TRANS_ACTIONS()
{
	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		/* Walk the singles. */
		for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
			RedTransAp *trans = stel->value;
			ARRAY_ITEM( INT(TRANS_ACTION( trans )), false );
		}

		/* Walk the ranges. */
		for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
			RedTransAp *trans = rtel->value;
			ARRAY_ITEM( INT(TRANS_ACTION( trans )), false );
		}

		/* The state's default index goes next. */
		if ( st->defTrans != 0 ) {
			RedTransAp *trans = st->defTrans;
			ARRAY_ITEM( INT(TRANS_ACTION( trans )), false );
		}
	}

	for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
		if ( st->eofTrans != 0 ) {
			RedTransAp *trans = st->eofTrans;
			ARRAY_ITEM( INT(TRANS_ACTION( trans )), false );
		}
	}

	/* Output one last number so we don't have to figure out when the last
	 * entry is and avoid writing a comma. */
	ARRAY_ITEM( INT(0), true );
	return out;
}

std::ostream &RustTabCodeGen::TRANS_TARGS_WI()
{
	/* Transitions must be written ordered by their id. */
	RedTransAp **transPtrs = new RedTransAp*[redFsm->transSet.length()];
	for ( TransApSet::Iter trans = redFsm->transSet; trans.lte(); trans++ )
		transPtrs[trans->id] = trans;

	/* Keep a count of the num of items in the array written. */
	for ( int t = 0; t < redFsm->transSet.length(); t++ ) {
		/* Save the position. Needed for eofTargs. */
		RedTransAp *trans = transPtrs[t];
		trans->pos = t;

		/* Write out the target state. */
		ARRAY_ITEM( INT(trans->targ->id), ( t >= redFsm->transSet.length()-1 ) );
	}
	delete[] transPtrs;
	return out;
}


std::ostream &RustTabCodeGen::TRANS_ACTIONS_WI()
{
	/* Transitions must be written ordered by their id. */
	RedTransAp **transPtrs = new RedTransAp*[redFsm->transSet.length()];
	for ( TransApSet::Iter trans = redFsm->transSet; trans.lte(); trans++ )
		transPtrs[trans->id] = trans;

	/* Keep a count of the num of items in the array written. */
	for ( int t = 0; t < redFsm->transSet.length(); t++ ) {
		/* Write the function for the transition. */
		RedTransAp *trans = transPtrs[t];
		ARRAY_ITEM( INT(TRANS_ACTION( trans )), ( t >= redFsm->transSet.length()-1 ) );
	}
	delete[] transPtrs;
	return out;
}

void RustTabCodeGen::writeExports()
{
	if ( exportList.length() > 0 ) {
		for ( ExportList::Iter ex = exportList; ex.lte(); ex++ ) {
			STATIC_VAR( ALPH_TYPE(), DATA_PREFIX() + "ex_" + ex->name )
					<< " = " << KEY(ex->key) << ";\n";
		}
		out << "\n";
	}
}

void RustTabCodeGen::writeStart()
{
	out << START_STATE_ID();
}

void RustTabCodeGen::writeFirstFinal()
{
	out << FIRST_FINAL_STATE();
}

void RustTabCodeGen::writeError()
{
	out << ERROR_STATE();
}

void RustTabCodeGen::writeData()
{
	/* If there are any transtion functions then output the array. If there
	 * are none, don't bother emitting an empty array that won't be used. */
	if ( redFsm->anyActions() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActArrItem), A() );
		ACTIONS_ARRAY();
		CLOSE_ARRAY() <<
		"\n";
	}

	if ( redFsm->anyConditions() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxCondOffset), CO() );
		COND_OFFSETS();
		CLOSE_ARRAY() <<
		"\n";

		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxCondLen), CL() );
		COND_LENS();
		CLOSE_ARRAY() <<
		"\n";

		OPEN_ARRAY( WIDE_ALPH_TYPE(), CK() );
		COND_KEYS();
		CLOSE_ARRAY() <<
		"\n";

		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxCondSpaceId), C() );
		COND_SPACES();
		CLOSE_ARRAY() <<
		"\n";
	}

	OPEN_ARRAY( ARRAY_TYPE(redFsm->maxKeyOffset), KO() );
	KEY_OFFSETS();
	CLOSE_ARRAY() <<
	"\n";

	OPEN_ARRAY( WIDE_ALPH_TYPE(), K() );
	KEYS();
	CLOSE_ARRAY() <<
	"\n";

	OPEN_ARRAY( ARRAY_TYPE(redFsm->maxSingleLen), SL() );
	SINGLE_LENS();
	CLOSE_ARRAY() <<
	"\n";

	OPEN_ARRAY( ARRAY_TYPE(redFsm->maxRangeLen), RL() );
	RANGE_LENS();
	CLOSE_ARRAY() <<
	"\n";

	OPEN_ARRAY( ARRAY_TYPE(redFsm->maxIndexOffset), IO() );
	INDEX_OFFSETS();
	CLOSE_ARRAY() <<
	"\n";

	if ( useIndicies ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxIndex), I() );
		INDICIES();
		CLOSE_ARRAY() <<
		"\n";

		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxState), TT() );
		TRANS_TARGS_WI();
		CLOSE_ARRAY() <<
		"\n";

		if ( redFsm->anyActions() ) {
			OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActionLoc), TA() );
			TRANS_ACTIONS_WI();
			CLOSE_ARRAY() <<
			"\n";
		}
	}
	else {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxState), TT() );
		TRANS_TARGS();
		CLOSE_ARRAY() <<
		"\n";

		if ( redFsm->anyActions() ) {
			OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActionLoc), TA() );
			TRANS_ACTIONS();
			CLOSE_ARRAY() <<
			"\n";
		}
	}

	if ( redFsm->anyToStateActions() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActionLoc), TSA() );
		TO_STATE_ACTIONS();
		CLOSE_ARRAY() <<
		"\n";
	}

	if ( redFsm->anyFromStateActions() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActionLoc), FSA() );
		FROM_STATE_ACTIONS();
		CLOSE_ARRAY() <<
		"\n";
	}

	if ( redFsm->anyEofActions() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxActionLoc), EA() );
		EOF_ACTIONS();
		CLOSE_ARRAY() <<
		"\n";
	}

	if ( redFsm->anyEofTrans() ) {
		OPEN_ARRAY( ARRAY_TYPE(redFsm->maxIndexOffset+1), ET() );
		EOF_TRANS();
		CLOSE_ARRAY() <<
		"\n";
	}

	if ( redFsm->startState != 0 )
		STATIC_VAR( "uint", START() ) << " = " << START_STATE_ID() << ";\n";

	if ( !noFinal )
		STATIC_VAR( "uint" , FIRST_FINAL() ) << " = " << FIRST_FINAL_STATE() << ";\n";

	if ( !noError )
		STATIC_VAR( "uint", ERROR() ) << " = " << ERROR_STATE() << ";\n";
	
	out << "\n";

	if ( entryPointNames.length() > 0 ) {
		for ( EntryNameVect::Iter en = entryPointNames; en.lte(); en++ ) {
			STATIC_VAR( "uint", DATA_PREFIX() + "en_" + *en ) <<
					" = " << entryPointIds[en.pos()] << ";\n";
		}
		out << "\n";
	}
}

void RustTabCodeGen::writeExec()
{
	out <<
		"{\n";

	for ( int i = 0; i < array_inits.size(); ++i) {
		out << "    " << array_inits[i] << "\n";
	}

  out <<
		"    let mut _klen: uint;\n";

	if ( redFsm->anyRegCurStateRef() )
		out << "    let _ps: uint;\n";

	out << "    let mut _trans = 0;\n";

	if ( redFsm->anyConditions() )
		out << "    let _widec: uint;\n";

	if ( redFsm->anyToStateActions() || redFsm->anyRegActions() ||
			redFsm->anyFromStateActions() )
	{
		out <<
			"    let mut _acts: uint;\n"
			"    let mut _nacts: uint;\n";
	}

	out <<
		"    let mut _keys: uint;\n"
		"    let mut _goto_targ: uint = 0;\n"
		"\n";
	
	out <<
		"    '_goto: loop {\n"
		"        match _goto_targ {\n"
		"          0 => {\n";

	if ( !noEnd ) {
		out <<
			"            if " << P() << " == " << PE() << " {\n"
			"                _goto_targ = " << _test_eof << ";\n"
			"                continue '_goto;\n"
			"            }\n";
	}

	if ( redFsm->errState != 0 ) {
		out <<
			"            if " << vCS() << " == " << redFsm->errState->id << " {\n"
			"                _goto_targ = " << _out << ";\n"
			"                continue '_goto;\n"
			"            }\n";
	}

	out <<
		"            _goto_targ = " << _resume << ";\n"
		"            continue '_goto;\n"
		"          }\n"
		"          " << _resume << " => {\n";

	if ( redFsm->anyFromStateActions() ) {
		out <<
			"            _acts = " << FSA() << "[" << vCS() << "] as uint;\n"
			"            _nacts = " << A() << "[_acts] as uint;\n"
			"            _acts += 1;\n"
			"            while _nacts > 0 {\n"
			"                _nacts -= 1;\n"
			"                let __acts = _acts;\n"
			"                _acts += 1;\n"
			"                match " << A() << "[__acts] {\n";
			FROM_STATE_ACTION_SWITCH() <<
			"                    _ => panic!(),\n"
			"                }\n"
			"            }\n"
			"\n";
	}

	if ( redFsm->anyConditions() )
		COND_TRANSLATE();

	LOCATE_TRANS();

	if ( useIndicies )
		out << "            _trans = " << I() << "[_trans] as uint;\n";
	
	if ( redFsm->anyEofTrans() )
		out <<
			"            _goto_targ = " << _eof_trans << ";\n"
			"            continue;\n"
			"          }\n"
			"          " << _eof_trans << " => {\n";

	if ( redFsm->anyRegCurStateRef() )
		out << "            _ps = " << vCS() << ";\n";

	out <<
		"            " << vCS() << " = " << TT() << "[_trans] as uint;\n"
		"\n";

	if ( redFsm->anyRegActions() ) {
		out <<
			"            if " << TA() << "[_trans] != 0 {\n"
			"                _acts = " <<  TA() << "[_trans] as uint;\n"
			"                _nacts = " << A() << "[_acts] as uint;\n"
			"                _acts += 1;\n"
			"                while _nacts > 0 {\n"
			"                    _nacts -= 1;\n"
			"                    let __acts = _acts;\n"
			"                    _acts += 1;\n"
			"                    match " << A() << "[__acts] {\n";
		ACTION_SWITCH() <<
			      "                        _ => panic!(),"
			"                    }\n"
			"                }\n"
			"            }\n"
			"\n";
	}

	out <<
		"          _goto_targ = " << _again << ";\n"
		"          continue;\n"
		"        }\n"
		"        " << _again << " => {\n";

	if ( redFsm->anyToStateActions() ) {
		out <<
			"          _acts = " << TSA() << "[" << vCS() << "] as uint;\n"
			"          _nacts = " << A() << "[_acts] as uint;\n"
			"          _acts += 1;\n"
			"          while _nacts > 0 {\n"
			"              _nacts -= 1;\n"
			"              let __acts = _acts;\n"
			"              _acts += 1;\n"
			"              match " << A() << "[__acts] {\n";
		TO_STATE_ACTION_SWITCH() <<
			"                  _ => panic!(),"
			"              }\n"
			"          }\n"
			"\n";
	}

	if ( redFsm->errState != 0 ) {
		out <<
			"          if " << vCS() << " == " << redFsm->errState->id << " {\n"
			"              _goto_targ = " << _out << ";\n"
			"              continue '_goto;\n"
			"          }\n";
	}

	if ( !noEnd ) {
		out <<
			"          " << P() << " += 1;\n"
			"          if " << P() << " != " << PE() << " {\n"
			"              _goto_targ = " << _resume << ";\n"
			"              continue '_goto;\n"
			"          }\n"
			"        _goto_targ = " << _test_eof << ";\n"
			"        continue '_goto;\n";
	}
	else {
		out <<
			"          " << P() << " += 1;\n"
			"          _goto_targ = " << _resume << ";\n"
			"          continue '_goto;\n";
	}

	out <<
		"        }\n";

	if ( redFsm->anyEofTrans() || redFsm->anyEofActions() ) {
		out <<
			"        " << _test_eof << " => {\n"
			"            if " << P() << " == " << vEOF() << " {\n";

		if ( redFsm->anyEofTrans() ) {
			out <<
				"                if " << ET() << "[" << vCS() << "] > 0 {\n"
				"                    _trans = (" << ET() << "[" << vCS() << "] - 1) as uint;\n"
				"                    _goto_targ = " << _eof_trans << ";\n"
				"                    continue '_goto;\n"
				"                }\n";
		}

		if ( redFsm->anyEofActions() ) {
			out <<
				"                let mut __acts = " << EA() << "[" << vCS() << "]" << " as uint;\n"
				"                let mut __nacts = " << A() << "[__acts] as uint;\n"
				"                __acts += 1;\n"
				"                while __nacts > 0 {\n"
				"                    __nacts -= 1;\n"
				"                    let ___acts = __acts;\n"
				"                    __acts += 1;\n"
				"                    match " << A() << "[___acts] {\n";
			EOF_ACTION_SWITCH() <<
	  			"                        _ => panic!(),"
				"                    }\n"
				"                }\n";
		}

		out <<
			"            }\n"
			"        }\n"
			"\n";
	} else {
		out <<
			"        " << _test_eof << " => { }\n";
  }

	out <<
		"        " << _out << " => { }\n"
		"        _ => panic!(),";

	/* The switch and goto loop. */
	out <<
		"      }\n"
		"      break;\n"
		"    }\n";

	/* The execute block. */
	out << "}\n";
}

std::ostream &RustTabCodeGen::OPEN_ARRAY( string type, string name )
{
	array_type = type;
	array_name = name;
	item_count = 0;
	div_count = 1;

	out <<
		"static " << name << ": &'static [" << type << "] = &[\n";

	return out;
}

std::ostream &RustTabCodeGen::ARRAY_ITEM( string item, bool last )
{
	item_count++;

	out << setw(5) << setiosflags(ios::right) << item;
	
	if ( !last ) {
		if (item_count % IALL == 0) {
			out << ",\n";
		} else {
			out << ",";
		}
	}
	return out;
}

std::ostream &RustTabCodeGen::CLOSE_ARRAY()
{
	out <<
		"\n"
		"];\n";

	return out;
}


std::ostream &RustTabCodeGen::STATIC_VAR( string type, string name )
{
	out <<
		"#[allow(dead_code)]\n" <<
		"static " << name << ": " << type;
	return out;
}

string RustTabCodeGen::ARR_OFF( string ptr, string offset )
{
	return ptr + " + " + offset;
}

string RustTabCodeGen::NULL_ITEM()
{
	/* In rust we use integers instead of pointers. */
	return "-1";
}

string RustTabCodeGen::GET_KEY()
{
	ostringstream ret;
	if ( getKeyExpr != 0 ) {
		/* Emit the user supplied method of retrieving the key. */
		ret << "(";
		INLINE_LIST( ret, getKeyExpr, 0, false );
		ret << ")";
	}
	else {
		/* Expression for retrieving the key, use simple dereference. */
		ret << DATA() << ".as_bytes()[" << P() << "]";
	}
	return ret.str();
}

string RustTabCodeGen::CTRL_FLOW()
{
	return "";
}

unsigned int RustTabCodeGen::arrayTypeSize( unsigned long maxVal )
{
	long long maxValLL = (long long) maxVal;
	HostType *arrayType = keyOps->typeSubsumes( maxValLL );
	assert( arrayType != 0 );
	return arrayType->size;
}

string RustTabCodeGen::ARRAY_TYPE( unsigned long maxVal )
{
	long long maxValLL = (long long) maxVal;
	HostType *arrayType = keyOps->typeSubsumes( maxValLL );
	assert( arrayType != 0 );

	string ret = arrayType->data1;
	if ( arrayType->data2 != 0 ) {
		ret += " ";
		ret += arrayType->data2;
	}
	return ret;
}


/* Write out the fsm name. */
string RustTabCodeGen::FSM_NAME()
{
	return fsmName;
}

/* Emit the offset of the start state as a decimal integer. */
string RustTabCodeGen::START_STATE_ID()
{
	ostringstream ret;
	ret << redFsm->startState->id;
	return ret.str();
};

/* Write out the array of actions. */
std::ostream &RustTabCodeGen::ACTIONS_ARRAY()
{
	ARRAY_ITEM( INT(0), false );
	for ( GenActionTableMap::Iter act = redFsm->actionMap; act.lte(); act++ ) {
		/* Write out the length, which will never be the last character. */
		ARRAY_ITEM( INT(act->key.length()), false );

		for ( GenActionTable::Iter item = act->key; item.lte(); item++ )
			ARRAY_ITEM( INT(item->value->actionId), (act.last() && item.last()) );
	}
	return out;
}


string RustTabCodeGen::ACCESS()
{
	ostringstream ret;
	if ( accessExpr != 0 )
		INLINE_LIST( ret, accessExpr, 0, false );
	return ret.str();
}

string RustTabCodeGen::P()
{
	ostringstream ret;
	if ( pExpr == 0 )
		ret << "p";
	else {
		ret << "(";
		INLINE_LIST( ret, pExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}

string RustTabCodeGen::PE()
{
	ostringstream ret;
	if ( peExpr == 0 )
		ret << "pe";
	else {
		ret << "(";
		INLINE_LIST( ret, peExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}

string RustTabCodeGen::vEOF()
{
	ostringstream ret;
	if ( eofExpr == 0 )
		ret << "eof";
	else {
		ret << "(";
		INLINE_LIST( ret, eofExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}

string RustTabCodeGen::vCS()
{
	ostringstream ret;
	if ( csExpr == 0 )
		ret << ACCESS() << "cs";
	else {
		/* Emit the user supplied method of retrieving the key. */
		ret << "(";
		INLINE_LIST( ret, csExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}

string RustTabCodeGen::TOP()
{
	ostringstream ret;
	if ( topExpr == 0 )
		ret << ACCESS() + "top";
	else {
		ret << "(";
		INLINE_LIST( ret, topExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}

string RustTabCodeGen::STACK()
{
	ostringstream ret;
	if ( stackExpr == 0 )
		ret << ACCESS() + "stack";
	else {
		ret << "(";
		INLINE_LIST( ret, stackExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}

string RustTabCodeGen::ACT()
{
	ostringstream ret;
	if ( actExpr == 0 )
		ret << ACCESS() + "act";
	else {
		ret << "(";
		INLINE_LIST( ret, actExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}

string RustTabCodeGen::TOKSTART()
{
	ostringstream ret;
	if ( tokstartExpr == 0 )
		ret << ACCESS() + "ts";
	else {
		ret << "(";
		INLINE_LIST( ret, tokstartExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}

string RustTabCodeGen::TOKEND()
{
	ostringstream ret;
	if ( tokendExpr == 0 )
		ret << ACCESS() + "te";
	else {
		ret << "(";
		INLINE_LIST( ret, tokendExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}

string RustTabCodeGen::DATA()
{
	ostringstream ret;
	if ( dataExpr == 0 )
		ret << ACCESS() + "data";
	else {
		ret << "(";
		INLINE_LIST( ret, dataExpr, 0, false );
		ret << ")";
	}
	return ret.str();
}


string RustTabCodeGen::GET_WIDE_KEY()
{
	if ( redFsm->anyConditions() )
		return "_widec";
	else
		return GET_KEY();
}

string RustTabCodeGen::GET_WIDE_KEY( RedStateAp *state )
{
	if ( state->stateCondList.length() > 0 )
		return "_widec";
	else
		return GET_KEY();
}

/* Write out level number of tabs. Makes the nested binary search nice
 * looking. */
string RustTabCodeGen::TABS( int level )
{
	string result;
	while ( level-- > 0 )
		result += "    ";
	return result;
}

string RustTabCodeGen::KEY( Key key )
{
	ostringstream ret;
	if ( keyOps->isSigned || !hostLang->explicitUnsigned )
		ret << key.getVal();
	else
		ret << (unsigned long) key.getVal();
	return ret.str();
}

string RustTabCodeGen::INT( int i )
{
	ostringstream ret;
	ret << i;
	return ret.str();
}

void RustTabCodeGen::LM_SWITCH( ostream &ret, GenInlineItem *item,
		int targState, int inFinish )
{
	ret <<
		"    match " << ACT() << " {\n";

	for ( GenInlineList::Iter lma = *item->children; lma.lte(); lma++ ) {
		/* Write the match label and the action. */
		if ( lma->lmId < 0 )
			ret << "      _ => {\n";
		else
			ret << "      " << lma->lmId << " => {\n";

		/* Write the block and close it off. */
		ret << "        {";
		INLINE_LIST( ret, lma->children, targState, inFinish );
		ret << "        }\n";

		ret << "      }\n";
	}

  ret <<
	"        _ => panic!()";

	ret <<
		"    }\n"
		"    ";
}

void RustTabCodeGen::SET_ACT( ostream &ret, GenInlineItem *item )
{
	ret << ACT() << " = " << item->lmId << ";";
}

void RustTabCodeGen::SET_TOKEND( ostream &ret, GenInlineItem *item )
{
	/* The tokend action sets tokend. */
	ret << TOKEND() << " = " << P();
	if ( item->offset != 0 )
		out << "+" << item->offset;
	out << ";";
}

void RustTabCodeGen::GET_TOKEND( ostream &ret, GenInlineItem *item )
{
	ret << TOKEND();
}

void RustTabCodeGen::INIT_TOKSTART( ostream &ret, GenInlineItem *item )
{
	ret << TOKSTART() << " = " << NULL_ITEM() << ";";
}

void RustTabCodeGen::INIT_ACT( ostream &ret, GenInlineItem *item )
{
	ret << ACT() << " = 0;";
}

void RustTabCodeGen::SET_TOKSTART( ostream &ret, GenInlineItem *item )
{
	ret << TOKSTART() << " = " << P() << ";";
}

void RustTabCodeGen::SUB_ACTION( ostream &ret, GenInlineItem *item,
		int targState, bool inFinish )
{
	if ( item->children->length() > 0 ) {
		/* Write the block and close it off. */
		ret << "{";
		INLINE_LIST( ret, item->children, targState, inFinish );
		ret << "}";
	}
}

void RustTabCodeGen::ACTION( ostream &ret, GenAction *action, int targState, bool inFinish )
{
	/* Write the preprocessor line info for going into the source file. */
	rustLineDirective( ret, action->loc.fileName, action->loc.line );

	/* Write the block and close it off. */
	ret << "    {";
	INLINE_LIST( ret, action->inlineList, targState, inFinish );
	ret << "}\n";
}

void RustTabCodeGen::CONDITION( ostream &ret, GenAction *condition )
{
	ret << "\n";
	rustLineDirective( ret, condition->loc.fileName, condition->loc.line );
	INLINE_LIST( ret, condition->inlineList, 0, false );
}

string RustTabCodeGen::ERROR_STATE()
{
	ostringstream ret;
	if ( redFsm->errState != 0 )
		ret << redFsm->errState->id;
	else
		ret << "-1";
	return ret.str();
}

string RustTabCodeGen::FIRST_FINAL_STATE()
{
	ostringstream ret;
	if ( redFsm->firstFinState != 0 )
		ret << redFsm->firstFinState->id;
	else
		ret << redFsm->nextStateId;
	return ret.str();
}

void RustTabCodeGen::writeInit()
{
	out << "    {\n";

	if ( !noCS )
		out << "        " << vCS() << " = " << START() << ";\n";
	
	/* If there are any calls, then the stack top needs initialization. */
	if ( redFsm->anyActionCalls() || redFsm->anyActionRets() )
		out << "        " << TOP() << " = 0;\n";

	if ( hasLongestMatch ) {
		out <<
			"        " << TOKSTART() << " = " << NULL_ITEM() << ";\n"
			"        " << TOKEND() << " = " << NULL_ITEM() << ";\n"
			"        " << ACT() << " = 0;\n";
	}
	out << "    }\n";
}

void RustTabCodeGen::finishRagelDef()
{
	/* The frontend will do this for us, but it may be a good idea to force it
	 * if the intermediate file is edited. */
	redFsm->sortByStateId();

	/* Choose default transitions and the single transition. */
	redFsm->chooseDefaultSpan();
		
	/* Maybe do flat expand, otherwise choose single. */
	redFsm->chooseSingle();

	/* If any errors have occured in the input file then don't write anything. */
	if ( gblErrorCount > 0 )
		return;
	
	/* Anlayze Machine will find the final action reference counts, among
	 * other things. We will use these in reporting the usage
	 * of fsm directives in action code. */
	analyzeMachine();

	/* Determine if we should use indicies. */
	calcIndexSize();
}

ostream &RustTabCodeGen::source_warning( const InputLoc &loc )
{
	cerr << sourceFileName << ":" << loc.line << ":" << loc.col << ": warning: ";
	return cerr;
}

ostream &RustTabCodeGen::source_error( const InputLoc &loc )
{
	gblErrorCount += 1;
	assert( sourceFileName != 0 );
	cerr << sourceFileName << ":" << loc.line << ":" << loc.col << ": ";
	return cerr;
}


