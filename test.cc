#include <iostream>

#include <boost/thread.hpp>

#include "bhttp.hpp"

void doGets(BHttp& http) {
  for (auto i = 0; i < 20; ++i) {
    http.doGet("/").get();
  }
}

void doPosts(BHttp& http) {
  for (auto i = 0; i < 20; ++i) {
    http.doPost("/", "echo").get();
  }
}

int main(int argc, char* argv[]) {
  BHttp http("127.0.0.1", "8070");
  http.connect();

  boost::thread thread1([&http]() { doGets(http); });
  boost::thread thread2([&http]() { doPosts(http); });

  thread1.join();
  thread2.join();
}
