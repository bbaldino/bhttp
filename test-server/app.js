var https = require('https');
var fs = require('fs');
var express = require('express');
var bodyParser = require('body-parser');
var app = express();

var options = {
  key: fs.readFileSync('./localhost.key'),
  cert: fs.readFileSync('./localhost.crt'),
  requestCert: false,
  rejectUnauthorized: false
};

app.use(bodyParser.text());

var getNum = 0;
app.get('/', function(req, res) {
  console.log("Got a GET request");
  var sleepTime = Math.floor((Math.random() * 1000) + 1);
  setTimeout(function(res) {
    res.send("get response " + getNum++);
  }.bind(null, res), sleepTime);
});

var postNum = 0;
app.post('/', function(req, res) {
  console.log("Got a POST request");
  var sleepTime = Math.floor((Math.random() * 1000) + 1);
  setTimeout(function(res) {
    res.send("post response " + postNum++);
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

var longPollNum = 0;
app.get('/longpoll', function(req, res) {
  console.log("Got a longpoll request");
  var sleepTime = 5000;
  setTimeout(function(res, num) {
    res.send("longpoll response " + longPollNum++);
  }.bind(null, res), sleepTime);
});

app.post('/echo', function(req, res) {
  res.send(req.body);
});


//var server = app.listen(8070, function() {
var server = https.createServer(options, app).listen(8070, function() {
  var host = server.address().address;
  var port = server.address().port;

  console.log("Server listening at http://%s:%s", host, port)
});
