#ifndef YYERRCODE
#define YYERRCODE 256
#endif

#define NUMBER 257
#define WORD 258
typedef union {
     char *string;
} YYSTYPE;
extern YYSTYPE yylval;
