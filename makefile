CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 
TARGET = a
OBJS = ./http/*.cpp  ./log/*.cpp ./mySQL/*.cpp  *.cpp

a: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET) -pthread -lmysqlclient

clean:
	rm  -r a
