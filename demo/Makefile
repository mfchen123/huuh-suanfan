# 国标麻将算番 Demo Makefile
LIB_DIR = ../mahjong-algorithm

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra -Wno-unused-parameter

LIB_SRCS = $(LIB_DIR)/fan_calculator.cpp $(LIB_DIR)/shanten.cpp $(LIB_DIR)/stringify.cpp

demo: demo.cpp $(LIB_SRCS)
	$(CXX) $(CXXFLAGS) -I$(LIB_DIR) demo.cpp $(LIB_SRCS) -o demo

run: demo
	./demo "1112345678999s9s" win=self initial

clean:
	rm -f demo

.PHONY: run clean
