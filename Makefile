CC := g++
CFLAGS := -Wall -std=c++20
TARGET := RMB
INCLUDES = -Iglfw/include -Iimgui -Ilinux -IUtils -Iviews
LIBS = glfw/lib-linux-64/libglfw3.a -lGL $(shell pkg-config --libs x11 xi xfixes xtst) -lm -lpthread -ldl

SRCS := $(wildcard *.cpp) $(wildcard imgui/*.cpp) $(wildcard linux/*.cpp) $(wildcard views/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

ifeq ($(BUILD),debug)   
# "Debug" build - no optimization, and debugging symbols
CFLAGS += -O0 -g -D_DEBUG=1
else
# "Release" build - optimization, and no debug symbols
CFLAGS += -O2 -s -DNDEBUG
endif

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)
%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
clean:
	rm -rf $(TARGET) $(OBJS) *.ini
	
.PHONY: all clean