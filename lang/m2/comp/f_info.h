/* F I L E   D E S C R I P T O R   S T R U C T U R E */

/* $Header$ */

struct f_info {
	unsigned short f_lineno;
	char *f_filename;
	char *f_workingdir;
};

extern struct f_info file_info;
#define LineNumber file_info.f_lineno
#define FileName file_info.f_filename
