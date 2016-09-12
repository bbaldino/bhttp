#include <beast/core/to_string.hpp>
#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <string>

using namespace std;


//typedef std::function<void(CURLcode curl_retcode, int http_code, _S_<Buffer> response,
//  const boost::container::map<std::string,std::string>& response_headers)> Callback;

class BHttp {
public:
  BHttp(
      const std::string&& hostname,
      const std::string&& port) :
    hostname_(std::move(hostname)),
    port_(std::move(port)),
    socket_(ios_),
    resolver_(ios_),
    thread_([this]() { cout << "running" << endl; ios_.run(); }),
    thread2_([this]() { cout << "running2" << endl; ios_.run(); }) {
  }

  void connect_async() {
    boost::asio::ip::tcp::resolver::query q{hostname_, port_};
    resolver_.async_resolve(q, boost::bind(&BHttp::onResolved, this, _1, _2));
  }

  void connect() {
    boost::asio::ip::tcp::resolver::query q{hostname_, port_};
    boost::asio::ip::tcp::resolver::iterator iter = resolver_.resolve(q);
    cout << "resolved to " << iter->endpoint() << endl;
    socket_.connect(*iter);
    //TODO: error handling?
    //TODO: ssl handshake
    //TODO: if we force sync here, we can't give any sort of timeout
    // which sucks.  could move to the async callback version (couldn't get
    // async future version working)
    onConnected({});
  }

  uint32_t Post(
      const std::string& endpoint_url,
      const std::map<std::string, std::string>& request_headers,
      const std::set<std::string>& request_response_headers) {

    return 0;
  }



//protected:
  const std::string hostname_;
  const std::string port_;
  boost::asio::io_service ios_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::thread thread_;
  boost::thread thread2_;


  void onConnected(const boost::system::error_code& ec) {
    cout << "onConnected" << endl;
    auto resp = doGet("/");

    cout << to_string(resp.body.data()) << endl;
    cout << "----" << endl;
    cout << resp.status << endl;
    cout << beast::http::reason_string(resp.status) << endl;
    cout << "----" << endl;
    cout << resp.headers.exists("Content-length") << endl;
    for (auto h = resp.headers.begin(); h != resp.headers.end(); ++h) {
      cout << h->name() << ", " << h->value() << endl;
    }
  }

  void onResolved(
      const boost::system::error_code& ec, 
      boost::asio::ip::tcp::resolver::iterator iter) {
    printf("resolved\n");
    if (!ec) {
      std::cout << "hostname resolved to " << iter->endpoint() << endl;
      //socket_.async_connect(*iter, boost::bind(&BHttp::onConnected, this, _1));
      auto connect_future = socket_.async_connect(*iter, boost::asio::use_future);
      std::cout << "waiting on connect future" << endl;
      connect_future.get();
      std::cout << "connected" << endl;
      onConnected(boost::system::error_code());
    } else {
      std::cout << "error: " << ec.message() << endl;
    }
  }

  beast::http::response_v1<beast::http::streambuf_body>
  doGet(const std::string& url) {
    beast::http::request_v1<beast::http::empty_body> req;
    req.method = "GET";
    req.version = 11;
    req.url = url;
    req.headers.replace("Host", hostname_ + ":" + to_string(socket_.remote_endpoint().port()));
    beast::http::prepare(req);
    beast::http::write(socket_, req);

    beast::streambuf sb;
    beast::http::response_v1<beast::http::streambuf_body> resp;
    beast::http::read(socket_, sb, resp);

    return resp;
  }

  void doGetTest(const std::string& url) {
    beast::http::request_v1<beast::http::empty_body> req;
    req.method = "GET";
    req.version = 11;
    req.url = url;
    req.headers.replace("Host", hostname_ + ":" + to_string(socket_.remote_endpoint().port()));
    beast::http::prepare(req);
    beast::http::write(socket_, req);

      beast::http::response_v1<beast::http::streambuf_body> resp;
    {
      beast::streambuf sb;
      beast::http::read(socket_, sb, resp);
    }
    cout << "printing response:" << endl;
    //cout << resp;
    cout << to_string(resp.body.data()) << endl;
    cout << "printed response" << endl;
    cout << "----" << endl;
    cout << resp.status << endl;
    cout << beast::http::reason_string(resp.status) << endl;
    cout << "----" << endl;
    cout << resp.headers.exists("Content-length") << endl;
    for (auto h = resp.headers.begin(); h != resp.headers.end(); ++h) {
      cout << h->name() << ", " << h->value() << endl;
    }

  }
};

int main(int argc, char* argv[]) {
  //BHttp http("brian.cloud-dev.fatline.io", "8080");
  BHttp http("127.0.0.1", "8070");
  http.connect();
  //http.connect_sync();
  usleep(10000000);




#if 0
  const string host = "brian.cloud-dev.fatline.io";
  const uint16_t port = 8080;
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver{io_service};
  boost::asio::ip::tcp::socket sock{io_service};
  boost::asio::ip::tcp::resolver::query q{host, port};
  resolver.async_resolve(q, resolve_handler);
  //boost::asio::connect(sock, resolver.resolve(boost::asio::ip::tcp::resolver::query{host, "http"}));
  boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("127.0.0.1"), 5555);
  //boost::asio::connect(sock, ep);
  sock.connect(ep);

  beast::http::request_v1<beast::http::empty_body> req;
  req.method = "GET";
  req.url = "/";
  req.version = 11;
  req.headers.replace("Host", host + ":" + to_string(sock.remote_endpoint().port()));
  req.headers.replace("User-Agent", "Beast");
  beast::http::prepare(req);
  beast::http::write(sock, req);

  beast::streambuf sb;
  beast::http::response_v1<beast::http::streambuf_body> resp;
  beast::http::read(sock, sb, resp);
  cout << resp;


#endif
  return 0;
}
