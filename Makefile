# fusionwm version
VERSION = 0.4

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

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo " LD " $(TARGET)
	@$(LD) -o $@ $(LDFLAGS)  $(OBJ) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADER)
	@echo " CC " $<
	@$(CC) -c $(CFLAGS) -o $@ $<

${OBJ}: config/config.h config/binds.h

clean:
	@echo " RM " $(TARGET)
	@rm -f $(TARGET)
	@echo " RM " $(OBJ)
	@rm -f $(OBJ)

info:
	@echo Some Info:
	@echo Compiling with: $(CC) -c $(CFLAGS) -o OUT INPUT
	@echo Linking with: $(LD) -o OUT $(LDFLAGS) INPUT

.PHONY: all clean info
