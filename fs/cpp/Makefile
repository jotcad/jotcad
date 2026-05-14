CXX = g++
CXXFLAGS = -std=c++17 -O3 -I. -I./vendor
LDFLAGS = -lcrypto -lssl -lpthread

SRCS = vfs_core.cpp cid.cpp vfs_primitives.cpp
OBJS = $(SRCS:.cpp=.o)

all: test_server

test_server: test_server.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o vfs_node test_server
