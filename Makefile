# filter server make file
#

CFLAGS += -Wall -pipe -c -D_REENTRANT  -O3

OBJECTS = utilities.o listener_socket.o filter_server.o

.PHONY : all  clean  uninstall

all : filter_server

install : /usr/local/bin/filter_server  Makefile

/usr/local/bin/filter_server : filter_server  Makefile
	sudo cp -f filter_server /usr/local/bin/filter_server

filter_server : $(OBJECTS)  Makefile
	g++ -Wall -pipe -o filter_server  $(OBJECTS)

utilities.o : utilities.h utilities.cpp  Makefile
	g++ $(CFLAGS) utilities.cpp

listener_socket.o : listener_socket.h listener_socket.cpp  Makefile
	g++ $(CFLAGS) listener_socket.cpp

filter_server.o : utilities.h  listener_socket.h  filter_server.cpp  Makefile
	g++ $(CFLAGS) filter_server.cpp

clean :
	rm -f *.o *~

uninstall :
	rm -f filter_server

# end
