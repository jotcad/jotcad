# Standalone Simulation Makefile
CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -g

TARGET = sim_app
SRCS = main.cpp sim_core.cpp sim_init.cpp sim_ecology.cpp sim_infrastructure.cpp sim_settlements.cpp sim_exporters.cpp sim_visuals_surface.cpp sim_visuals_projection.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
