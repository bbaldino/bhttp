#pragma once

#include <beast/core/to_string.hpp>
#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <future>
#include <string>

#include "scheduler.hpp"
#include "scheduler_context.hpp"

typedef beast::http::response_v1<beast::http::streambuf_body> Response;
template<typename T>
using Request = beast::http::request_v1<T>;
typedef boost::system::error_code error_code_t;
#define HTTPS
#ifdef HTTPS
typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_t;
#else
typedef boost::asio::ip::tcp::socket socket_t;
#endif

// Step 4: library that can do post and get requests, give responses.
//  can support multiple sockets.  single thread still handles all requests
//  and will not block if a socket is unavailable at the time of the request.
//  coroutines make the logic here a lot simpler.  the api here is also changed
//  so the caller is not required to be using a coroutine and doGet and doPost will
//  return a future
class BHttp {
public:
  BHttp(
      Scheduler& master_scheduler,
      const std::string& hostname,
      const std::string& port) :
      scheduler_(master_scheduler),
      hostname_(hostname),
      port_(port),
      resolver_(scheduler_.GetIoService()) {
    socket_sem_ = scheduler_.CreateSemaphore();
#ifdef HTTPS
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    //ctx.load_verify_file("/Users/brian/Downloads/boost_1_61_0/libs/asio/example/cpp03/ssl/ca.pem");
    for (auto i = 0; i < 1; ++i) {
      sockets_.push_back(std::move(std::make_shared<socket_t>(scheduler_.GetIoService(), ctx)));
    }
#else
    for (auto i = 0; i < 1; ++i) {
      sockets_.push_back(std::move(std::make_shared<socket_t>(scheduler_.GetIoService())));
    }
#endif
  }

  ~BHttp() {
    std::cout << "dtor" << std::endl;
    scheduler_.Stop();
    std::cout << "scheduler stopped" << std::endl;
  }

  static boost::asio::ssl::context createSslContext(const std::string& pemFilePath) {
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.load_verify_file(pemFilePath);
    return ctx;
  }

  // Synchronous.  Must be called before any calls to make requests.
  void connect() {
    boost::asio::ip::tcp::resolver::query q{boost::asio::ip::tcp::v4(), hostname_, port_};
    boost::asio::ip::tcp::resolver::iterator iter = resolver_.resolve(q);
    std::cout << "resolved to " << iter->endpoint() << std::endl;
    for (auto& s : sockets_) {
#ifdef HTTPS
      s->set_verify_mode(boost::asio::ssl::verify_peer);
      s->set_verify_callback([](bool preverified, boost::asio::ssl::verify_context& ctx) {
        std::cout << "verified" << std::endl;
        return true;
      });
      s->lowest_layer().connect(*iter);
      s->handshake(boost::asio::ssl::stream_base::client);
#else
      s->connect(*iter);
#endif
    }
  }

  std::future<Response> doGet(const std::string& url) {
    Request<beast::http::empty_body> req;
    req.method = "GET";
    req.version = 11;
    req.url = url;
    req.headers.replace("Host", hostname_ + ":" + port_);
    beast::http::prepare(req);

    auto resp_promise = std::make_shared<std::promise<Response>>();
    scheduler_.SpawnCoroutine([req = move(req), resp_promise, this](boost::asio::yield_context context) {
      auto socket = getAvailableSocket(context);
      resp_promise->set_value(handleReqResp(socket, req, context));
    });
    return resp_promise->get_future();
  }

  std::future<Response> doPost(const std::string& url) {
    Request<beast::http::empty_body> req;
    req.method = "POST";
    req.version = 11;
    req.url = url;
    req.headers.replace("Host", hostname_ + ":" + port_);
    beast::http::prepare(req);

    auto resp_promise = std::make_shared<std::promise<Response>>();
    scheduler_.SpawnCoroutine([req = move(req), resp_promise, this](boost::asio::yield_context context) {
      auto socket = getAvailableSocket(context);
      resp_promise->set_value(handleReqResp(socket, req, context));
    });
    return resp_promise->get_future();
  }

//protected:
  const std::string hostname_;
  const std::string port_;
  SchedulerContext scheduler_;
  boost::asio::ip::tcp::resolver resolver_;
  // Accessed only from inside the io service context
  std::deque<std::shared_ptr<socket_t>> sockets_;
  std::shared_ptr<AsyncSemaphore> socket_sem_;

  std::shared_ptr<socket_t> getAvailableSocket(boost::asio::yield_context context) {
    scheduler_.VerifyInIoServiceThread();
    if (sockets_.empty()) {
      socket_sem_->Wait(context);
    }
    assert(!sockets_.empty());
    auto socket = sockets_.back();
    sockets_.pop_back();
    return socket;
  }

  void returnSocket(const std::shared_ptr<socket_t>& socket) {
    scheduler_.VerifyInIoServiceThread();
    sockets_.push_back(socket);
    socket_sem_->Raise();
  }

  template<typename T>
  Response handleReqResp(std::shared_ptr<socket_t> socket, Request<T> req, boost::asio::yield_context context) {
    scheduler_.VerifyInIoServiceThread();
    error_code_t ec;
    Response resp;
    beast::http::async_write(*socket, req, context[ec]);
    if (ec) {
      std::cout << "Error writing request";
    } else {
      beast::streambuf sb;
      beast::http::async_read(*socket, sb, resp, context[ec]);
      if (ec) {
        std::cout << "Error reading response";
      }
    }
    returnSocket(socket);
    return resp;
  }
};
