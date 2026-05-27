CXX = g++
CXXFLAGS = -std=c++17 -O1 -I. -I./vendor
LDFLAGS = -lcrypto -lssl -lpthread

SRCS = vfs_core.cpp cid.cpp vfs_primitives.cpp
OBJS = $(SRCS:.cpp=.o)

TESTS = test_pubsub test_links

all: test_server $(TESTS)

test_server: test_server.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

test_pubsub: test/vfs_pubsub_test.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

test_links: test/vfs_links_test.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

test_node: $(TESTS)
	./test_pubsub
	./test_links

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o vfs_node test_server $(TESTS)
