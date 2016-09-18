#include <iostream>

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "scheduler.hpp"
#include "bhttp.hpp"

/*
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
*/

boost::mutex m;
boost::condition_variable cond;
Scheduler master_scheduler;
BHttp http(master_scheduler, "127.0.0.1", "8070");

void spawnFunc(boost::asio::yield_context context) {
  http.connect();

  for (auto i = 0; i < 10; ++i) {
    auto resp = http.doGet("/", context);
    std::cout << resp << std::endl;
  }
  boost::mutex::scoped_lock lock(m);
  cond.notify_one();
}

int main(int argc, char* argv[]) {
  boost::asio::spawn(master_scheduler.io_service_, &spawnFunc);
  std::cout << "Waiting for finish" << std::endl;
  boost::mutex::scoped_lock lock(m);
  cond.wait(lock);
  std::cout << "Finished" << std::endl;

  //boost::thread thread1([&http]() { doGets(http); });
  //boost::thread thread2([&http]() { doPosts(http); });

  //thread1.join();
  //thread2.join();
}
