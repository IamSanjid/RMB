CC := g++
CFLAGS := -Wall -g
TARGET := RMB
INCLUDES = -Iglfw/include -Iimgui -Ilinux -IUtils -Iviews
LIBS = glfw/lib-linux-64/libglfw3.a -lglut -lGLU -lGL $(shell pkg-config --libs x11 xi xfixes) -lXtst -lm -lXxf86vm -lXrandr -lpthread -ldl

SRCS := $(wildcard *.cpp) $(wildcard imgui/*.cpp) $(wildcard linux/*.cpp) $(wildcard views/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)
%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
clean:
	rm -rf $(TARGET) *.o
	rm -rf imgui/*.o
	rm -rf linux/*.o
	rm -rf Utils/*.o
	rm -rf views/*.o
	
.PHONY: all clean