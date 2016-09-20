#pragma once

#include <beast/core/to_string.hpp>
#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <future>
#include <string>

typedef beast::http::response_v1<beast::http::streambuf_body> Response;
template<typename T>
using Request = beast::http::request_v1<T>;
typedef boost::system::error_code error_code_t;
typedef boost::asio::ip::tcp::socket socket_t;

// Step 1: library that can do post and get requests, give responses.
//  safe with regards to multiple requests by naively locking a single
//  socket instance.
class BHttp {
public:
  BHttp(
      const std::string&& hostname,
      const std::string&& port) :
    hostname_(std::move(hostname)),
    port_(std::move(port)),
    work_(ios_),
    socket_(ios_),
    resolver_(ios_),
    thread_([this]() { ios_.run(); }) {
  }

  ~BHttp() {
    work_ = boost::none;
    thread_.join();
  }

  void connect() {
    boost::asio::ip::tcp::resolver::query q{boost::asio::ip::tcp::v4(), hostname_, port_};
    boost::asio::ip::tcp::resolver::iterator iter = resolver_.resolve(q);
    std::cout << "resolved to " << iter->endpoint() << std::endl;
    socket_.connect(*iter);
  }

  std::future<Response> doGet(const std::string& url) {
    beast::http::request_v1<beast::http::empty_body> req;
    req.method = "GET";
    req.version = 11;
    req.url = url;
    req.headers.replace("Host", hostname_ + ":" + std::to_string(socket_.remote_endpoint().port()));
    beast::http::prepare(req);

    return handleReqResp(move(req));
  }

  std::future<Response> doPost(const std::string& url, const std::string& body) {
    beast::http::request_v1<beast::http::string_body> req;
    req.method = "POST";
    req.version = 11;
    req.url = url;
    req.headers.replace("Host", hostname_ + ":" + std::to_string(socket_.remote_endpoint().port()));
    req.headers.replace("Content-Type", "text/plain");
    req.body = body;
    beast::http::prepare(req);

    return handleReqResp(move(req));
  }
    
//protected:
  const std::string hostname_;
  const std::string port_;
  boost::asio::io_service ios_;
  boost::optional<boost::asio::io_service::work> work_;
  socket_t socket_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::thread thread_;
  boost::mutex socket_mutex_;

  template<typename T>
  std::future<Response> handleReqResp(Request<T> req) {
    socket_mutex_.lock();
    auto response_promise = std::make_shared<std::promise<Response>>();
    beast::http::async_write(
      socket_,
      req,
      [response_promise, this](const error_code_t& ec) mutable {
        if (ec) { 
          response_promise->set_value({}); 
          socket_mutex_.unlock();
        }
        auto sb = std::make_shared<beast::streambuf>();
        auto resp = std::make_shared<Response>();
        beast::http::async_read(
          socket_,
          *sb,
          *resp,
          [response_promise, resp, sb, this](const error_code_t& ec) mutable {
            if (ec)  {
              response_promise->set_value({});
            }
            response_promise->set_value(*resp);
            socket_mutex_.unlock();
          }
        );
      }
    );

    return response_promise->get_future();
  }
};
