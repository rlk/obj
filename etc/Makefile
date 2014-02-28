
OPTS= $(shell /usr/local/bin/sdl2-config --cflags) -I. -I.. -Wall
LIBS= $(shell /usr/local/bin/sdl2-config --libs) -framework OpenGL

example : example.o ../obj.o
	c++ $(OPTS) -o $@ $^ $(LIBS)

%.o : %.cpp
	c++ $(OPTS) -o $@ -c $<

%.o : %.c
	cc  $(OPTS) -o $@ -c $<

clean :
	rm example example.o ../obj.o
