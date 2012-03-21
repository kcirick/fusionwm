VERSION = 0.2.1

# paths
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -Iinclude/ -Iconfig/ -I/usr/include -I${X11INC}  `pkg-config --cflags glib-2.0`
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11 `pkg-config --libs glib-2.0` -lrt -lXinerama

# FLAGS
LD = gcc
CC = gcc
CFLAGS = -g -std=c99 -pedantic -Wall ${INCS} -D _XOPEN_SOURCE=600 -DXINERAMA
CFLAGS += -DVERSION=\"$(VERSION)\" 
LDFLAGS = -g

# project
SRCDIR = src
HDRDIR = include
OBJDIR = obj
SRC = $(wildcard $(SRCDIR)/*.c)
HEADER = $(wildcard $(HDRDIR)/*.h)
OBJ = $(addprefix $(OBJDIR)/,$(notdir $(SRC:.c=.o)))
TARGET = fusionwm

# colors
TPUT = tput
COLOR_CLEAR   = `$(TPUT) sgr0`
COLOR_NORMAL  = $(COLOR_CLEAR)
COLOR_ACTION  = `$(TPUT) bold``$(TPUT) setaf 3`
COLOR_FILE    = `$(TPUT) bold``$(TPUT) setaf 2`
COLOR_BRACKET = $(COLOR_CLEAR)`$(TPUT) setaf 4`
define colorecho
   @echo $(COLOR_BRACKET)"  ["$(COLOR_ACTION)$1$(COLOR_BRACKET)"]  " $(COLOR_FILE)$2$(COLOR_BRACKET)... $(COLOR_NORMAL)
endef

all: $(TARGET)

$(TARGET): $(OBJ)
	$(call colorecho,LD,$(TARGET))
	@$(LD) -o $@ $(LDFLAGS)  $(OBJ) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADER)
	$(call colorecho,CC,$<)
	@$(CC) -c $(CFLAGS) -o $@ $<

clean:
	$(call colorecho,RM,$(TARGET))
	@rm -f $(TARGET)
	$(call colorecho,RM,$(OBJ))
	@rm -f $(OBJ)

info:
	@echo Some Info:
	@echo Compiling with: $(CC) -c $(CFLAGS) -o OUT INPUT
	@echo Linking with: $(LD) -o OUT $(LDFLAGS) INPUT

.PHONY: all clean info
