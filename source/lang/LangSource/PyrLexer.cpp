/*
	SuperCollider real time audio synthesis system
    Copyright (c) 2002 James McCartney. All rights reserved.
	http://www.audiosynth.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <new.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef SC_WIN32
//# include <wx/wx.h>
# include <direct.h>
#else
# include <sys/param.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef SC_WIN32
#else
# include "lang11d_tab.h"
#endif
#include "SCBase.h"
#include "PyrObject.h"
#include "PyrObjectProto.h"
#include "PyrLexer.h"
#include "PyrSched.h"
#include "PyrParseNode.h"
#include "SC_InlineUnaryOp.h"
#include "SC_InlineBinaryOp.h"
#include "GC.h"
#include "SimpleStack.h"

#include "PyrSymbolTable.h"
#include "PyrInterpreter.h"
#include "PyrPrimitive.h"
#include "PyrObjectProto.h"
#include "PyrPrimitiveProto.h"
#include "PyrKernelProto.h"
#include "SC_InlineUnaryOp.h"
#include "SC_InlineBinaryOp.h"
#include "InitAlloc.h"
#include "bullet.h"
#include "PredefinedSymbols.h"
#ifdef SC_WIN32
#else
# include "dirent.h"
#endif
#include <string.h>

#include "SC_LibraryConfig.h"


#ifdef SC_WIN32
# include <stdio.h>
# define MAXPATHLEN _MAX_PATH
# include "erase-compiler/lang11d_tab.h"
#endif

int yyparse();
int processaccidental1(char *s);
int processaccidental2(char *s);


extern bool gFullyFunctional;
double compileStartTime;
int gNumCompiledFiles;
/*
thisProcess.interpreter.executeFile("Macintosh HD:score").size.postln;
*/

#ifdef SC_LINUX
# define ENABLE_LIBRARY_CONFIGURATOR 1
#else
# undef ENABLE_LIBRARY_CONFIGURATOR
#endif // SC_LINUX

PyrSymbol *gCompilingFileSym = 0;
VMGlobals *gCompilingVMGlobals = 0;
char gCompileDir[MAXPATHLEN];
char gSystemExtensionDir[MAXPATHLEN];
char gUserExtensionDir[MAXPATHLEN];

//#define DEBUGLEX 1
bool gDebugLexer = false;

bool gShowWarnings = false;
LongStack brackets;
LongStack closedFuncCharNo;
LongStack generatorStack;
int lastClosedFuncCharNo = 0;

char *binopchars = "!@%&*-+=|<>?/";
char yytext[MAXYYLEN];
char curfilename[PATH_MAX];

int yylen;
int lexCmdLine = 0;
bool compilingCmdLine = false;
bool compilingCmdLineErrorWindow = false;
long zzval;

int lineno, charno, linepos;
int *linestarts;
int maxlinestarts;

char *text;
int textlen;
int textpos;
int parseFailed = 0;
bool compiledOK = false;
int radixcharpos, decptpos;


/* so the text editor's dumb paren matching will work */
#define OPENPAREN '('
#define OPENCURLY '{'
#define OPENSQUAR '['
#define CLOSSQUAR ']'
#define CLOSCURLY '}'
#define CLOSPAREN ')'

int rtf2txt(char* txt);

extern void asRelativePath(char *inPath, char *outPath)
{
	uint32 len = strlen(gCompileDir);
	if (strlen(inPath) < len || memcmp(inPath, gCompileDir, len) != 0) {
		// gCompileDir is not the prefix.
		strcpy(outPath, inPath);
		return;
	}
    strcpy(outPath, inPath +  len);
}


bool getFileText(char* filename, char **text, int *length);
bool getFileText(char* filename, char **text, int *length)
{
	FILE *file;
        char *ltext;
        int llength;
	
#ifdef SC_WIN32
	file = fopen(filename, "rb");
#else
	file = fopen(filename, "r");
#endif
  if (!file) return false;
        
        fseek(file, 0L, SEEK_END);
        llength = ftell(file);
        fseek(file, 0L, SEEK_SET);
        ltext = (char*)pyr_pool_compile->Alloc((llength+1) * sizeof(char));
#ifdef SC_WIN32
        // win32 isprint( ) doesn't like the 0xcd after the end of file when
        // there is a mismatch in lengths due to line endings....
        memset(ltext,0,(llength+1) * sizeof(char));
#endif //SC_WIN32
        MEMFAIL(ltext);
#ifdef SC_WIN32
        size_t size = fread(ltext, 1, llength, file);
        if (size != llength)
          ::MessageBox(NULL,"size != llength", "Error", MB_OK);
#else
        fread(ltext, 1, llength, file);
#endif
        ltext[llength] = 0;
        //ltext[llength] = 0;
        *length = llength;
        fclose(file);
        *text = ltext;
        return true;
}


int bugctr = 0;

int stripNonAscii(char *txt)
{
    int rdpos=0, wrpos=0; 
	int c = 0;
	do {
		c = txt[rdpos++];
    if (isprint(c) || isspace(c) || c == 0) 
      txt[wrpos++] = c;
  } while(c);
	return wrpos;
}

// strips out all the RichTextFile crap
int rtf2txt(char* txt)  
{
    int rdpos=0, wrpos=0; 
    char c; 
    if (strncmp(txt,"{\\rtf",5)!=0) return 0;  // OK, not an RTF file
text: 
    switch (txt[wrpos]=txt[rdpos++]) 
            {
            case 0: 
                /*{
                    char fname[32];
                    sprintf(fname, "rtf2txt_out%d.txt", bugctr++);
                    FILE *fp = fopen(fname, "w");
                    fwrite(txt,wrpos,1,fp);
                    fclose(fp);
                }*/
                return wrpos; 
            case OPENCURLY: 
            case CLOSCURLY: 
            case '\n': goto text; 
            case '\\': 
                if (strncmp(txt+rdpos,"fonttbl",7)==0 
                        || strncmp(txt+rdpos,"filetbl",7)==0
                        || strncmp(txt+rdpos,"colortbl",8)==0
                        || strncmp(txt+rdpos,"stylesheet",10)==0
                    ) 
                {
                    int level = 1;
                    while(level && (c=txt[rdpos++]) != 0) {
                        if (c == OPENCURLY) level++;
                        else if (c == CLOSCURLY) level--;
                    }
                } else {
                    if (txt[rdpos]==CLOSCURLY || txt[rdpos]==OPENCURLY 
                            || txt[rdpos]=='\\' || txt[rdpos]=='\t'|| txt[rdpos]=='\n') 
                        { txt[wrpos++] = txt[rdpos++]; goto text; }
                    if (strncmp(txt+rdpos,"tab",3)==0) { txt[wrpos++] = '\t'; }
                    if (strncmp(txt+rdpos,"par",3)==0) { txt[wrpos++] = '\n'; }

                    while((c=txt[rdpos++]) && c!=' ' && c!='\\'); 
                    if (c=='\\') rdpos--; 
                }
                goto text;
        default : 
            wrpos++;
            goto text;
    }
}




bool startLexer(char* filename) 
{	
	if (!getFileText(filename, &text, &textlen)) return false;
	
	rtf2txt(text);
	textlen = stripNonAscii(text);
	
	initLongStack(&brackets);
	initLongStack(&closedFuncCharNo);
	initLongStack(&generatorStack);
	lastClosedFuncCharNo = 0;
	textpos = 0;
	linepos = 0;
	lineno = 1;
	charno = 0;
	
	yylen = 0;
	zzval = 0;
	parseFailed = 0;
	lexCmdLine = 0;
	strcpy(curfilename, filename);
	maxlinestarts = 1000;
	linestarts = (int*)pyr_pool_compile->Alloc(maxlinestarts * sizeof(int*));
	linestarts[0] = 0;
	linestarts[1] = 0;
	
	//postfl("<startLexer\n");
	return true;
}

void startLexerCmdLine(char *textbuf, int textbuflen)
{
	// pyrmalloc: 
	// lifetime: kill after compile. (this one gets killed anyway)
	text = (char*)pyr_pool_compile->Alloc((textbuflen+2) * sizeof(char));
	MEMFAIL(text);
	memcpy(text, textbuf, textbuflen);
	text[textbuflen] = ' ';
	text[textbuflen+1] = 0;
	textlen = textbuflen + 1;
	
	rtf2txt(text);
	textlen = stripNonAscii(text);
	
	//postfl("text '%s' %d\n", text, text);
		
	initLongStack(&brackets);
	initLongStack(&closedFuncCharNo);
	initLongStack(&generatorStack);
	lastClosedFuncCharNo = 0;
	textpos = 0;
	linepos = 0;
	lineno = 1;
	charno = 0;
	
	yylen = 0;
	zzval = 0;
	parseFailed = 0;
	lexCmdLine = 1;
	strcpy(curfilename, "selected text");
	maxlinestarts = 1000;
	linestarts = (int*)pyr_pool_compile->Alloc(maxlinestarts * sizeof(int*));
	linestarts[0] = 0;
	linestarts[1] = 0;
}

void finiLexer() 
{
	pyr_pool_compile->Free(linestarts);
	pyr_pool_compile->Free(text);
	freeLongStack(&brackets);
	freeLongStack(&closedFuncCharNo);
	freeLongStack(&generatorStack);
}

void initLexer() 
{
	//strcpy(binopchars, "!@%&*-+=|:<>?/");
}

int input() 
{
	int c;
	if (textpos >= textlen) {
		c = 0;
	} else {
		c = text[textpos++];
		charno++;
	}
	if (c == '\n' || c == '\r') {
		lineno++;
		linepos = textpos;
		if (linestarts) {
			if (lineno >= maxlinestarts) {
				maxlinestarts += maxlinestarts;
				linestarts = (int*)pyr_pool_compile->Realloc(
					linestarts,  maxlinestarts * sizeof(int*));
			}
			linestarts[lineno] = linepos;
		}
		charno = 0;
	}
	if (c != 0 && yylen < MAXYYLEN-2) yytext[yylen++] = c;
	//if (gDebugLexer) postfl("input '%c' %d\n",c,c);
	return c;
}

int input0() 
{
	int c;
	if (textpos >= textlen) {
		c = 0;
		textpos++; // so unput will work properly
	} else {
		c = text[textpos++];
		charno++;
	}
	if (c == '\n' || c == '\r') {
		lineno++;
		linepos = textpos;
		if (linestarts) {
			if (lineno >= maxlinestarts) {
				maxlinestarts += maxlinestarts;
				linestarts = (int*)pyr_pool_compile->Realloc(
					linestarts,  maxlinestarts * sizeof(int*));
			}
			linestarts[lineno] = linepos;
		}
		charno = 0;
	}
	//if (gDebugLexer) postfl("input0 '%c' %d\n",c,c);
	return c;
}

void unput(int c) 
{
	if (textpos>0) textpos--;
	if (c) {
		if (yylen) --yylen;
		if (charno) --charno;
		if (c == '\n' || c == '\r') {
			--lineno;
		}
	}
}

void unput0(int c) 
{
	if (textpos>0) textpos--;
	if (charno) --charno;
	if (c == '\n' || c == '\r') {
		--lineno;
	}
}

int yylex() 
{
	int r, c, c2, d;
    char extPath[MAXPATHLEN]; // for error reporting

	yylen = 0;
	// finite state machine to parse input stream into tokens

	if (lexCmdLine == 1) {
		lexCmdLine = 2;
		r = INTERPRET;
		goto leave;
	}
start:
	c = input();
	
	if (c == 0)   { r = 0; goto leave; }
	else if (c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='\v' || c=='\f') {
		yylen = 0;
		goto start;
	}
	else if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c == '_') goto ident;
	else if (c == '/') {
		c = input();
		if (c == '/') goto comment1;
		else if (c == '*') goto comment2;
		else { unput(c); goto binop; }
	}
	else if (c >= '0' && c <= '9') goto digits_1;
	else if (c == OPENPAREN || c == OPENSQUAR || c == OPENCURLY) {
		pushls(&brackets, (int)c);
		if (c == OPENCURLY) {
			pushls(&closedFuncCharNo, linestarts[lineno] + charno - 1);
		}
		r = c;
		goto leave;
	}
	else if (c == CLOSSQUAR) { 
		if (!emptyls(&brackets)) {
			if ((d = popls(&brackets)) != OPENSQUAR) {
				fatal();
				post("opening bracket was a '%c', but found a '%c'\n",d,c);
				goto error2;
			}
		} else {
			fatal();
			post("unmatched '%c'\n",c);
			goto error2;
		}
		r = c; 
		goto leave; 
	}
	else if (c == CLOSPAREN) { 
		if (!emptyls(&brackets)) {
			if ((d = popls(&brackets)) != OPENPAREN) {
				fatal();
				post("opening bracket was a '%c', but found a '%c'\n",d,c);
				goto error2;
			}
		} else {
			fatal();
			post("unmatched '%c'\n",c);
			goto error2;
		}
		r = c; 
		goto leave; 
	}
	else if (c == CLOSCURLY) { 
		if (!emptyls(&brackets)) {
			if ((d = popls(&brackets)) != OPENCURLY) {
				fatal();
				post("opening bracket was a '%c', but found a '%c'\n",d,c);
				goto error2;
			}
			lastClosedFuncCharNo = popls(&closedFuncCharNo);
		} else {
			fatal();
			post("unmatched '%c'\n",c);
			goto error2;
		}
		r = c; 
		goto leave; 
	}
	else if (c == '^') { r = c; goto leave; }
	else if (c == '~') { r = c; goto leave; }
	else if (c == ';') { r = c; goto leave; }
	else if (c == ':') { r = c; goto leave; }
	else if (c == '`') { r = c; goto leave; }
	else if (c == '\\') goto symbol1;
	else if (c == '\'') goto symbol3;
	else if (c == '"') goto string1;
	else if (c == '.') { 
		if ((c = input()) == '.') {
			if ((c = input()) == '.') {
				r = ELLIPSIS;
				goto leave; 
			} else {
				r = DOTDOT;
				unput(c); 
				goto leave; 
			}
		} else {
			unput(c); 
			r = '.';
			goto leave; 
		}
			
	}
	else if (c == '#') { 
		if ((c = input()) == OPENCURLY) {
			pushls(&brackets, OPENCURLY);
			pushls(&closedFuncCharNo, linestarts[lineno] + charno - 2);
			r = BEGINCLOSEDFUNC;
		} else {
			unput(c); 
			r = '#'; 
		}
		goto leave;
	}
	else if (c == '$') { 
		c = input();
		if (c == '\\') {
			c = input();
			switch (c) {
				case 'n' : c = '\n'; break;
				case 'r' : c = '\r'; break;
				case 't' : c = '\t'; break;
				case 'f' : c = '\f'; break;
				case 'v' : c = '\v'; break;
			}
		}
		r = processchar(c);
		goto leave; 
	}
	else if (c == ',') { r = c; goto leave; }
	else if (c == '=') {
		c = input();
		if (strchr(binopchars, c)) goto binop;
		else { 
			unput(c); 
			r = '=';
			goto leave; 
		}
	}
	else if (strchr(binopchars, c))  goto binop;
	else goto error1;

ident:
	c = input();
	
	if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' 
		|| c == '_' || c >= '0' && c <= '9') goto ident;
	else if (c == ':') {
		yytext[yylen] = 0;
		r = processkeywordbinop(yytext) ; 
		goto leave;
	} else {
		unput(c);
		yytext[yylen] = 0;
		r = processident(yytext) ; 
		goto leave;
	}

symbol1:
	c = input();
	
	if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c == '_') goto symbol2;
	else if (c >= '0' && c <= '9') goto symbol4;
	else {
		unput(c);
		yytext[yylen] = 0;
		r = processsymbol(yytext) ; 
		goto leave;
	}

symbol2:
	c = input();
	
	if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' 
		|| c == '_' || c >= '0' && c <= '9') goto symbol2;
	else {
		unput(c);
		yytext[yylen] = 0;
		r = processsymbol(yytext) ; 
		goto leave;
	}
	
symbol4:
	c = input();
	if (c >= '0' && c <= '9') goto symbol4;
	else {
		unput(c);
		yytext[yylen] = 0;
		r = processsymbol(yytext) ; 
		goto leave;
	}
	

binop:

	c = input();
	
	if (c == 0) goto binop2;
	if (strchr(binopchars, c))  goto binop;
	else {
		binop2:
		unput(c);
		yytext[yylen] = 0;
		r = processbinop(yytext) ; 
		goto leave;
	}

radix_digits_1:

	c = input();
	if (c >= '0' && c <= '9') goto radix_digits_1;
	if (c >= 'a' && c <= 'z') goto radix_digits_1;
	if (c >= 'A' && c <= 'Z') goto radix_digits_1;
	if (c == '.') {
		decptpos = yylen;
		goto radix_digits_2;
	}
	unput(c);
	yytext[yylen] = 0;
	r = processintradix(yytext);
	goto leave;
	
radix_digits_2:

	c = input();
	if (c >= '0' && c <= '9') goto radix_digits_2;
	if (c >= 'a' && c <= 'z') goto radix_digits_2;
	if (c >= 'A' && c <= 'Z') goto radix_digits_2;
	unput(c);
	yytext[yylen] = 0;
	r = processfloatradix(yytext);
	goto leave;

hexdigits:

	c = input();
	if (c >= '0' && c <= '9') goto hexdigits;
	if (c >= 'a' && c <= 'f') goto hexdigits;
	if (c >= 'A' && c <= 'F') goto hexdigits;
	unput(c);
	yytext[yylen] = 0;
	r = processhex(yytext);
	goto leave;

digits_1:	/* number started with digits */

	c = input();
	
	if (c >= '0' && c <= '9') goto digits_1;
	else if (c == 'r') {
		radixcharpos = yylen;
		goto radix_digits_1;
	}
	else if (c == 'e' || c == 'E') goto expon_1;
	else if (c == '.') {
		c2 = input();
		if (c2 >= '0' && c2 <= '9') goto digits_2;
		else {
			unput(c2);
			unput(c);
			yytext[yylen] = 0;
			r = processint(yytext); 
			goto leave;
		}
	} 
	else if (c == 'b' || c == 's') {
		d = input();
		if (d >= '0' && d <= '9') goto accidental1;
		if (d == c) goto accidental2;
		goto accidental3;
accidental1:
		d = input();
		if (d >= '0' && d <= '9') goto accidental1;
		unput(d);
		yytext[yylen] = 0;
		r = processaccidental1(yytext); 
		goto leave;
accidental2:
		d = input();
		if (d == c) goto accidental2;
accidental3:
		unput(d);
		yytext[yylen] = 0;
		r = processaccidental2(yytext); 
		goto leave;
	}
	else if (c == 'x') {
		yylen = 0; 
		goto hexdigits;
	} else {
		unput(c);
		yytext[yylen] = 0;
		r = processint(yytext); 
		goto leave;
	}

digits_2:	

	c = input();
	
	if (c >= '0' && c <= '9') goto digits_2;
	else if (c == 'e' || c == 'E') goto expon_1;
//	else if (c == 'π' || c == '∏') {
//		--yylen;
//		yytext[yylen] = 0;
//		r = processfloat(yytext, 1); 
//		goto leave;
//	} 
	else {
		unput(c);
		yytext[yylen] = 0;
		r = processfloat(yytext, 0); 
		goto leave;
	}

expon_1:	/* e has been seen, need digits */
	c = input();
	
	if (c >= '0' && c <= '9') goto expon_3;
	else if (c == '+' || c == '-') goto expon_2;
	else goto error1;

expon_2:	/* + or - seen but still need digits */
	c = input();
	
	if (c >= '0' && c <= '9') goto expon_3;
	else goto error1;

expon_3:
	c = input();
	
	if (c >= '0' && c <= '9') goto expon_3;
//	else if (c == 'π' || c == '∏') {
//		--yylen;
//		yytext[yylen] = 0;
//		r = processfloat(yytext, 1); 
//		goto leave;
//	}
	else {
		unput(c);
		yytext[yylen] = 0;
		r = processfloat(yytext, 0); 
		goto leave;
	}
	
symbol3 : {	
		int startline, endchar;
		startline = lineno;
		endchar = '\'';
        
		/*do {
			c = input();
		} while (c != endchar && c != 0);*/
		for (;yylen<MAXYYLEN;) {
			c = input();
			if (c == '\n' || c == '\r') {
                asRelativePath(curfilename,extPath);
				post("Symbol open at end of line on line %d in file '%s'\n", 
					startline, extPath);
				yylen = 0;
				r = 0; 
				goto leave;
			}
			if (c == '\\') {
				yylen--;
				c = input();
			} else if (c == endchar) break;
			if (c == 0) break;
		}
		if (c == 0) {
            asRelativePath(curfilename,extPath);
			post("Open ended symbol ... started on line %d in file '%s'\n", 
				startline, extPath);
			yylen = 0;
			r = 0; 
			goto leave;
		}
		yytext[yylen] = 0;
		yytext[yylen-1] = 0;
		r = processsymbol(yytext);
		goto leave;
	}
	
string1 : {	
		int startline, endchar;
		startline = lineno;
		endchar = '"';
		
		for (;yylen<MAXYYLEN;) {
			c = input();
			if (c == '\\') {
				yylen--;
				c = input();
				switch (c) {
					case 'n' : yytext[yylen-1] = '\n'; break;
					case 'r' : yytext[yylen-1] = '\r'; break;
					case 't' : yytext[yylen-1] = '\t'; break;
					case 'f' : yytext[yylen-1] = '\f'; break;
					case 'v' : yytext[yylen-1] = '\v'; break;
				}
			} else if (c == '\r') c = '\n';
			else if (c == endchar) break;
			if (c == 0) break;
		}
		if (c == 0) {
            asRelativePath(curfilename,extPath);
			post("Open ended string ... started on line %d in file '%s'\n", 
				startline, extPath);
			yylen = 0;
			r = 0; 
			goto leave;
		}
		yylen--;
		
		do {
			c = input0();
		} while (c && isspace(c));
		
		if (c == '"') goto string1;
		else if (c) unput0(c);
		
		yytext[yylen] = 0;
		r = processstring(yytext);
		goto leave;
	}
	
comment1:	/* comment -- to end of line */
	do {
		c = input0(); 
	} while (c != '\n' && c != '\r' && c != 0);
	yylen = 0;
	if (c == 0) { r = 0; goto leave; }
	else goto start;

comment2 : {
		int startline, clevel, prevc;
		startline = lineno;
		prevc = 0;
		clevel = 1;
		do {
			c = input0();
			if (c == '/' && prevc == '*') {
				if (--clevel <= 0) break;
			} else if (c == '*' && prevc == '/') clevel++;
			prevc = c;
		} while (c != 0);
		yylen = 0;
		if (c == 0) {
            asRelativePath(curfilename,extPath);
			post("Open ended comment ... started on line %d in file '%s'\n", 
				startline, extPath);
			r = 0; 
			goto leave;
		}
		goto start;
	}

	
error1:
	
	yytext[yylen] = 0;
    
    asRelativePath(curfilename, extPath);
	post("illegal input string '%s' \n   at '%s' line %d char %d\n", 
		yytext, extPath, lineno, charno);
	post("code %d\n", c);
	//postfl(" '%c' '%s'\n", c, binopchars);
	//postfl("%d\n", strchr(binopchars, c));
	
error2:
    asRelativePath(curfilename, extPath);
	post("  in file '%s' line %d char %d\n", extPath, lineno, charno);
	r = BADTOKEN; 
	goto leave;
	
leave:
	yytext[yylen] = 0;
	
#if DEBUGLEX
	if (gDebugLexer) postfl("yylex: %d  '%s'\n",r,yytext);
#endif
	//if (lexCmdLine>0) postfl("yylex: %d  '%s'\n",r,yytext);
	return r;
}

int processbinop(char *token) 
{
	PyrSymbol *sym;
	PyrSlot slot;
	PyrSlotNode *node;
	
#if DEBUGLEX
	if (gDebugLexer) postfl("processbinop: '%s'\n",token);
#endif
	sym = getsym(token);
	SetSymbol(&slot, sym);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	if (strcmp(token, "<-")==0) return LEFTARROW;
	if (strcmp(token, "<>")==0) return READWRITEVAR;
	if (strcmp(token, "|")==0) return '|';
	if (strcmp(token, "<")==0) return '<';
	if (strcmp(token, ">")==0) return '>';
	if (strcmp(token, "-")==0) return '-';
	if (strcmp(token, "*")==0) return '*';
	if (strcmp(token, "+")==0) return '+';
	return BINOP;
}

int processkeywordbinop(char *token) 
{
	PyrSymbol *sym;
	PyrSlot slot;
	PyrSlotNode *node;
	
	//post("'%s'  file '%s'\n", token, curfilename);
	
#if DEBUGLEX
	if (gDebugLexer) postfl("processkeywordbinop: '%s'\n",token);
#endif
	token[strlen(token)-1] = 0; // strip off colon
	sym = getsym(token);
	SetSymbol(&slot, sym);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return KEYBINOP;
}

int processident(char *token) 
{
	char c;
	PyrSymbol *sym;
	
	PyrSlot slot;
	PyrParseNode *node;
	
	c = token[0];
	zzval = -1;
	
#if DEBUGLEX
	if (gDebugLexer) postfl("word: '%s'\n",token);
#endif
	/*
	strcpy(uptoken, token);
	for (str = uptoken; *str; ++str) {
		if (*str >= 'a' && *str <= 'z') *str += 'A' - 'a';
	}*/

	if (token[0] == '_') {
		if (token[1] == 0) {
			node = newPyrCurryArgNode();
			zzval = (int)node;
			return CURRYARG;
		} else {
			sym = getsym(token);
			SetSymbol(&slot, sym);
			node = newPyrSlotNode(&slot);
			zzval = (int)node;
			return PRIMITIVENAME;
		}
	}
	if (token[0] >= 'A' && token[0] <= 'Z') {
		sym = getsym(token);
		SetSymbol(&slot, sym);
		node = newPyrSlotNode(&slot);
		zzval = (int)node;
#if DEBUGLEX
	if (gDebugLexer) postfl("CLASSNAME: '%s'\n",token);
#endif
		return CLASSNAME;
	}
	if (strcmp("var",token) ==0) return VAR; 
	if (strcmp("arg",token) ==0) return ARG; 
	if (strcmp("classvar",token) ==0) return CLASSVAR; 
	if (strcmp("const",token) ==0) return CONST; 
	
	if (strcmp("while",token) ==0) { 
		sym = getsym(token);
		SetSymbol(&slot, sym);
		node = newPyrSlotNode(&slot);
		zzval = (int)node;
		return WHILE; 
	}
	if (strcmp("pi",token) ==0) {
		SetFloat(&slot, pi);
		node = newPyrSlotNode(&slot);
		zzval = (int)node;
		return PIE; 
	}
	if (strcmp("true",token) ==0) {
		SetTrue(&slot);
		node = newPyrSlotNode(&slot);
		zzval = (int)node;
		return TRUEOBJ; 
	}
	if (strcmp("false",token) ==0) {
		SetFalse(&slot);
		node = newPyrSlotNode(&slot);
		zzval = (int)node;
		return FALSEOBJ; 
	}
	if (strcmp("nil",token) ==0) {
		SetNil(&slot);
		node = newPyrSlotNode(&slot);
		zzval = (int)node;
		return NILOBJ; 
	}
	if (strcmp("inf",token) ==0) {
#ifdef SC_WIN32
    double a = 0.0;
    double b = 1.0/a;
    SetFloat(&slot, b);
#else
    SetFloat(&slot, INFINITY);
#endif
		node = newPyrSlotNode(&slot);
		zzval = (int)node;
		return FLOAT; 
	}

	sym = getsym(token);
	
	SetSymbol(&slot, sym);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return NAME;
}

int processhex(char *s)
{
	PyrSlot slot;
	PyrSlotNode *node;
	char *c;
	int val;
#if DEBUGLEX
	if (gDebugLexer) postfl("processhex: '%s'\n",s);
#endif

	c = s;
	val = 0;
	while (*c) {
		if (*c >= '0' && *c <= '9') val = val*16 + *c - '0';
		else if (*c >= 'a' && *c <= 'z') val = val*16 + *c - 'a' + 10;
		else if (*c >= 'A' && *c <= 'Z') val = val*16 + *c - 'A' + 10;
		c++;
	}
	
	SetInt(&slot, val);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return INTEGER;
}


int processintradix(char *s) 
{
	PyrSlot slot;
	PyrSlotNode *node;
	char *c;
	int val, radix;
#if DEBUGLEX
	if (gDebugLexer) postfl("processhex: '%s'\n",s);
#endif

	c = s;
	radix = 0;
	while (*c != 'r') {
		if (*c >= '0' && *c <= '9') radix = radix*10 + *c - '0';
		c++;
	}
	c++;
	val = 0;
	while (*c) {
		if (*c >= '0' && *c <= '9') val = val*radix + *c - '0';
		else if (*c >= 'a' && *c <= 'z') val = val*radix + *c - 'a' + 10;
		else if (*c >= 'A' && *c <= 'Z') val = val*radix + *c - 'A' + 10;
		c++;
	}
	
	SetInt(&slot, val);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return INTEGER;
}

int processfloatradix(char *s) 
{
	PyrSlot slot;
	PyrSlotNode *node;
	char *c;
	int iradix, numdecplaces;
	double val, radix;
#if DEBUGLEX
	if (gDebugLexer) postfl("processhex: '%s'\n",s);
#endif

	c = s;
	iradix = 0;
	while (*c != 'r') {
		if (*c >= '0' && *c <= '9') iradix = iradix*10 + *c - '0';
		c++;
	}
	c++;
	
	radix = iradix;
	val = 0.;
	while (*c) {
		if (*c >= '0' && *c <= '9') val = val*radix + *c - '0';
		else if (*c >= 'a' && *c <= 'z') val = val*radix + *c - 'a' + 10;
		else if (*c >= 'A' && *c <= 'Z') val = val*radix + *c - 'A' + 10;
		//else if (c == '.') OK..
		c++;
	}
	numdecplaces = yylen - decptpos;
	val = val / pow(radix, numdecplaces);
	SetFloat(&slot, val);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return INTEGER;
}

int processint(char *s) 
{
	PyrSlot slot;
	PyrSlotNode *node;
#if DEBUGLEX
	if (gDebugLexer) postfl("processint: '%s'\n",s);
#endif
	
	SetInt(&slot, atoi(s));
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return INTEGER;
}

int processchar(int c) 
{
	PyrSlot slot;
	PyrSlotNode *node;
#if DEBUGLEX
	if (gDebugLexer) postfl("processhex: '%c'\n",c);
#endif
	
	SetChar(&slot, c);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return ASCII;
}

int processfloat(char *s, int sawpi) 
{
	PyrSlot slot;
	PyrSlotNode *node;
	double z;
#if DEBUGLEX
	if (gDebugLexer) postfl("processfloat: '%s'\n",s);
#endif
	
	if (sawpi) { z = atof(s)*pi; SetFloat(&slot, z); }
	else  { SetFloat(&slot, atof(s)); }
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return FLOAT;
}


int processaccidental1(char *s) 
{
	PyrSlot slot;
	PyrSlotNode *node;
	char *c;
	double degree=0.;
	double cents=0.;
	double centsdiv=1000.;
#if 0
	printf("processaccidental1: '%s'\n",s);
#endif

	c = s;
	while (*c) {
		if (*c >= '0' && *c <= '9') degree = degree*10. + *c - '0';
		else break;
		c++;
	}
	
	if (*c == 'b') centsdiv = -1000.;
	else if (*c == 's') centsdiv = 1000.;
	c++;
		
	while (*c) {
		if (*c >= '0' && *c <= '9') {
			cents = cents*10. + *c - '0';
		}
		else break;
		c++;
	}
	
	if (cents > 499.) cents = 499.;

	SetFloat(&slot, degree + cents/centsdiv);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return ACCIDENTAL;
}

int processaccidental2(char *s) 
{
	PyrSlot slot;
	PyrSlotNode *node;
	char *c;
	double degree=0.;
	double semitones=0.;
#if 0
	printf("processaccidental2: '%s'\n",s);
#endif

	c = s;
	while (*c) {
		if (*c >= '0' && *c <= '9') degree = degree*10. + *c - '0';
		else break;
		c++;
	}
	
	while (*c) {
		if (*c == 'b') semitones -= 1.;
		else if (*c == 's') semitones += 1.;
		c++;
	}

	if (semitones > 4.) semitones = 4.;
	
	SetFloat(&slot, degree + semitones/10.);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return ACCIDENTAL;
}

int processsymbol(char *s) 
{
	PyrSlot slot;
	PyrSlotNode *node;
	PyrSymbol *sym;
#if DEBUGLEX
	if (gDebugLexer) postfl("processsymbol: '%s'\n",s);
#endif
	sym = getsym(s+1);
	
	SetSymbol(&slot, sym);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return SYMBOL;
}

int processstring(char *s) 
{
	PyrSlot slot;
	PyrSlotNode *node;
	PyrString *string;
#if DEBUGLEX
	if (gDebugLexer) postfl("processstring: '%s'\n",s);
#endif
	int flags = compilingCmdLine ? obj_immutable : obj_permanent | obj_immutable;
	string = newPyrString(gMainVMGlobals->gc, s+1, flags, false);
	SetObject(&slot, string);
	node = newPyrSlotNode(&slot);
	zzval = (int)node;
	return STRING;
}

void yyerror(char *s) 
{
	parseFailed = 1;
	yytext[yylen] = 0;
	error("Parse error\n");
	postErrorLine(lineno, linepos, charno);
	//Debugger();
}

void fatal() 
{
	parseFailed = 1;
	yytext[yylen] = 0;
	error("Parse error\n");
	postErrorLine(lineno, linepos, charno);
	//Debugger();
}

#if 0
void postErrorLine() 
{
	int i, j, start, end;
	char str[256];
	
	parseFailed = true;
	for (i=textpos-1; i>=0; --i) {
		if (text[i] == '\r' || text[i] == '\n') break;
	}
	start = i+1;
	for (i=textpos; i < textlen; ++i) {
		if (text[i] == '\r' || text[i] == '\n') break;
	}
	end=i;
	for (i=start, j=0; i<end; ++i) {
		if (i == textpos) str[j++] = '¶';
		str[j++] = text[i];
	}
	if (textpos == end) str[j++] = '¶';
	str[j] = 0;
	postfl("%s\n", str);
}
#endif

void postErrorLine(int linenum, int start, int charpos) 
{
	int i, j, end, pos;
	char str[256];
	
	//post("start %d\n", start);
	//parseFailed = true;
    char extPath[MAXPATHLEN];
    asRelativePath(curfilename, extPath);
	post("   in file '%s'\n", extPath);
	post("   line %d char %d :\n", linenum, charpos);
	// nice: postfl previous line for context

	//postfl("text '%s' %d\n", text, text);

	// postfl error line for context
	pos = start + charpos;
	for (i=pos; i < textlen; ++i) {
		if (text[i] == 0 || text[i] == '\r' || text[i] == '\n') break;
	}
	end=i;
	for (i=start, j=0; i<end && j<255; ++i) {
		if (i == pos) str[j++] = BULLET_CHAR;
		str[j++] = text[i];
	}
	if (pos == end) str[j++] = BULLET_CHAR;
	str[j] = 0;
	post("  %s\n", str);
	
	i=end+1;
	if (i<textlen) {
		// postfl following line for context
		for (j=0; j<255 && i<textlen; ++i) {
			if (text[i] == 0 ||text[i] == '\r' || text[i] == '\n') break;
			str[j++] = text[i];
		}
		str[j] = 0;
		post("  %s\n", str);
	}
	post("-----------------------------------\n", str);
}

/*
void c2pstrcpy(unsigned char* dst, const char *src);
void c2pstrcpy(unsigned char* dst, const char *src)
{
	int c;
	unsigned char *dstp = &dst[1];
	while ((c = *src++) != 0) *dstp++ = c;
	dst[0] = dstp - dst - 1;
}

void p2cstrcpy(char *dst, const unsigned char* src);
void p2cstrcpy(char *dst, const unsigned char* src)
{
	int n = *src++;
	for (int i=0; i<n; ++i) *dst++ = *src++;
	*dst++ = 0;
}
*/

void pstrncpy(unsigned char *s1, unsigned char *s2, int n);
void pstrncpy(unsigned char *s1, unsigned char *s2, int n)
{
	int i, m;
	m = *s2++;
	n = (n < m) ? n : m;
	*s1 = n; s1++;
	for (i=0; i<n; ++i) { *s1 = *s2; s1++; s2++; }
}

int pstrcmp(unsigned char *s1, unsigned char *s2);
int pstrcmp(unsigned char *s1, unsigned char *s2)
{
	int i, len1, len2, len;
	len1 = *s1++;
	len2 = *s2++;
	len = sc_min(len1, len2);
	for (i=0; i<len; ++i) {
		if (s1[i] < s2[i]) return -1;
		if (s1[i] > s2[i]) return 1;
	}
	if (len1 < len2) return -1;
	if (len1 > len2) return 1;
	return 0;
}

bool scanForClosingBracket()
{
	int r, c, d, startLevel;
	bool res = true;
	// finite state machine to parse input stream into tokens
	
#if DEBUGLEX
	if (gDebugLexer) postfl("->scanForClosingBracket\n");
#endif
	startLevel = brackets.num;
start:
	c = input0();
	
	if (c == 0) goto leave;
	else if (c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='\v' || c=='\f') {
		goto start;
	}
	else if (c == '\'') goto symbol3;
	else if (c == '"') goto string1;
	else if (c == '/') {
		c = input0();
		if (c == '/') goto comment1;
		else if (c == '*') goto comment2;
		else { unput(c); goto start; }
	}
	else if (c == '$') { 
		c = input0();
		if (c == '\\') {
			c = input0();
			switch (c) {
				case 'n' : c = '\n'; break;
				case 'r' : c = '\r'; break;
				case 't' : c = '\t'; break;
				case 'f' : c = '\f'; break;
				case 'v' : c = '\v'; break;
			}
		}
		goto start; 
	}
	else if (c == OPENPAREN || c == OPENSQUAR || c == OPENCURLY) {
		pushls(&brackets, (int)c);
		r = c;
		goto start;
	}
	else if (c == CLOSSQUAR) { 
		if (!emptyls(&brackets)) {
			if ((d = popls(&brackets)) != OPENSQUAR) {
				fatal();
				post("opening bracket was a '%c', but found a '%c'\n",d,c);
				goto error1;
			}
		} else {
			fatal();
			post("unmatched '%c'\n",c);
			goto error1;
		}
		r = c; 
		if (brackets.num < startLevel) goto leave;
		else goto start; 
	}
	else if (c == CLOSPAREN) { 
		if (!emptyls(&brackets)) {
			if ((d = popls(&brackets)) != OPENPAREN) {
				fatal();
				post("opening bracket was a '%c', but found a '%c'\n",d,c);
				goto error1;
			}
		} else {
			fatal();
			post("unmatched '%c'\n",c);
			goto error1;
		}
		if (brackets.num < startLevel) goto leave;
		else goto start; 
	}
	else if (c == CLOSCURLY) { 
		if (!emptyls(&brackets)) {
			if ((d = popls(&brackets)) != OPENCURLY) {
				fatal();
				post("opening bracket was a '%c', but found a '%c'\n",d,c);
				goto error1;
			}
		} else {
			fatal();
			post("unmatched '%c'\n",c);
			goto error1;
		}
		if (brackets.num < startLevel) goto leave;
		else goto start; 
	} else {
		goto start; 
	}
symbol3 : {	
		int startline, endchar;
		startline = lineno;
		endchar = '\'';
	
		do {
			c = input0();
			if (c == '\\') {
				c = input0();
			}
		} while (c != endchar && c != 0);
		if (c == 0) {
            char extPath[MAXPATHLEN];
            asRelativePath(curfilename, extPath);
			post("Open ended symbol ... started on line %d in file '%s'\n", 
				startline, extPath);
			goto error2;
		}
		goto start;
	}

string1 : {	
		int startline, endchar;
		startline = lineno;
		endchar = '\"';
	
		do  {
			c = input0();
			if (c == '\\') {
				c = input0();
			}
		} while (c != endchar && c != 0);
		if (c == 0) {
            char extPath[MAXPATHLEN];
            asRelativePath(curfilename, extPath);
			post("Open ended string ... started on line %d in file '%s'\n", 
				startline, extPath);
			goto error2;
		}
		goto start;
	}
comment1:	/* comment -- to end of line */
	do {
		c = input0(); 
	} while (c != '\n' && c != '\r' && c != 0);
	if (c == 0) { goto error1; }
	else goto start;

comment2 : {
		int startline, clevel, prevc;
		startline = lineno;
		prevc = 0;
		clevel = 1;
		do {
			c = input0();
			if (c == '/' && prevc == '*') {
				if (--clevel <= 0) break;
			} else if (c == '*' && prevc == '/') clevel++;
			prevc = c;
		} while (c != 0);
		if (c == 0) {
            char extPath[MAXPATHLEN];
            asRelativePath(curfilename, extPath);
			post("Open ended comment ... started on line %d in file '%s'\n", 
				startline, extPath);
			goto error2;
		}
		goto start;
	}

error1:
    char extPath[MAXPATHLEN];
    asRelativePath(curfilename, extPath);
	post("  in file '%s' line %d char %d\n", extPath, lineno, charno);
	res = false;
	goto leave;
	
error2:
	res = false;
	goto leave;
	
leave:
#if DEBUGLEX
	if (gDebugLexer) postfl("<-scanForClosingBracket\n");
#endif
	return res;
}


int numClassDeps;
static ClassExtFile* sClassExtFiles;

ClassExtFile* newClassExtFile(PyrSymbol *fileSym);
ClassExtFile* newClassExtFile(PyrSymbol *fileSym)
{
	ClassExtFile* classext;
	classext = (ClassExtFile*)pyr_pool_compile->Alloc(sizeof(ClassExtFile));
	classext->fileSym = fileSym;
	classext->next = sClassExtFiles;
	sClassExtFiles = classext;
	return classext;
}


ClassDependancy* newClassDependancy(PyrSymbol *className, PyrSymbol *superClassName, 
	PyrSymbol *fileSym)
{
	ClassDependancy* classdep;
	
	//post("classdep '%s' '%s' '%s' %d %d\n", className->name, superClassName->name, 
	//	fileSym->name, className, superClassName);
	// pyrmalloc: 
	// lifetime: kill after compile. 
	numClassDeps++;
	if (className->classdep) {
		error("duplicate Class found: '%s' \n", className->name);
                post("%s\n",className->classdep->fileSym->name);
                postfl("%s\n\n",fileSym->name);
		return className->classdep;
	}
	classdep = (ClassDependancy*)pyr_pool_compile->Alloc(sizeof(ClassDependancy));
	MEMFAIL(text);
	classdep->className = className;
	classdep->superClassName = superClassName;
	classdep->fileSym = fileSym;
	classdep->superClassDep = NULL;
	classdep->next = NULL;
	classdep->subclasses = NULL;
	className->classdep = classdep;
	return classdep;
}

void buildDepTree()
{
	ClassDependancy *next;
	SymbolTable* symbolTable = gMainVMGlobals->symbolTable;
	
	//postfl("->buildDepTree\n"); fflush(stdout);
	for (int i=0; i<symbolTable->TableSize(); ++i) {
		PyrSymbol *sym = symbolTable->Get(i);
		if (sym && (sym->flags & sym_Class)) {
			if (sym->classdep) {
				if (sym->classdep->superClassName->classdep) {
					next = sym->classdep->superClassName->classdep->subclasses;
					sym->classdep->superClassName->classdep->subclasses = sym->classdep;
					sym->classdep->next = next;
				} else if (sym->classdep->superClassName != s_none) {
					error("Superclass '%s' of class '%s' is not defined in any file.\n%s\n",
						sym->classdep->superClassName->name, sym->classdep->className->name,sym->classdep->fileSym->name);
				}
			}
		}
	}
	//postfl("<-buildDepTree\n"); fflush(stdout);
}

ClassDependancy **gClassCompileOrder;
int gClassCompileOrderNum = 0;
int gClassCompileOrderSize = 1000;

void compileDepTree();

void traverseFullDepTree()
{
	//postfl("->traverseFullDepTree\n"); fflush(stdout);
	gClassCompileOrderNum = 0;
	gClassCompileOrder = (ClassDependancy**)pyr_pool_compile->Alloc(
								gClassCompileOrderSize * sizeof(ClassDependancy));
	MEMFAIL(gClassCompileOrder);
	
	// parse and compile all files
	initParser();
	gParserResult = -1;

	traverseDepTree(s_object->classdep, 0);
	compileDepTree();
	compileClassExtensions();
	
	pyr_pool_compile->Free(gClassCompileOrder);
	
	finiParser();
	//postfl("<-traverseFullDepTree\n"); fflush(stdout);
}


void traverseDepTree(ClassDependancy *classdep, int level)
{
	ClassDependancy *subclassdep;
        
        if (!classdep) return;
	
	//postfl("traverse %d '%s' '%s' '%s'\n", level, classdep->className->name, classdep->superClassName->name,
//		classdep->fileSym->name); fflush(stdout);
	
	subclassdep = classdep->subclasses;
	for (; subclassdep; subclassdep = subclassdep->next) {
		traverseDepTree(subclassdep, level+1);
	}
	if (gClassCompileOrderNum > gClassCompileOrderSize) {
		gClassCompileOrderSize *= 2;
		gClassCompileOrder = (ClassDependancy**)pyr_pool_compile->Realloc(gClassCompileOrder, 
								gClassCompileOrderSize * sizeof(ClassDependancy));
		MEMFAIL(gClassCompileOrder);		
	}
	gClassCompileOrder[gClassCompileOrderNum++] = classdep;
}


void compileFileSym(PyrSymbol *fileSym)
{
	if (!(fileSym->flags & sym_Compiled)) {
		fileSym->flags |= sym_Compiled;
		gCompilingFileSym = fileSym;
		gCompilingVMGlobals = 0;
		gRootParseNode = NULL;
		initParserPool();
		if (startLexer(fileSym->name)) {
			//postfl("->Parsing %s\n", fileSym->name); fflush(stdout);
			parseFailed = yyparse();
			//postfl("<-Parsing %s %d\n", fileSym->name, parseFailed); fflush(stdout);
			//post("parseFailed %d\n", parseFailed); fflush(stdout);
			if (!parseFailed && gRootParseNode) {
				//postfl("Compiling nodes %08X\n", gRootParseNode);fflush(stdout);
				compilingCmdLine = false;
				compileNodeList(gRootParseNode, true);
				//postfl("done compiling\n");fflush(stdout);
			} else {
				compileErrors++;
                char extPath[MAXPATHLEN];
                asRelativePath(fileSym->name, extPath);
				error("file '%s' parse failed\n", extPath);
			}
			finiLexer();
		} else {
			error("file '%s' open failed\n", fileSym->name);
		}
		freeParserPool();

	}
}

void compileDepTree()
{
	ClassDependancy *classdep;
	int i;
	
	for (i=gClassCompileOrderNum-1; i>=0; --i) {
		classdep = gClassCompileOrder[i];
		//postfl("compile %d '%s' '%s' '%s'\n", i, classdep->className->name, classdep->superClassName->name,
		//	classdep->fileSym->name);
		compileFileSym(classdep->fileSym);
	}
	//postfl("<compile\n");
}

void compileClassExtensions()
{
	ClassExtFile *classext = sClassExtFiles;
	while (classext) {
		compileFileSym(classext->fileSym);
		classext = classext->next;
	}
}

void findDiscrepancy();

void traverseFullDepTree2()
{
	// assign a class index to all classes
	if (!parseFailed && !compileErrors) {
		buildClassTree();
		gNumClasses = 0;
		
		// now I index them during pass one
		indexClassTree(class_object, 0);
		setSelectorFlags();
		if (2*numClassDeps != gNumClasses) {
			error("There is a discrepancy.\n");
                    /* not always correct
                    if(2*numClassDeps < gNumClasses) {
                        post("Duplicate files may exist in the directory structure.\n");
                    } else {
                        post("Some class files may be missing.\n");
                    }
                    */
                    post("numClassDeps %d   gNumClasses %d\n", numClassDeps, gNumClasses);
			findDiscrepancy();
			compileErrors++;
		} else {
			double elapsed;
			buildBigMethodMatrix();
			SymbolTable* symbolTable = gMainVMGlobals->symbolTable;
			post("\tNumber of Symbols %d\n", symbolTable->NumItems());
			post("\tByte Code Size %d\n", totalByteCodes);
			//elapsed = TickCount() - compileStartTime;
			//elapsed = 0;
                        elapsed = elapsedTime() - compileStartTime;
                        post("\tcompiled %d files in %.2f seconds \n", 
				gNumCompiledFiles, elapsed );
			post("compile done\n");
		}
	}
}

bool parseOneClass(PyrSymbol *fileSym)
{
	int token;
	PyrSymbol *className, *superClassName;
	ClassDependancy *classdep;
	bool res;
	
	res = true;
	token = yylex();
	if (token == CLASSNAME) {
		className = ((PyrSlotNode*)zzval)->mSlot.us;
		// I think this is wrong: zzval is space pool alloced
		//pyrfree((PyrSlot*)zzval);
		token = yylex();
		if (token == 0) return false;
		if (token == OPENSQUAR) {
			scanForClosingBracket(); // eat indexing spec
			token = yylex();
			if (token == 0) return false;
		}
		if (token == ':') {
			token = yylex();  // get super class
			if (token == 0) return false;
			if (token == CLASSNAME) {
				superClassName = ((PyrSlotNode*)zzval)->mSlot.us;
				// I think this is wrong: zzval is space pool alloced
				//pyrfree((PyrSlot*)zzval);
				token = yylex();
				if (token == 0) return false;
				if (token == OPENCURLY) {
					scanForClosingBracket(); // eat class body
					classdep = newClassDependancy(className, superClassName, fileSym);
				} else {
					compileErrors++;
					postfl("Expected %c.  got token: '%s' %d\n", OPENCURLY, yytext, token);
					postErrorLine(lineno, linepos, charno);
					return false;
				}
			} else {
				compileErrors++;
				post("Expected superclass name.  got token: '%s' %d\n", yytext, token);
				postErrorLine(lineno, linepos, charno);
				return false;
			}
		} else if (token == OPENCURLY) {
			if (className == s_object) superClassName = s_none;
			else superClassName = s_object;
			scanForClosingBracket(); // eat class body
			classdep = newClassDependancy(className, superClassName, fileSym);
		} else {
			compileErrors++;
			post("Expected ':' or %c.  got token: '%s' %d\n", OPENCURLY, yytext, token);
			postErrorLine(lineno, linepos, charno);
			return false;
		}
	} else if (token == '+') {
		newClassExtFile(fileSym);
		return false;
	} else {
		if (token != 0) {
			compileErrors++;
			post("Expected class name.  got token: '%s' %d\n", yytext, token);
			postErrorLine(lineno, linepos, charno);
			return false;
		} else {
			res = false;
		}
	}
	return res;
}

//void ClearLibMenu();

void aboutToFreeRuntime();
void aboutToFreeRuntime()
{
	//ClearLibMenu();
}

//void init_graph_compile();
//void tellPlugInsAboutToCompile();
void pyrmath_init_globs();

void initPassOne()
{
	aboutToFreeRuntime();
	
	//dump_pool_histo(pyr_pool_runtime);
	pyr_pool_runtime->FreeAllInternal();
	//dump_pool_histo(pyr_pool_runtime);
	//gPermanentObjPool.Init(pyr_pool_runtime, PERMOBJCHUNK);
	sClassExtFiles = 0;
	
	void *ptr = pyr_pool_runtime->Alloc(sizeof(SymbolTable));
	gMainVMGlobals->symbolTable  = new (ptr) SymbolTable(pyr_pool_runtime, 8192);

	//gFileSymbolTable = newSymbolTable(512);
	
	pyrmath_init_globs();
	
	initSymbols(); // initialize symbol globals
	//init_graph_compile();
	initSpecialSelectors();
	initSpecialClasses();
	initClasses();
	initParserPool();
	initParseNodes();
	initPrimitives();
	//tellPlugInsAboutToCompile();
	initLexer();
	compileErrors = 0;
	numClassDeps = 0;
	compiledOK = false;
}

void finiPassOne()
{
    //postfl("->finiPassOne\n");
    freeParserPool();
    //postfl("<-finiPassOne\n");
}

// Returns TRUE iff dirname is an existing directory.

bool sc_DirectoryExists(const char *dirname);
bool sc_DirectoryExists(const char *dirname) 
{
#ifndef SC_WIN32
  struct stat buf;
  int err = stat(dirname, &buf);
  return (err == 0) && (buf.st_mode & S_IFDIR);
#else
  return 0;
#endif
}

// Returns TRUE iff 'name' is to be ignored during compilation.

static bool sc_IsNonHostPlatformDir(const char *name);
static bool sc_IsNonHostPlatformDir(const char *name)
{
#if defined(SC_DARWIN)
  char a[] = "linux", b[] = "windows";
#elif defined(SC_LINUX)
  char a[] = "osx", b[] = "windows";
#elif defined(SC_WIN32)
  char a[] = "osx", b[] = "linux";
#endif
  return ((strcmp(name, a) == 0) || 
	  (strcmp(name, b) == 0));
}

static bool sc_SkipDirectory(const char *name);
static bool sc_SkipDirectory(const char *name)
{
  return ((strcmp(name, ".") == 0) || 
	  (strcmp(name, "..") == 0) ||
	  (strcasecmp(name, "help") == 0) ||
	  (strcasecmp(name, "test") == 0) ||
	  sc_IsNonHostPlatformDir(name));
}

bool passOne_ProcessDir(char *dirname, int level);
#ifndef SC_WIN32
bool passOne_ProcessDir(char *dirname, int level)
{
	bool success = true;

#ifdef ENABLE_LIBRARY_CONFIGURATOR
 	if (gLibraryConfig && gLibraryConfig->pathIsExcluded(dirname)) {
 	  post("\texcluding dir: '%s'\n", dirname);
 	  return success;
 	}
#endif

 	if (level == 0) post("\tcompiling dir: '%s'\n", dirname);

	DIR *dir = opendir(dirname);	
	if (!dir) {
		error("open directory failed '%s'\n", dirname); fflush(stdout);
		return false;
	}
	
	for (;;) {
		struct dirent *de;
		
		de = readdir(dir);
		if (!de) break;
		
		if (sc_SkipDirectory(de->d_name)) continue;

		char *entrypathname = (char*)malloc(strlen(dirname) + strlen((char*)de->d_name) + 2);
		strcpy(entrypathname, dirname);
		//strcat(entrypathname, ":");
		strcat(entrypathname, "/");
		strcat(entrypathname, (char*)de->d_name);

        bool isDirectory = false;

#ifdef SC_DARWIN
		isDirectory = (de->d_type == DT_DIR);
#endif // SC_DARWIN
#ifdef SC_LINUX
		{
			isDirectory = sc_DirectoryExists(entrypathname);
		}
#endif // SC_LINUX

        if (isDirectory) {
            success = passOne_ProcessDir(entrypathname, level + 1);
        } else {
            success = passOne_ProcessOneFile(entrypathname, level + 1);
        }

		free(entrypathname);
		if (!success) break;
	}
	closedir(dir);
	return success;
}
#else
bool passOne_ProcessDir(char *dirname, int level)
{
	bool success = true;

#ifdef ENABLE_LIBRARY_CONFIGURATOR
 	if (gLibraryConfig && gLibraryConfig->pathIsExcluded(dirname)) {
 	  post("\texcluding dir: '%s'\n", dirname);
 	  return success;
 	}
#endif

 	if (level == 0) 
    post("\tcompiling dir: '%s'\n", dirname);

	char allInDir[_MAX_PATH];
  sprintf(allInDir,"%s\\*.*",dirname);
  
  WIN32_FIND_DATA findData;
  HANDLE hFind = ::FindFirstFile(allInDir, &findData);
  if (hFind == INVALID_HANDLE_VALUE) {
		error("open directory failed '%s'\n", dirname); fflush(stdout);
		return false;
	}
  
	do {
	  if (sc_SkipDirectory(findData.cFileName)) continue;
	  
    char *entrypathname = (char*)malloc(strlen(dirname) + strlen(findData.cFileName) + 2);
		strcpy(entrypathname, dirname);
		strcat(entrypathname, "\\");
		strcat(entrypathname, findData.cFileName);

    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      success = passOne_ProcessDir(entrypathname, level + 1);
    } else {
      success = passOne_ProcessOneFile(entrypathname, level + 1);
    }

		free(entrypathname);
		if (!success) 
      break;
  } while (::FindNextFile(hFind, &findData));
	return success;
}
#endif

#ifndef SC_WIN32
# include <unistd.h>
# include <sys/param.h>
#endif

// Get the Users home directory.

#if SC_WIN32
# include "win32_utils.h"
#endif

void sc_GetUserHomeDirectory(char *str, int size);
void sc_GetUserHomeDirectory(char *str, int size)
{
#ifndef SC_WIN32
  char *home = getenv("HOME");
  strncpy(str, home, size);
#else
  win32_GetHomeFolder(str,size);
#endif
}

// Get the System level 'Extensions' directory.

void sc_GetSystemExtensionDirectory(char *str, int size);
void sc_GetSystemExtensionDirectory(char *str, int size)
{
  strncpy(str, 
#ifdef SC_DARWIN
	  "/Library/Application Support/SuperCollider/Extensions",
#else
	  "/usr/local/share/SuperCollider/Extensions",
#endif
	  size);
}

// Get the System level 'Extensions' directory.

void sc_GetUserExtensionDirectory(char *str, int size);
void sc_GetUserExtensionDirectory(char *str, int size)
{
  char home[MAXPATHLEN];
  sc_GetUserHomeDirectory(home, MAXPATHLEN);

  snprintf(str, 
	   size, 
#ifdef SC_DARWIN
	   "%s/Library/Application Support/SuperCollider/Extensions",
#else
	   "%s/share/SuperCollider/Extensions",
#endif
	   home);
}

// Add a component to a path.

void sc_AppendToPath(char *path, const char *component);
void sc_AppendToPath(char *path, const char *component)
{
#ifndef SC_WIN32
  strcat(path, "/");
#else
  strcat(path, "\\");
#endif
  strcat(path, component);
}

// Locate directories to compile.

static void sc_InitCompileDirectories(void);
static void sc_InitCompileDirectories(void)
{
  getcwd(gCompileDir, MAXPATHLEN-32);
  sc_AppendToPath(gCompileDir,"SCClassLibrary");

  sc_GetSystemExtensionDirectory(gSystemExtensionDir, MAXPATHLEN);
  sc_GetUserExtensionDirectory(gUserExtensionDir, MAXPATHLEN);
}

bool passOne()
{
	bool success;
	initPassOne();
	// This function must be provided by the host environment.
	// It should choose a directory to scan recursively and call
	// passOne_ProcessOneFile(char *filename) for each file
	
	if (!gLibraryConfig) {
	        sc_InitCompileDirectories();

		success = passOne_ProcessDir(gCompileDir, 0);
		if (!success) return false;

		if(sc_DirectoryExists(gSystemExtensionDir)) {
		  success = passOne_ProcessDir(gSystemExtensionDir,0);
		  if (!success) return false;
		}

		if(sc_DirectoryExists(gUserExtensionDir)) {
		  success = passOne_ProcessDir(gUserExtensionDir,0);
		  if (!success) return false;
		}
	} else {
#ifdef ENABLE_LIBRARY_CONFIGURATOR
		success = gLibraryConfig->forEachIncludedDirectory(passOne_ProcessDir);
		if (!success) return false;
#endif // ENABLE_LIBRARY_CONFIGURATOR
	}

	finiPassOne();
	return true;
}

// true if filename ends in ".sc"
bool isValidSourceFileName(char *filename)
{
	int len = strlen(filename);
	return (len>3 && strncmp(filename+len-3, ".sc",3) == 0) 
            || (len>7 && strncmp(filename+len-7, ".sc.rtf",7) == 0);
}


#if 0
bool passOne_ProcessOneFile(char *filename)
{
	bool success = true;
	PyrSymbol *fileSym;
	if (isValidSourceFileName(filename)) {
		gNumCompiledFiles++;
		if (startLexer(filename)) {
			fileSym = getsym(filename);
			while (parseOneClass(fileSym)) { };
			finiLexer();
		} else {
			error("file '%s' open failed\n", filename);
			success = false;
		}
	}
	return success;
}
#endif

// sekhar's replacement
bool passOne_ProcessOneFile(char *filename, int level)
{
	bool success = true;

#ifdef ENABLE_LIBRARY_CONFIGURATOR
 	if (gLibraryConfig && gLibraryConfig->pathIsExcluded(filename)) {
 	  post("\texcluding file: '%s'\n", filename);
 	  return success;
 	}
#endif

	PyrSymbol *fileSym;
	if (isValidSourceFileName(filename)) {
		gNumCompiledFiles++;
		if (startLexer(filename)) {
			fileSym = getsym(filename);
			while (parseOneClass(fileSym)) { };
			finiLexer();
		} else {
			error("file '%s' open failed\n", filename);
			success = false;
		}
	} else {
#ifndef SC_WIN32
    // check if this is a symlink
		char realpathname[MAXPATHLEN];
		realpath(filename, realpathname);
		if (strncmp(filename, realpathname, strlen(filename))) {
			if (sc_DirectoryExists(realpathname))
				success = passOne_ProcessDir(realpathname, level);
		}
#else
    // under window, we're sure it's a file so wer don't do anything...
    // maybe processing .lnk files could be interesting...
    // $$$todo fixme add .lnk file parsing...
    // (see http://www.thecodeproject.com/managedcpp/mcppshortcuts.asp)
#endif
  }
	return success;
}

void InitSynthGlobals(VMGlobals *g, int inNumInputs, int inNumOutputs);
void InitSynthGlobals(VMGlobals *g, int inNumInputs, int inNumOutputs)
{
//	PyrObject *classvararray = g->classvars[s_synth->u.classobj->classIndex.ui].uo;
}

void schedRun();

void compileSucceeded();
void compileSucceeded()
{
	compiledOK = !(parseFailed || compileErrors);	
	if (compiledOK) {		
		compiledOK = true;
		
		compiledOK = initRuntime(gMainVMGlobals, 128*1024, pyr_pool_runtime);
			
		if (compiledOK) { 				
			VMGlobals *g = gMainVMGlobals;
			InitSynthGlobals(g,  2, 2);
			
                        g->canCallOS = true;
			//++g->sp; SetObject(g->sp, g->process);
			//runInterpreter(g, s_hardwaresetup, 1);
			
			++g->sp; SetObject(g->sp, g->process);
			runInterpreter(g, s_startup, 1);
                        g->canCallOS = false;

			schedRun();
		}
		flushPostBuf();
	}
}

void aboutToCompileLibrary();
void aboutToCompileLibrary()
{
	//printf("->aboutToCompileLibrary\n");
	pthread_mutex_lock (&gLangMutex);
	if (compiledOK) {
		++gMainVMGlobals->sp;
		SetObject(gMainVMGlobals->sp, gMainVMGlobals->process);
		runInterpreter(gMainVMGlobals, s_shutdown, 1);
	}
	pthread_mutex_unlock (&gLangMutex);
	//printf("<-aboutToCompileLibrary\n");
}

void closeAllGUIScreens();
void TempoClock_stopAll(void);

void shutdownLibrary();
void shutdownLibrary()
{
	closeAllGUIScreens();
	aboutToCompileLibrary();
	schedStop();
	TempoClock_stopAll();	
}

bool compileLibrary();
bool compileLibrary() 
{
	//printf("->compileLibrary\n");
	shutdownLibrary();

	pthread_mutex_lock (&gLangMutex);
	gNumCompiledFiles = 0;
	compiledOK = false;
        compileStartTime = elapsedTime();
	
	totalByteCodes = 0;
	
	postfl("compiling class library..\n");

	bool res = passOne();
	if (res) {
		
		postfl("\tpass 1 done\n");
		
		if (!compileErrors) {
			buildDepTree();
			traverseFullDepTree();
			traverseFullDepTree2();
			flushPostBuf();

			if (!compileErrors && gShowWarnings) {
				SymbolTable* symbolTable = gMainVMGlobals->symbolTable;
				symbolTable->CheckSymbols();
			}
		}
		pyr_pool_compile->FreeAll();
		flushPostBuf();
		compileSucceeded();	
	} else {
		compiledOK = false;
	}
	
	pthread_mutex_unlock (&gLangMutex);
	//printf("<-compileLibrary\n");
	return compiledOK;
}

void signal_init_globs();

void dumpByteCodes(PyrBlock *theBlock);

void runLibrary(PyrSymbol* selector) 
{
        VMGlobals *g = gMainVMGlobals;
        g->canCallOS = true;
	try {
		if (compiledOK) {
                        ++g->sp; SetObject(g->sp, g->process);
			runInterpreter(g, selector, 1);
		} else {
			postfl("Library has not been compiled successfully.\n");
		}
	} catch (std::exception &ex) {
		PyrMethod *meth = g->method;
		if (meth) {
			int ip = meth->code.uob ? g->ip - meth->code.uob->b : -1;
			post("caught exception in runLibrary %s-%s %3d\n", 
				meth->ownerclass.uoc->name.us->name, meth->name.us->name, ip
			);
			dumpByteCodes(meth);
		} else {
			post("caught exception in runLibrary\n");
		}
		error(ex.what());
	} catch (...) {
		postfl(BULLET"DANGER: OUT of MEMORY. Operation failed.\n");
	}
        g->canCallOS = false;
}

void interpretCmdLine(const char *textbuf, int textlen, char *methodname)
{
	PyrString *string;
	
	if (compiledOK) {
		PyrSlot slot;

		string = newPyrStringN(gMainVMGlobals->gc, textlen, 0, false);
		memcpy(string->s, textbuf, textlen);
		SetObject(&gMainVMGlobals->process->interpreter.uoi->cmdLine, string);
		gMainVMGlobals->gc->GCWrite(gMainVMGlobals->process->interpreter.uo, string);
		SetObject(&slot, gMainVMGlobals->process);
//#if __profile__
//		ProfilerInit(collectSummary, microsecondsTimeBase, 500, 100);
//#endif
		(++gMainVMGlobals->sp)->ucopy = slot.ucopy;
		runInterpreter(gMainVMGlobals, getsym(methodname), 1);
//#if __profile__
//		ProfilerDump("\pErase2.prof");
//		ProfilerTerm();
//#endif
	} else {
		postfl("Library has not been compiled successfully.\n");
	}
}

void init_SuperCollider()
{
}
