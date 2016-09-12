var express = require('express');
var bodyParser = require('body-parser');
var app = express();

app.use(bodyParser.text());

var getNum = 0;
app.get('/', function(req, res) {
  console.log("Got a GET request");
  var sleepTime = Math.floor((Math.random() * 1000) + 1);
  setTimeout(function(res) {
    res.send("" + getNum++);
  }.bind(null, res), sleepTime);
});

var postNum = 0;
app.post('/', function(req, res) {
  console.log("Got a POST request");
  var sleepTime = Math.floor((Math.random() * 1000) + 1);
  setTimeout(function(res) {
    res.send("" + postNum++);
  }.bind(null, res), sleepTime);
});

/*
var num = 0;
var first = true;
app.post('/', function(req, res) {
  console.log("Got a POST request");
  console.log("request: ", req.body);
  var currNum = num++;
  var sleepTime = Math.floor((Math.random() * 1000) + 1);
  if (first) {
    sleepTime = 5000;
    first = false;
  } else {
    sleepTime = 0;
  }
  console.log("num " + currNum + " sleeping for " + sleepTime + " milliseconds");
  setTimeout(function(res, num) {
    console.log("sending response " + num);
    res.send("" + num);
  }.bind(null, res, currNum), sleepTime);
  //res.send("echo");
});
*/

var firstRes;
app.post('/slow', function(req, res) {
  firstRes = res;
  //var sleepTime = 5000;
  //setTimeout(function(res, num) {
  //  res.send("slow");
  //}.bind(null, res), sleepTime);
});

app.post('/fast', function(req, res) {
  res.send("fast");
  //firstRes.send("slow");
});

app.post('/echo', function(req, res) {
  res.send(req.body);
});


var server = app.listen(8070, function() {
  var host = server.address().address;
  var port = server.address().port;

  console.log("Server listening at http://%s:%s", host, port)
});
