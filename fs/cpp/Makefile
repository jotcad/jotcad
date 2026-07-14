CXX = g++
CXXFLAGS = -std=c++17 -O1 -MMD -MP -I. -I./vendor -I../../geo/cgal/zenoh/include
LDFLAGS = ../../geo/cgal/zenoh/lib/libzenohc.a -lcrypto -lssl -lpthread -ldl

SRCS = vfs_core.cpp cid.cpp vfs_primitives.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

TESTS = test_pubsub test_links

all: test_server

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
	rm -f *.o *.d vfs_node test_server $(TESTS)

-include $(DEPS)
