
LIBS = `pkg-config --libs --cflags gtk+-3.0 gdk-x11-3.0 vte-2.91 x11`
OUT = fuckterm

all:
	gcc -o $(OUT) sauce/*.c -O2 $(LIBS)

install: all
	install $(OUT) /usr/local/bin
