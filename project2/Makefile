CC = g++
CPPFLAGS = -std=c++17
TARGET = run

$(TARGET): run.o snapshot.o
	$(CC) $(CPPFLAGS) -o $(TARGET) run.o snapshot.o -lpthread
	rm *.o

clean: 
	rm *.o
