cc = gcc
CFLAGS = -Wall -std=c11 -fPIC `pkg-config --cflags python3`

all: caltool cal.so	
	chmod +x xcal.py

caltool: calutil.o caltool.o
calutil.o: calutil.c calutil.h
caltool.o: caltool.c caltool.h
cal.so: calmodule.o calutil.o
	$(cc) -shared $^ $(CFLAGS) -o CalModule.so
calmodule.o: calmodule.c calutil.h
clean: 
	rm -rf *.o *.so caltool
