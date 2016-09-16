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

// Step 2: library that can do post and get requests, give responses.
//  can support multiple sockets.  single thread still handles all requests
//  and will not block if a socket is unavailable at the time of the request.
class BHttp {
public:
  BHttp(
      const std::string&& hostname,
      const std::string&& port) :
      hostname_(std::move(hostname)),
      port_(std::move(port)),
      work_(ios_),
      resolver_(ios_),
      socket_timer_(ios_),
      context_(0),
      thread_([this]() { ios_.run(); }) {
    for (auto i = 0; i < 5; ++i) {
      sockets_.push_back(std::move(std::make_shared<socket_t>(ios_)));
    }
  }

  ~BHttp() {
    std::cout << "dtor" << std::endl;
    work_ = boost::none;
    thread_.join();
  }

  void connect() {
    boost::asio::ip::tcp::resolver::query q{boost::asio::ip::tcp::v4(), hostname_, port_};
    boost::asio::ip::tcp::resolver::iterator iter = resolver_.resolve(q);
    std::cout << "resolved to " << iter->endpoint() << std::endl;
    for (auto& s : sockets_) {
      s->connect(*iter);
    }
  }

  std::future<Response> doGet(const std::string& url) {
    auto req = std::make_shared<beast::http::request_v1<beast::http::empty_body>>();
    req->method = "GET";
    req->version = 11;
    req->url = url;
    req->headers.replace("Host", hostname_ + ":" + port_);
    beast::http::prepare(*req);

    auto response_promise = std::make_shared<std::promise<Response>>();
    getAvailableSocket([req, response_promise, this](const std::shared_ptr<socket_t>& socket) {
      handleReqResp(socket, *req, response_promise);
    });
    return response_promise->get_future();
  }

  std::future<Response> doPost(const std::string& url, const std::string& body) {
    auto req = std::make_shared<beast::http::request_v1<beast::http::empty_body>>();
    req->method = "POST";
    req->version = 11;
    req->url = url;
    req->headers.replace("Host", hostname_ + ":" + port_);
    beast::http::prepare(*req);

    auto response_promise = std::make_shared<std::promise<Response>>();
    getAvailableSocket([req, response_promise, this](const std::shared_ptr<socket_t>& socket) {
      handleReqResp(socket, *req, response_promise);
    });
    return response_promise->get_future();
  }
    
//protected:
  const std::string hostname_;
  const std::string port_;
  boost::asio::io_service ios_;
  boost::optional<boost::asio::io_service::work> work_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::thread thread_;
  // Accessed only from thread_
  std::deque<std::shared_ptr<socket_t>> sockets_;
  boost::asio::deadline_timer socket_timer_;
  int context_;

  // Because there may not be a socket available and we can't block waiting for one,
  //  this takes a handler to be invoked once a socket is available.
  template<typename OnSocketHandler>
  void getAvailableSocket(const OnSocketHandler& onSocket) {
    ios_.post([onSocket, this]() {
      auto myContext = context_++;
      std::cout << boost::this_thread::get_id() << " " << myContext << ": Getting socket" << std::endl;
      assert(boost::this_thread::get_id() == thread_.get_id());
      if (!sockets_.empty()) {
        std::cout << boost::this_thread::get_id() << " " << myContext << ": Socket already available" << std::endl;
        auto socket = sockets_.back();
        sockets_.pop_back();
        onSocket(socket);
      } else {
        std::cout << boost::this_thread::get_id() << " " << myContext << ": Waiting for available socket" << std::endl;
        socket_timer_.async_wait([onSocket, this, myContext](const error_code_t& ec) {
          std::cout << boost::this_thread::get_id() << " " << myContext << ": Socket now available" << std::endl;
          if (ec != boost::system::errc::operation_canceled) {
            std::cout << "Error waiting for socket: " << ec.message();
          } else {
            assert(!sockets_.empty());
            auto socket = sockets_.back();
            sockets_.pop_back();
            onSocket(socket);
          }
        });
      }
    });
  }

  // Must be called from the io service thread
  void returnSocket(const std::shared_ptr<socket_t>& socket) {
    assert(boost::this_thread::get_id() == thread_.get_id());
    std::cout << boost::this_thread::get_id() << ": returning socket to pool" << std::endl;
    sockets_.push_back(socket);
    socket_timer_.cancel_one();
  }

  template<typename T>
  void handleReqResp(
      std::shared_ptr<socket_t> socket,
      Request<T> req,
      std::shared_ptr<std::promise<Response>> response_promise) {
    std::cout << "writing req" << std::endl;
    beast::http::async_write(
      *socket, 
      req,
      [response_promise, socket, this](const error_code_t& ec) {
        std::cout << "wrote req" << std::endl;
        if (ec) {
          std::cout << "Error making request: " << ec.message() << std::endl;
          response_promise->set_value({});
          returnSocket(socket);
          return;
        }
        auto sb = std::make_shared<beast::streambuf>();
        auto resp = std::make_shared<Response>();
        std::cout << "reading response" << std::endl;
        beast::http::async_read(
          *socket,
          *sb,
          *resp,
          [response_promise, resp, sb, socket, this](const error_code_t& ec) {
            std::cout << "got resp" << std::endl;
            if (ec) {
              response_promise->set_value({});
              returnSocket(socket);
              return;
            }
            std::cout << "setting response in future" << std::endl;
            response_promise->set_value(*resp);
            returnSocket(socket);
          }
        );
      }
    );
  }
};
