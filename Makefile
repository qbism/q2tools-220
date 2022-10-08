#
# Linux Makefile for q2tools
#


# BUILD = RELEASE or DEBUG
BUILD ?= RELEASE

# Where programs will be installed on 'make install'
INSTALL_DIR ?= ./install

# Compiler
CC ?= gcc

# Compile Options
WITH_PTHREAD ?= -pthread -DUSE_PTHREADS
WITHOUT_PTHREAD ?= -DUSE_SETRLIMIT
# THREADING_OPTION = WITH_PTHREAD or WITHOUT_PTHREAD
THREADING_OPTION ?= $(WITH_PTHREAD)
BASE_CFLAGS ?= -fno-common -Wall -Wno-unused-result -Wno-strict-aliasing $(THREADING_OPTION) -DUSE_ZLIB -Wl,-z,stack-size=16777216
RELEASE_CFLAGS ?= $(BASE_CFLAGS) -O3 
DEBUG_CFLAGS ?= $(BASE_CFLAGS) -O0 -g -ggdb

# Link Options
LDFLAGS ?= -lm -lz

#
srcdir = .
srcbsp = bsp/
includedirs = -Icommon
# depdir = .deps

# source locations
vpath %.h 4bsp
vpath %.c 4bsp
vpath %.h 4vis
vpath %.c 4vis
vpath %.h 4rad
vpath %.c 4rad
vpath %.h 4data
vpath %.c 4data
vpath %.h common
vpath %.c common

ifeq ($(BUILD),DEBUG)
CFLAGS ?= $(DEBUG_CFLAGS)
builddir = debug
vpath %.o debug
else
CFLAGS ?= $(RELEASE_CFLAGS)
builddir = release
vpath %.o release
endif

# common files 
cmn_srcs = \
	bspfile.c	\
	cmdlib.c	\
	l3dslib.c	\
	llwolib.o	\
	lbmlib.c	\
	mathlib.c	\
	mdfour.c	\
	polylib.c	\
	scriplib.c	\
	trilib.c	\
	threads.c

cmn_objs = $(cmn_srcs:.c=.o)

# cmn_dep = $(addprefix $(depdir)/, $(cmn_srcs:.c=.d) )


#
# files for 4bsp
#
4bsp_srcs =	\
    4bsp.c     \
	brushbsp.c	\
	faces.c		\
	leakfile.c	\
	4bsp_main.c \
	map.c		\
	portals.c	\
	prtfile.c	\
	textures.c	\
	tree.c		\
	writebsp.c	\
	csg.c

4bsp_cmnobjs = \
	bspfile.o	\
	cmdlib.o	\
	lbmlib.o	\
	mathlib.o	\
	polylib.o	\
	scriplib.o	\
	threads.o

4bsp_objs = $(4bsp_srcs:.c=.o)
4bsp_objs_all = $(4bsp_objs) $(4bsp_cmnobjs)
# 4bsp_dep = $(addprefix $(depdir)/, $(4bsp_srcs:.c=.d) )

#
# files for 4vis
#
4vis_srcs =	\
	4vis.c		\
	flow.c \
	4vis_main.c

4vis_cmnobjs =	\
	bspfile.o	\
	cmdlib.o	\
	mathlib.o	\
	scriplib.o	\
	threads.o

4vis_objs = $(4vis_srcs:.c=.o)
4vis_objs_all = $(4vis_objs) $(4vis_cmnobjs)
# 4vis_dep = $(addprefix $(depdir)/, $(4vis_srcs:.c=.d) )


#
# files for 4data
#
4data_srcs =	\
	images.c	\
	models.c	\
	4data.c	\
	sprites.c	\
	tables.c	\
	video.c

4data_cmnobjs =	\
	bspfile.o	\
	cmdlib.o	\
	lbmlib.o	\
	l3dslib.o	\
	llwolib.o	\
	mathlib.o	\
	mdfour.o	\
	polylib.o	\
	scriplib.o	\
	trilib.o	\
	threads.o

4data_objs = $(4data_srcs:.c=.o)
4data_objs_all = $(4data_objs) $(4data_cmnobjs)
# 4data_dep = $(addprefix $(depdir)/, $(4data_srcs:.c=.d) )


#
# files for 4rad
#
4rad_srcs =	\
	4rad.c		\
	lightmap.c	\
	4rad_main.c \
	patches.c 	\
	trace.c

4rad_cmnobjs = \
	bspfile.o	\
	cmdlib.o	\
	lbmlib.o	\
	mathlib.o	\
	polylib.o	\
	scriplib.o	\
	threads.o

4rad_objs = $(4rad_srcs:.c=.o)
4rad_objs_all = $(4rad_objs) $(4rad_cmnobjs)
# 4rad_dep = $(addprefix $(depdir)/, $(4rad_srcs:.c=.d) )

#
# files for merged tools q2tools-220
#
q2tools_220_srcs =	\
	main.c

q2tools_220_cmnobjs = \
	bspfile.o	\
	cmdlib.o	\
	l3dslib.o	\
	llwolib.o	\
	lbmlib.o	\
	mathlib.o	\
	mdfour.o	\
	polylib.o	\
	scriplib.o	\
	trilib.o	\
	threads.o \
  4bsp.o     \
	brushbsp.o	\
	faces.o		\
	leakfile.o	\
	map.o		\
	portals.o	\
	prtfile.o	\
	textures.o	\
	tree.o		\
	writebsp.o	\
	csg.o \
	4vis.o		\
	flow.o \
	4rad.o		\
	lightmap.o	\
	patches.o 	\
	trace.o

q2tools_220_objs = $(q2tools_220_srcs:.c=.o)
q2tools_220_objs_all = $(q2tools_220_objs) $(q2tools_220_cmnobjs)

#
# --- targets ---
#

all: subdirectories $(builddir)/4bsp $(builddir)/4vis $(builddir)/4rad $(builddir)/4data $(builddir)/q2tools-220

# dependency (.d) files
include $(cmn_dep)
## include $(bspinfo3_dep)
include $(4bsp_dep)
include $(4vis_dep)
include $(4rad_dep)
include $(4data_dep)

# compile common sources
$(cmn_objs): %o : %c
	$(CC) -c $(includedirs) $(CFLAGS) $< -o $(builddir)/$@


# link 4bsp
$(builddir)/4bsp: $(4bsp_objs_all)
	$(CC) $(CFLAGS) -DBSP $(addprefix $(builddir)/, $(4bsp_objs_all)) -o $(builddir)/4bsp $(LDFLAGS)

# compile 4bsp sources
$(4bsp_objs): %o : %c
	$(CC) -c $(includedirs) $(CFLAGS) $< -o $(builddir)/$@

# link 4vis
$(builddir)/4vis: $(4vis_objs_all)
	$(CC) $(CFLAGS) $(addprefix $(builddir)/, $(4vis_objs_all)) -o $(builddir)/4vis $(LDFLAGS)

# compile 4vis sources
$(4vis_objs): %o : %c
	$(CC) -c $(includedirs) $(CFLAGS) $< -o $(builddir)/$@

# link 4rad
$(builddir)/4rad: $(4rad_objs_all)
	$(CC) $(CFLAGS) $(addprefix $(builddir)/, $(4rad_objs_all)) -o $(builddir)/4rad $(LDFLAGS)

# compile 4rad sources
$(4rad_objs): %o : %c
	$(CC) -c $(includedirs) $(CFLAGS) $< -o $(builddir)/$@

# link 4data
$(builddir)/4data: $(4data_objs_all)
	$(CC) $(CFLAGS) $(addprefix $(builddir)/, $(4data_objs_all)) -o $(builddir)/4data $(LDFLAGS)

# compile 4data sources
$(4data_objs): %o : %c
	$(CC) -c $(includedirs) $(CFLAGS) $< -o $(builddir)/$@

# link q2tools-220
$(builddir)/q2tools-220: $(q2tools_220_objs_all)
	$(CC) $(CFLAGS) $(addprefix $(builddir)/, $(q2tools_220_objs_all)) -o $(builddir)/q2tools-220 $(LDFLAGS)

# compile q2tools-220 sources
$(q2tools_220_objs): %o : %c
	$(CC) -c $(includedirs) $(CFLAGS) $< -o $(builddir)/$@

subdirectories:
#	mkdir -p $(depdir)
	mkdir -p $(builddir)

clean:
	rm -f $(builddir)/*.o
	rm -f $(builddir)/4bsp
	rm -f $(builddir)/4vis
	rm -f $(builddir)/4rad
	rm -f $(builddir)/4data
	rm -f $(builddir)/q2tools-220

# Add these to remove executables from install directory

#	rm -f $(INSTALL_DIR)/4bsp
#	rm -f $(INSTALL_DIR)/4vis
#	rm -f $(INSTALL_DIR)/4rad
#	rm -f $(INSTALL_DIR)/4data

install:
	mkdir -p $(INSTALL_DIR)
	cp $(builddir)/4bsp   $(INSTALL_DIR)
	cp $(builddir)/4vis   $(INSTALL_DIR)
	cp $(builddir)/4rad   $(INSTALL_DIR)
	cp $(builddir)/4data   $(INSTALL_DIR)
	cp $(builddir)/q2tools-220 $(INSTALL_DIR)

