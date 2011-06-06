# Name of the target file
target = dagdb.so
target_test = test_dagdb

# The list of used packages/libraries (uses pkg-config names)
libraries = 

# Name of the source code directory
sourcedir = 

testdir = 

# list of source code files
define sourcefiles 
base.c
endef

define testfiles
test.c
endef

# Add libraries that are not in pkg-config
LDFLAGS += -lgcrypt 

# Add c compile flags 
CFLAGS += -Wall -Wno-unused-result -D_FORTIFY_SOURCE=2 -g

# Tell that we want fancy console output
fancy=1

###############################################################################
# Generic makefile for building a single library from multiple sourcefiles.
###############################################################################
# - Supported source file types are *.c.
# - Supports out-of-source build using the VPATH feature and the following 
# linking-makefile:
# 	root = ..
# 	VPATH = $(root)
# 	include $(root)/Makefile
# - Creates a dependency makefile for each source file.
# - Has a 'fancy' formatting that outputs a progress indicator and step 
# description, instead of lengthy and unreadable command line. Disable these 
# fancy lines with 'make fancy=0', print command lines with 'make VERBOSE=1'.
# - Clean does not update the dependency files and only removes the files that
# can be recreated using this makefile.
###############################################################################

# Add the flags of the libraries
ifneq "$(libraries)" ""
 LDFLAGS := $(shell pkg-config --libs $(libraries)) $(LDFLAGS)
 CFLAGS := $(shell pkg-config --cflags $(libraries)) $(CFLAGS)
endif

# Obtain the (relative) directory of this makefile
curdir := $(subst Makefile,,$(lastword $(MAKEFILE_LIST)))

# Add the src and build directory to the search path
CFLAGS := $(CFLAGS) -I$(abspath $(curdir)$(sourcedir)) -I$(abspath $(sourcedir))

# Convert newlines to spaces and prepend source folder path
sourcefiles := $(addprefix $(sourcedir),$(strip $(sourcefiles)))
testfiles   := $(addprefix $(testdir),$(strip $(testfiles)))

# append .o to each sourcefile name to obtain objectfiles
objectfiles := $(addsuffix .o,$(sourcefiles) )

# append .o to each sourcefile name to obtain objectfiles
objectfiles_test := $(addsuffix .o,$(testfiles) )

# append .d to each sourcefile and testfile name dmakefiles
dmakefiles := $(addsuffix .d,$(sourcefiles) $(testfiles) )

all: $(target) $(target_test)
test: $(target_test)
	env LD_LIBRARY_PATH=. ./$(target_test)

# use fancy output if requested.
fancy ?= 0
ifneq "$(fancy)" "0"
 ifeq "$(or $(MAKECMDGOALS),all)" "all"
  $(VERBOSE).SILENT:
  ifndef progress
   stepcount:=$(shell $(MAKE) $(MAKECMDGOALS) $(MFLAGS) -n progress=\#STEP | grep "\#STEP" | wc -l )
   ifneq "$(stepcount)" "0"
    $(info $()       There are $(stepcount) pending steps.)
   endif
   stepnr:=0
   define progress 
    $(eval stepnr := $(shell expr $(stepnr) '+' 1))
    $(eval stepprogress := $(shell expr 100 '*' $(stepnr) '/' $(stepcount) ) )
    $(eval stepmessage := $(subst $(curdir),,$(subst $(sourcedir),,$(1))) )
    @$(if $(stepprogress), printf "[%3d%%] %s\n" $(stepprogress) "$(stepmessage)", printf "[ ? %%] %s\n" "$(stepmessage)")
   endef
  endif
  dep_progress=+echo "[  0%] Calculating dependencies" >&2 $(eval dep_progress:=)
 endif
endif

# create target from source files
$(target): $(objectfiles)
	$(call progress,Linking '$@')
	@$(CC) $(LDFLAGS) $(objectfiles) --shared -o $@

# create test from test files and target
$(target_test): $(objectfiles_test) $(target)
	$(call progress,Linking '$@')
	@$(CC) $(LDFLAGS) $(target) $(objectfiles_test) -o $@

# creates the parent directories for the output file
define makedir
@mkdir -p $(dir $@)
endef

# Auto prerequisite makefile rule:
# Tell make how to create a .d file from a .cpp file
define build-prerequisite
$(makedir)
$(dep_progress)
$(CC) $(CPPFLAGS) $(CFLAGS) -MM $< | sed 's,^.*\?\.o[ :]*,$(@:.d=.o) $@ : ,g' > $@
endef

%.c.d: %.c
	$(build-prerequisite)
	
# Tell how to compile c source code
define build-c
$(makedir)
$(eval shortfilepath := $(subst $(curdir),,$(subst $(sourcedir),,$(<))))
$(call progress,Compiling c source '$(shortfilepath)')
cd $(subst $(shortfilepath),,$(abspath $<)) && $(CC) $(CPPFLAGS) $(CFLAGS) -c $(shortfilepath) -o $(abspath $@)
endef

%.c.o: %.c
	$(build-c)

# Include will trigger .d files to be updated by the prerequisite rule if necessary.
# Any warnings are suppressed. If dependency file generation failed, so will the 
# compilation step.
ifneq "$(MAKECMDGOALS)" "clean"
-include $(dmakefiles)
endif

# Force build the all target.
.PHONY: all test clean

# Don't use unused implicit rules.
.SUFFIXES: 
%: %,v
%: s.%
%: RCS/%
%: RCS/%,v
%: SCCS/s.%

# Suppress implicit dependency checking of source files.
# And tell when they're missing
Makefile $(addprefix $(curdir),$(sourcefiles)):
	$(if $(wildcard $@),,$(error Can't create source file $@))

# The clean target:
clean:
	$(eval CLEAN_FILES := $(wildcard $(objectfiles) $(dmakefiles) $(addsuffix .pb.h,$(protobufbases) ) $(addsuffix .pb.cc,$(protobufbases) ) ) )
	$(if $(CLEAN_FILES),-$(RM) $(CLEAN_FILES))
	$(eval CLEAN_DIRS := $(wildcard $(sort $(dir $(sourcefiles)))) )
	$(if $(CLEAN_DIRS),-rmdir --ignore-fail-on-non-empty -p $(CLEAN_DIRS) 2>/dev/null)

