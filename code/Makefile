# Makefile for the Dynamic Routing lab
# ------------------------------------------------------------------------------
# make         -- builds the shared library which handles the dynamic routing
# make clean   -- clean up byproducts

ME = Makefile

# utility programs used by this Makefile
CC   = g++
MAKE = make --no-print-directory

# set system-dependent variables
OSTYPE = $(shell uname)
ifeq ($(OSTYPE),Linux)
ARCH=-D_LINUX_
ENDIAN=-D_LITTLE_ENDIAN_
endif
ifeq ($(OSTYPE),SunOS)
ARCH=-D_SOLARIS_
ENDIAN=-D_BIG_ENDIAN_
endif
ifeq ($(OSTYPE),Darwin)
ARCH = -D_DARWIN_
endif

# define names of our build targets
LIB_DR = libdr.so

# compiler and its directives
DIR_INC       =
DIR_LIB       =
LIBS          = -lpthread
FLAGS_CC_BASE = -c -fPIC -Wall $(ARCH) $(ENDIAN) $(DIR_INC)

# compiler directives for debug and release modes
BUILD_TYPE = debug
ifeq ($(BUILD_TYPE),debug)
FLAGS_CC_BUILD_TYPE = -g -D_DEBUG_
else
FLAGS_CC_BUILD_TYPE = -O3
endif

# put all the flags together
CFLAGS = $(FLAGS_CC_BASE) $(FLAGS_CC_BUILD_TYPE)

# project sources
SRCS = dr_api.c rmutex.c
OBJS = $(patsubst %.c,%.o,$(SRCS))
DEPS = $(patsubst %.c,.%.d,$(SRCS))

# include the dependencies once we've built them
ifdef INCLUDE_DEPS
include $(DEPS)
endif

#########################
## PHONY TARGETS
#########################
# note targets which don't produce a file with the target's name
PHONY=phony
.PHONY: all clean clean-all clean-deps debug deps release submit $(LIB_DR).$(PHONY)

# build the program
all: $(LIB_DR)

# clean up by-products (except dependency files)
clean:
	rm -f $(OBJS) $(LIB_DR)

# clean up all by-products
clean-all: clean clean-deps

# clean up dependency files
clean-deps:
	rm -f $(DEPS)

# shorthand for building debug or release builds
debug release:
	@$(MAKE) BUILD_TYPE=$@ all

# build the dependency files
deps: $(DEPS)

# make a tarball of the source files
submit:
	tar zcvf "lab-dr.tar.gz" *.c *.h Makefile README

# includes are ready build command
$(LIB_DR).$(PHONY): $(OBJS)
	$(CC) -shared -o $(LIB_DR) $(OBJS) $(DIR_LIB) $(LIBS)

#########################
## REAL TARGETS
#########################
$(LIB_DR): deps
	@$(MAKE) -f $(ME) BUILD_TYPE=$(BUILD_TYPE) INCLUDE_DEPS=1 $@.$(PHONY)

$(DEPS): .%.d: %.c
	$(CC) -MM $(CFLAGS) $(DIRS_INC) $< > $@
