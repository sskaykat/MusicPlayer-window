﻿// Scintilla source code edit control
/** @file Catalogue.cxx
 ** Lexer infrastructure.
 ** Contains a list of LexerModules which can be searched to find a module appropriate for a
 ** particular language.
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cstdlib>
#include <cassert>
#include <cstring>

#include <stdexcept>
#include <vector>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "LexerModule.h"
#include "CatalogueModules.h"
#include "Catalogue.h"

using namespace Scintilla;

namespace {

CatalogueModules catalogueDefault;

}

const LexerModule *Catalogue::Find(int language) {
	return catalogueDefault.Find(language);
}

const LexerModule *Catalogue::Find(const char *languageName) noexcept {
	return catalogueDefault.Find(languageName);
}

void Catalogue::AddLexerModule(LexerModule *plm) {
	catalogueDefault.AddLexerModule(plm);
}

// To add or remove a lexer, add or remove its file and run LexGen.py.

// Force a reference to all of the Scintilla lexers so that the linker will
// not remove the code of the lexers.
int Scintilla_LinkLexers() {

	static int initialised = 0;
	if (initialised)
		return 0;
	initialised = 1;

#if !defined(SCI_EMPTYCATALOGUE)

// Shorten the code that declares a lexer and ensures it is linked in by calling a method.
#define LINK_LEXER(lexer) extern LexerModule lexer; catalogueDefault.AddLexerModule(&lexer);

//++Autogenerated -- run scripts/LexGen.py to regenerate
//**\(\tLINK_LEXER(\*);\n\)
	LINK_LEXER(lmNull);

//--Autogenerated -- end of automatically generated section

#endif

	return 1;
}