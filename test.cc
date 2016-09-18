#include <iostream>

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "scheduler.hpp"
#include "bhttp.hpp"

using namespace std;

void doLongPoll(BHttp& http) {
  for (auto i = 0; i < 4; ++i) {
    cout << http.doGet("/longpoll").get() << endl;
  }
}

void doGets(BHttp& http) {
  for (auto i = 0; i < 20; ++i) {
    cout << http.doGet("/").get() << endl;
  }
}

void doPosts(BHttp& http) {
  for (auto i = 0; i < 20; ++i) {
    cout << http.doPost("/").get() << endl;
  }
}

boost::mutex m;
boost::condition_variable cond;
Scheduler master_scheduler;

int main(int argc, char* argv[]) {
  BHttp http(master_scheduler, "127.0.0.1", "8070");
  http.connect();
  std::cout << http.doGet("/").get() << std::endl;
  std::cout << http.doGet("/").get() << std::endl;

  //boost::thread thread1([&http]() { doLongPoll(http); });
  //boost::thread thread2([&http]() { doPosts(http); });

  //thread1.join();
  //thread3.join();
}
