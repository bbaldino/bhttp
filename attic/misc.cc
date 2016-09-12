
  /*
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
  */

  // Executes the given request and returns a future for its response
  template<typename T>
  future<Response> handleReqResOld(Request<T>&& req) {
    socket_t* socket = getAvailableSocket();
    auto resp_promise = make_shared<promise<Response>>();
    beast::http::async_write(
      socket,
      req,
      [resp_promise, socket](const error_code_t& ec) mutable {
        if (!ec) {
          cout << "request successful" << endl;
          auto sb = make_shared<beast::streambuf>();
          auto resp = make_shared<Response>();
          beast::http::async_read(
            socket,
            sb,
            *resp,
            [resp_promise, resp, sb, socket](const error_code_t& ec) mutable {
              if (!ec) {
                cout << "got response" << endl;
                //TODO: move the resp? will shared_ptr be mad if object is moved out of it?
                resp_promise->set_value(*resp);
              } else {
                cout << "handling response failed: " << ec.message() << endl;
                resp_promise->set_value({});
              }
            });
        } else {
          cout << "request failed" << endl;
          resp_promise->set_value({});
        }
      });
    return resp_promise->get_future();
  }
