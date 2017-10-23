
OPTS= $(shell sdl2-config --cflags) -I. -I.. -Wall
LIBS= $(shell sdl2-config --libs)

ifeq ($(shell uname), Linux)
	LIBS += -lGL -lGLEW
endif

ifeq ($(shell uname), Darwin)
	LIBS += -framework OpenGL
endif

example : example.o ../obj.o
	c++ $(OPTS) -o $@ $^ $(LIBS)

%.o : %.cpp
	c++ $(OPTS) -o $@ -c $<

%.o : %.c
	cc  $(OPTS) -o $@ -c $<

clean :
	rm example example.o ../obj.o
