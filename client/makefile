CFLAGS:=-c -Wall -Weffc++ -g -std=c++11 -Iinclude
LDFLAGS:=-lboost_system -lpthread

# יעד ברירת מחדל
all: bin/StompWCIClient

# יצירת קובץ ההרצה - Linking
bin/StompWCIClient: obj/StompClient.o obj/ConnectionHandler.o obj/StompProtocol.o obj/event.o
	@mkdir -p bin
	g++ -o bin/StompWCIClient obj/StompClient.o obj/ConnectionHandler.o obj/StompProtocol.o obj/event.o $(LDFLAGS)

# קומפילציה של הקבצים הבודדים - Compilation
obj/StompClient.o: src/StompClient.cpp
	@mkdir -p obj
	g++ $(CFLAGS) -o obj/StompClient.o src/StompClient.cpp

obj/ConnectionHandler.o: src/ConnectionHandler.cpp
	@mkdir -p obj
	g++ $(CFLAGS) -o obj/ConnectionHandler.o src/ConnectionHandler.cpp

obj/StompProtocol.o: src/StompProtocol.cpp
	@mkdir -p obj
	g++ $(CFLAGS) -o obj/StompProtocol.o src/StompProtocol.cpp

obj/event.o: src/event.cpp
	@mkdir -p obj
	g++ $(CFLAGS) -o obj/event.o src/event.cpp

.PHONY: clean
clean:
	rm -rf bin/* obj/*