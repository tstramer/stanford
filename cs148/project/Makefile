# the name of the compiled binary

T1 := game

# list files to compile and link together
FILES1   := main simple_image simple_texture stb mesh game_object player_object scene coin_object enemy_object

#################################################################


# The following Makefile rules should work for Linux or Cygwin
CC		 := g++
LD		 := g++
OBJSUFFIX	 := .o
LIBPREFIX	 := lib
STATIC_LIBSUFFIX := .a
CFLAGS 		 := -g -std=c++11
CFLAGS_PLATFORM  := 
LDFLAGS		 := 
FRAMEWORKS	 := 
LIBS		 :=

ARCH=$(shell uname | sed -e 's/-.*//g')

ifeq ($(ARCH), CYGWIN_NT)

# if we're building in cygwin, we'll need to use the
# win32 versions of gl and glut
EXESUFFIX  := .exe
LIBS  	   += glut32 opengl32
CFLAGS_PLATFORM += 

else

ifeq ($(ARCH),Darwin)

# we're building on the mac

EXESUFFIX  :=
FRAMEWORKS += OpenGL GLUT 
CFLAGS_PLATFORM += -Wno-deprecated-declarations

EXTRA_LIB_DIRS := 
EXTRA_INC_DIRS  := 

# 
# The default location of a fink install is given below.
# 
#EXTRA_LIB_DIRS := /sw/lib /opt/local/lib
#EXTRA_INC_DIRS  := /sw/include /opt/local/include

else

# building on Linux

FILES1 += myth_workaround
LIBS += pthread
EXESUFFIX  :=
LIBS	   += glut GL GLU
CFLAGS_PLATFORM += 

#
# hack for myth machines.  Add /usr/lib as an explicit lib dir so
# it gets picked up instead of /usr/pubsw/lib.
# 

EXTRA_LIB_DIRS := /usr/lib
EXTRA_INC_DIRS :=

endif
endif


TARGET     := $(addsuffix $(EXESUFFIX), $(TARGET))

INCDIRS    := .  $(EXTRA_INC_DIRS)
LIBDIRS    := $(EXTRA_LIB_DIRS)

CFLAGS     += $(addprefix -I, $(INCDIRS))
CFLAGS	   += $(CFLAGS_PLATFORM)
LDFLAGS    += $(addprefix -L, $(LIBDIRS))

LDLIBS	   := $(addprefix -l, $(LIBS))
LDFRAMEWORKS := $(addprefix -framework , $(FRAMEWORKS))

OBJS1       :=  $(addsuffix $(OBJSUFFIX), $(FILES1))
OBJS3       :=  $(addsuffix $(OBJSUFFIX), $(FILES3))
OBJS3bf       :=  $(addsuffix $(OBJSUFFIX), $(FILES3bf))
OBJS6       :=  $(addsuffix $(OBJSUFFIX), $(FILES6))


.SUFFIXES : .cpp $(OBJSUFFIX)

.PHONY : clean release all

all: one

one: $(T1)

$(T1) : $(OBJS1)
	$(LD) -o $(T1) $(OBJS1) $(LDFLAGS) $(LDLIBS) $(LDFRAMEWORKS)

%.o : %.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf *$(OBJSUFFIX) $(TARGET) *~ .#* #*
	rm -rf *$(OBJSUFFIX) $(T1) *~ .#* #*
	rm -rf *$(OBJSUFFIX) $(T3) *~ .#* #*
	rm -rf *$(OBJSUFFIX) $(T3bf) *~ .#* #*
	rm -rf *$(OBJSUFFIX) $(T6) *~ .#* #*

release:
	@make --no-print-directory RELEASE=1
