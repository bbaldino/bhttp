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
typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_t;

// Step 5: library that can do post and get requests, give responses.
//  can support multiple sockets.  single thread still handles all requests
//  and will not block if a socket is unavailable at the time of the request.
//  coroutines make the logic here a lot simpler.  the api here is also changed
//  so the caller is not required to be using a coroutine and doGet and doPost will
//  return a future.  now add the ability to specify whether or not to use ssl for
//  a connection.
class BHttp {
public:
  BHttp(
      Scheduler& master_scheduler,
      const std::string& hostname,
      const std::string& port,
      boost::asio::ssl::context& context,
      bool sslEnabled) :
      scheduler_(master_scheduler),
      hostname_(hostname),
      port_(port),
      resolver_(scheduler_.GetIoService()),
      sslEnabled_(sslEnabled) {
    socket_sem_ = scheduler_.CreateSemaphore();
    for (auto i = 0; i < 1; ++i) {
      sockets_.push_back(std::make_shared<socket_t>(scheduler_.GetIoService(), context));
    }
  }

  ~BHttp() {
    std::cout << "dtor" << std::endl;
    scheduler_.Stop();
    std::cout << "scheduler stopped" << std::endl;
  }

  static boost::asio::ssl::context createSslContext(const std::string& pemFilePath) {
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    if (!pemFilePath.empty()) {
      ctx.load_verify_file(pemFilePath);
    }
    ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    return ctx;
  }

  static boost::asio::ssl::context createDummySslContext() {
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_verify_mode(boost::asio::ssl::verify_none);
    return ctx;
  }

  // Synchronous.  Must be called before any calls to make requests.
  bool connect() {
    boost::asio::ip::tcp::resolver::query q{boost::asio::ip::tcp::v4(), hostname_, port_};
    error_code_t ec;
    boost::asio::ip::tcp::resolver::iterator iter = resolver_.resolve(q, ec);
    if (ec) {
      std::cout << "Error resolving host: " << ec.message();
      return false;
    }
    std::cout << "resolved to " << iter->endpoint() << std::endl;
    for (auto& s : sockets_) {
      if (sslEnabled_) {
        std::cout << "Enabling https" << std::endl;
        s->set_verify_callback([](bool preverified, boost::asio::ssl::verify_context& ctx) {
          std::cout << "verified" << std::endl;
          return true;
        });
      }
      s->lowest_layer().connect(*iter, ec);
      if (ec) {
        std::cout << "failed to connect" << std::endl;
        return false;
      }
      if (sslEnabled_) {
        s->handshake(boost::asio::ssl::stream_base::client, ec);
        if (ec) {
          std::cout << "failed doing ssl handleshake" << std::endl;
          return false;
        }
      }
    }
    return true;
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
  bool sslEnabled_;
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
    if (sslEnabled_) {
      beast::http::async_write(*socket, req, context[ec]);
    } else {
      beast::http::async_write(socket->next_layer(), req, context[ec]);
    }
    if (ec) {
      std::cout << "Error writing request";
    } else {
      beast::streambuf sb;
      if (sslEnabled_) {
        beast::http::async_read(*socket, sb, resp, context[ec]);
      } else {
        beast::http::async_read(socket->next_layer(), sb, resp, context[ec]);
      }
      if (ec) {
        std::cout << "Error reading response";
      }
    }
    returnSocket(socket);
    return resp;
  }
};
