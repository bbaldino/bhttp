#pragma once

#include <beast/core/to_string.hpp>
#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <future>
#include <string>

#include "scheduler.hpp"
#include "scheduler_context.hpp"
#include "async_semaphore.hpp"

typedef beast::http::response_v1<beast::http::streambuf_body> Response;
template<typename T>
using Request = beast::http::request_v1<T>;
typedef boost::system::error_code error_code_t;
typedef boost::asio::ip::tcp::socket socket_t;

// Step 3: library that can do post and get requests, give responses.
//  can support multiple sockets.  single thread still handles all requests
//  and will not block if a socket is unavailable at the time of the request.
//  coroutines make the logic here a lot simpler

//TODO: wondering about the use case here though.  the issue here is that all
// calls must be made from within a coroutine spawned by the same scheduler_context
// that's being used here.  so, first of all, this class doesn't need the scheduler
// at all, it just needs the io_service from it.  once we have that, then the use
// case becomes a bit better maybe?  caller just needs to make sure all calls into
// this are through that scheduler_context's thread...so either from a coro spawned
// by it or done via a post (which scheduler already gives futures for).  OR...it
// does get a scheduler, and the public apis post everything onto it.  how would
// we make that work with coroutines?  this class would have to have its own coro
// run loop?
// could have support for coros and futures.  if coro, lib would use the io_service
// given, otherwise it would spin up its own io service and thread?  if given an io
// service, it could do either coros or futures. if it spins up its own, obviously
// only future-type handlers would make sense
class BHttp {
public:
  BHttp(
      Scheduler& master_scheduler,
      const std::string&& hostname,
      const std::string&& port) :
      scheduler_(master_scheduler),
      hostname_(std::move(hostname)),
      port_(std::move(port)),
      resolver_(scheduler_.master_scheduler_.io_service_),
      socket_sem_(scheduler_.master_scheduler_.io_service_) {
    for (auto i = 0; i < 100; ++i) {
      sockets_.push_back(std::move(std::make_shared<socket_t>(scheduler_.master_scheduler_.io_service_)));
    }
  }

  ~BHttp() {
    std::cout << "dtor" << std::endl;
  }

  void connect() {
    boost::asio::ip::tcp::resolver::query q{boost::asio::ip::tcp::v4(), hostname_, port_};
    boost::asio::ip::tcp::resolver::iterator iter = resolver_.resolve(q);
    std::cout << "resolved to " << iter->endpoint() << std::endl;
    for (auto& s : sockets_) {
      s->connect(*iter);
    }
  }

  Response doGet(const std::string& url, boost::asio::yield_context context) {
    Request<beast::http::empty_body> req;
    req.method = "GET";
    req.version = 11;
    req.url = url;
    req.headers.replace("Host", hostname_ + ":" + port_);
    beast::http::prepare(req);

    auto socket = getAvailableSocket(context);
    return handleReqResp(socket, req, context);
  }

  Response doPost(const std::string& url, boost::asio::yield_context context) {
    Request<beast::http::empty_body> req;
    req.method = "POST";
    req.version = 11;
    req.url = url;
    req.headers.replace("Host", hostname_ + ":" + port_);
    beast::http::prepare(req);

    auto socket = getAvailableSocket(context);
    return handleReqResp(socket, req, context);
  }

//protected:
  const std::string hostname_;
  const std::string port_;
  SchedulerContext scheduler_;
  boost::asio::ip::tcp::resolver resolver_;
  // Accessed only from thread_
  std::deque<std::shared_ptr<socket_t>> sockets_;
  AsyncSemaphore socket_sem_;

  void assertInIoServiceThread() {
    assert(boost::this_thread::get_id() == scheduler_.master_scheduler_.thread_.get_id());
  }

  std::shared_ptr<socket_t> getAvailableSocket(boost::asio::yield_context context) {
    assertInIoServiceThread();
    if (sockets_.empty()) {
      socket_sem_.Wait(context);
    }
    assert(!sockets_.empty());
    auto socket = sockets_.back();
    sockets_.pop_back();
    return socket;
  }

  void returnSocket(const std::shared_ptr<socket_t>& socket) {
    assertInIoServiceThread();
    sockets_.push_back(socket);
    socket_sem_.Raise();
  }

  template<typename T>
  Response handleReqResp(std::shared_ptr<socket_t> socket, Request<T> req, boost::asio::yield_context context) {
    assertInIoServiceThread();
    error_code_t ec;
    beast::http::async_write(*socket, req, context[ec]);
    if (ec) {
      std::cout << "Error writing request";
      returnSocket(socket);
      return {};
    }
    beast::streambuf sb;
    Response resp;
    beast::http::async_read(*socket, sb, resp, context[ec]);
    if (ec) {
      std::cout << "Error reading response";
      returnSocket(socket);
      return {};
    }
    returnSocket(socket);
    return resp;
  }
};
