/*
 * (c) copyright 1983 by the Vrije Universiteit, Amsterdam, The Netherlands.
 *
 *          This product is part of the Amsterdam Compiler Kit.
 *
 * Permission to use, sell, duplicate or disclose this software must be
 * obtained in writing. Requests for such permissions may be sent to
 *
 *      Dr. Andrew S. Tanenbaum
 *      Wiskundig Seminarium
 *      Vrije Universiteit
 *      Postbox 7161
 *      1007 MC Amsterdam
 *      The Netherlands
 *
 */

/*
 *  L L G E N
 *
 *  An Extended LL(1) Parser Generator
 *
 *  Author : Ceriel J.H. Jacobs
 */

/*
 * global.c
 * Contains declarations visible in several other source files
 */

# include "types.h"
# include "extern.h"
# include "io.h"

# ifndef NORCSID
static string rcsid4 = "$Header$";
# endif

char	ltext[LTEXTSZ];
p_nont	nonterms;
p_nont	maxnt;
int	nnonterms;
p_token	tokens;
p_token	maxt;
int	ntokens;
p_order	porder, sorder;
p_start	start;
int	linecount;
int	assval;
FILE	*fout;
FILE	*fpars;
FILE	*finput;
FILE	*fact;
string	f_pars = PARSERFILE;
string	f_out = OUTFILE;
string	f_temp = ACTFILE;
string	f_input;
string	e_noopen = "Cannot open %s";
int	verbose;
string	lexical;
int	ntneeded;
int	ntprint;
# ifndef NDEBUG
int	debug;
# endif not NDEBUG
p_file	files;
p_file	maxfiles;
p_file	pfile;
string	LLgenid = "/* LLgen generated code from source %s */\n";
t_token lextoken;
int	nerrors;
string	rec_file, incl_file;
p_link	links;
p_term	terms;
