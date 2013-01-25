CFLAGS = -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
bin_PROGRAMS = fatback
fatback_SOURCES = fatback.c mbr.c vbr.c util.c sig.c output.c input.c recovery.c \
dirtree.c lfn.c fat.c interface.c interface_data.c vars.c cmd_chain.c cmd_cp.c\
cmd_sh.c cmd_misc.c cmd_ls.c cmd_stat.c cmd_lostchains.c cmd_set.c sety.y \
setl.l getopt.c getopt1.c cmd_cpchain.c fatback.h mbr.h vbr.h util.h sig.h \
output.h input.h dirtree.h recovery.h lfn.h fat.h interface.h interface_data.h \
vars.h getopt.h sety.h
fatback_LDADD = @LEXLIB@
fatback_TEXINFOS = fatback-manual.texi
man_MANS = fatback.1
info_TEXINFOS = $(fatback_TEXINFOS)

EXTRA_DIST = $(fatback_TEXINFOS) $(man_MANS) pseudo_code situations

YFLAGS = -d
