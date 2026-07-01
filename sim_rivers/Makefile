# Focused River Simulation Makefile
CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -g

TARGET = sim_rivers
SRCS = main.cpp simulator.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
