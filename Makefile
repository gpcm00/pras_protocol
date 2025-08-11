CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -Os
LDFLAGS  ?=
LDLIBS   ?= -lssl -lcrypto -lstdc++fs

ifeq ($(DEBUG),1)
    CXXFLAGS := -std=c++17 -g -O0 -DDEBUG
endif

SRCS = main.cc protocol.cc crypto.cc cp_buffer.cc
OBJS = $(SRCS:.cc=.o)

TARGET = procsr

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
