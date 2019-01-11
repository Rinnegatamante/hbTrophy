TARGET          := libhbTrophy
SOURCES         := source

LIBS = -lc -lm -lSceGxm_stub -lSceDisplay_stub

CPPFILES   := $(foreach dir,$(SOURCES), $(wildcard $(dir)/*.cpp))
OBJS     := $(CPPFILES:.cpp=.o)

PREFIX  = arm-vita-eabi
CXX      = $(PREFIX)-g++
AR      = $(PREFIX)-gcc-ar
CFLAGS  = -g -Wl,-q -O2 -ffast-math -mtune=cortex-a9 -flto
ASFLAGS = $(CFLAGS)

all: $(TARGET).a

$(TARGET).a: $(OBJS)
	$(AR) -rc $@ $^
	
clean:
	@rm -rf $(TARGET).a $(TARGET).elf $(OBJS)
	
install: $(TARGET).a
	@mkdir -p $(VITASDK)/$(PREFIX)/lib/
	cp $(TARGET).a $(VITASDK)/$(PREFIX)/lib/
	@mkdir -p $(VITASDK)/$(PREFIX)/include/
	cp source/hbTrophy.h $(VITASDK)/$(PREFIX)/include/
