CC=g++
CFLAGS=-std=c++1y -g

BOOST_INCLUDE_DIR=/usr/local/include/
BOOST_LIB_BASE_PATH=/usr/local/lib
#LIB_DIRS=-L $(BOOST_LIB_BASE_PATH)/context/build/darwin-4.2.1/debug/link-static/ -L $(BOOST_LIB_BASE_PATH)/coroutine/build/darwin-4.2.1/debug/link-static/ -L $(BOOST_LIB_BASE_PATH)/system/build/darwin-4.2.1/debug/link-static/ -L $(BOOST_LIB_BASE_PATH)/thread/build/darwin-4.2.1/debug/link-static/threading-multi -L $(BOOST_LIB_BASE_PATH)/chrono/build/darwin-4.2.1/debug/link-static/
LIBS=-lboost_system -lboost_thread -lboost_context -lboost_coroutine -lboost_chrono

INCLUDE_DIRS=include/Beast/include 
#TODO: add scheduler as a submodule
SCHEDULER_INCLUDE_DIR=/Users/brian/work/scheduler

#GTEST_ROOT=./googletest/googletest
#GMOCK_ROOT=./googletest/googlemock

#all: async_sleep_test async_semaphore_test async_future_test scheduler_test scheduler_context_test

test: test.cc bhttp.hpp
	$(CC) $(CFLAGS) -I $(INCLUDE_DIRS) -I $(BOOST_INCLUDE_DIR) -I $(SCHEDULER_INCLUDE_DIR) -L /usr/local/lib $(LIBS) test.cc -o test

test2: test2.cc
	$(CC) $(CFLAGS) -I $(INCLUDE_DIRS) -I $(BOOST_INCLUDE_DIR) -L /usr/local/lib $(LIBS) test2.cc -o test2

test3: test3.cc
	$(CC) $(CFLAGS) -I $(INCLUDE_DIRS) -I $(SCHEDULER_INCLUDE_DIR) -I $(BOOST_INCLUDE_DIR) -L /usr/local/lib $(LIBS) test3.cc -o test3
#async_sleep_test: test/async_sleep_test.cc
#	$(CC) $(CFLAGS) -I $(GTEST_ROOT)/include -I . -I $(BOOST_INCLUDE_DIR) $(LIB_DIRS) -L $(GMOCK_ROOT)/gtest -lgtest -lgtest_main $(LIBS) test/async_sleep_test.cc -o test/async_sleep_test

#check: 
#	test/async_sleep_test && test/async_semaphore_test && test/async_future_test && test/scheduler_test && test/scheduler_context_test
