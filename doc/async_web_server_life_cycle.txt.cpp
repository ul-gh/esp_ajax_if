/* ESPAsyncWebServer API key features,
 * AsyncWebServer set-up and operation life cycle in call order.
 */


///// Types
class AsyncWebServer;
class AsyncWebServerRequest;
class AsyncWebServerResponse;
class AsyncWebHeader;
class AsyncWebParameter;
class AsyncWebRewrite;
class AsyncWebHandler;
class AsyncStaticWebHandler;
class AsyncCallbackWebHandler;
class AsyncResponseStream;

// AsyncWebServer structure details
class AsyncWebServer {
  protected:
    AsyncServer _server;
    LinkedList<AsyncWebRewrite*> _rewrites;
    LinkedList<AsyncWebHandler*> _handlers;
    AsyncCallbackWebHandler* _catchAllHandler;

  public:
    AsyncWebServer(uint16_t port);
    ~AsyncWebServer();

    void begin();
    void end();
    // [....]
}

// AsyncWebServerRequest structure details
class AsyncWebServerRequest {
  using File = fs::File;
  using FS = fs::FS;
  friend class AsyncWebServer;
  friend class AsyncCallbackWebHandler;
  // FIXME: Debug
  public:
    AsyncClient* _client;
    AsyncWebServer* _server;
    AsyncWebHandler* _handler;
    AsyncWebServerResponse* _response;
    StringArray _interestingHeaders;
    ArDisconnectHandler _onDisconnectfn;


///// AsyncWebServer life cycle begins and ends with the server object,
///// this must be a single instance.

// The constructor most importantly registers the backend AsyncServer::onClient
// callback which instanciates a new AsyncWebServerRequest object for
// each incoming client connection. The request objects live on the heap
// and responses are later created and also run from the async_tcp task.
AsyncWebServer::AsyncWebServer(uint16_t port)
  : _server(port)
  , _rewrites(LinkedList<AsyncWebRewrite*>([](AsyncWebRewrite* r){ delete r; }))
  , _handlers(LinkedList<AsyncWebHandler*>([](AsyncWebHandler* h){ delete h; }))
{
  _catchAllHandler = new AsyncCallbackWebHandler();
  if(_catchAllHandler == NULL)
    return;
  // This sets the AsyncClient::_connect_cb which is called from "async_tcp" task
  _server.onClient([](void *s, AsyncClient* c){
    if(c == NULL)
      return;
    c->setRxTimeout(3);
    AsyncWebServerRequest *r = new AsyncWebServerRequest((AsyncWebServer*)s, c);
    if(r == NULL){
      c->close(true);
      c->free();
      delete c;
    }
  }, this); // _connect_cb_arg is this AsyncWebServer instance
}


//// From the application/server implementation side, the individual
//// request handlers are registered into the server.
//// Only one of these is then later attached to any request by the request
//// itself calling AsyncWebServer::_attachHandler().
AsyncCallbackWebHandler& AsyncWebServer::on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest, ArUploadHandlerFunction onUpload, ArBodyHandlerFunction onBody){
  AsyncCallbackWebHandler* handler = new AsyncCallbackWebHandler();
  handler->setUri(uri);
  handler->setMethod(method);
  handler->onRequest(onRequest);
  handler->onUpload(onUpload);
  handler->onBody(onBody);
  addHandler(handler);
  return *handler;
}

AsyncWebHandler& AsyncWebServer::addHandler(AsyncWebHandler* handler){
  _handlers.add(handler);
  return *handler;
}


///// AsyncWebServer starts TCP backend AsyncServer
///// after the essential callbacks have been set up
void AsyncWebServer::begin(){
  _server.setNoDelay(true);
  _server.begin(); // Starts all of the AsyncTCP internals, event loop etc.
}

///// By this time, the AsyncTCP backend is running and has the following
///// callbacks registered.
///// These are called from and run in the async_tcp task:

///// AsyncWebServerRequest life cycle, 
AsyncWebServerRequest::AsyncWebServerRequest(AsyncWebServer* s, AsyncClient* c)
  : _client(c)
  , _server(s)
  , _handler(NULL)
  , _response(NULL)
  //, [...]
{
    // Request constructor sets up essential callbacks on the client object
    // for connection state change handling.
    // The clients are wrapped in the request object which then goes through
    // the rewrites if present and later attach themselves a matching request
    // handler function by calling the AsyncWebServer::_attachHandler() method.
    c->onError([](void *r, AsyncClient* c, int8_t error){ ... });
    c->onAck(
        [](void *r, AsyncClient* c, size_t len, uint32_t time){
            req->_onAck(len, time);
        },
        this);
    c->onDisconnect(
        [](void *r, AsyncClient* c){
            AsyncWebServerRequest *req = (AsyncWebServerRequest*)r;
            c->close(true);
            c->free();
            delete c;
            req->_onDisconnect();
            },
        this);
    c->onTimeout(...
    c->onData(...
    c->onPoll(...
}

void AsyncWebServerRequest::_onData(void *buf, size_t len){
  while (true) {
    if(_parseState < PARSE_REQ_BODY){
      _parseLine();
      (...)
    }
    // [...]
    if(_parsedLength == _contentLength){
      _parseState = PARSE_REQ_END;
      //check if authenticated before calling handleRequest and request auth instead
      if(_handler) _handler->handleRequest(this);
      else send(501);
    }
}

void AsyncWebServerRequest::_parseLine(){
  if(_parseState == PARSE_REQ_START){
    if(!_temp.length()){
      _parseState = PARSE_REQ_FAIL;
      _client->close();
    } else {
      _parseReqHead();
      _parseState = PARSE_REQ_HEADERS;
    }
    return;
  }
  if(_parseState == PARSE_REQ_HEADERS){
    if(!_temp.length()){
      //end of headers
      _server->_rewriteRequest(this);
      _server->_attachHandler(this);
      _removeNotInterestingHeaders();
      //check handler for authentication
      if(_contentLength){
        _parseState = PARSE_REQ_BODY;
      } else {
        _parseState = PARSE_REQ_END;
        if(_handler) _handler->handleRequest(this);
        else send(501);
      }
    } else _parseReqHeader();
  }
}

// This is called from void AsyncWebServerRequest::_parseLine() in WebRequest.cpp
// when a request is received
void AsyncWebServer::_attachHandler(AsyncWebServerRequest *request){
  for(const auto& h: _handlers){
    if (h->filter(request) && h->canHandle(request)){
      request->setHandler(h);
      return;
    }
  }
  request->addInterestingHeader("ANY");
  request->setHandler(_catchAllHandler);
}

// Also called from void AsyncWebServerRequest::_parseLine(), or, alternatively,
// called from void AsyncWebServerRequest::_onData(void *buf, size_t len).
// The response is sent here.
// This method is overridden in the specific derivatives of AsyncWebHandler, e.g.:
void AsyncStaticWebHandler::handleRequest(AsyncWebServerRequest *request) {
    request->send(response);
    request->send(404);
}

void AsyncWebServerRequest::send(AsyncWebServerResponse *response){
  _response = response;
  if(_response == NULL){
    _client->close(true);
    _onDisconnect();
    return;
  }
  if(!_response->_sourceValid()){
    delete response;
    _response = NULL;
    send(500);
  }
  else {
    _client->setRxTimeout(0);
    _response->_respond(this);
  }
}

// See WebResponseImpl.h:
// The different response types are overridden individually.
// See WebResponses.cpp:
// In all cases, a first segment of data is sent back to the TCP client
// after which this method returns and the server waits for a TCP ACK package
// to arrive again from the client.
void AsyncBasicResponse::_respond(AsyncWebServerRequest *request){
  _state = RESPONSE_HEADERS;
  String out = _assembleHead(request->version());
  size_t outLen = out.length();
  size_t space = request->client()->space();
  if(!_contentLength && space >= outLen){
    _writtenLength += request->client()->write(out.c_str(), outLen);
  }
  // ...
}

// The TCP ACK then either triggers a reply of more data chunks
// or the ACK sets the _state = RESPONSE_END
size_t AsyncBasicResponse::_ack(AsyncWebServerRequest *request, size_t len, uint32_t time){
  (void)time;
  _ackedLength += len;
  if(_state == RESPONSE_CONTENT){
    size_t available = _contentLength - _sentLength;
    size_t space = request->client()->space();
    //we can fit in this packet
    if(space > available){
      _writtenLength += request->client()->write(_content.c_str(), available);
      _content = String();
      _state = RESPONSE_WAIT_ACK;
      return available;
    }
    //send some data, the rest on ack
    String out = _content.substring(0, space);
    _content = _content.substring(space);
    _sentLength += space;
    _writtenLength += request->client()->write(out.c_str(), space);
    return space;
  } else if(_state == RESPONSE_WAIT_ACK){
    if(_ackedLength >= _writtenLength){
      _state = RESPONSE_END;
    }
  }
  return 0;
}


void AsyncWebServerRequest::_onDisconnect(){
  //os_printf("d\n");
  if(_onDisconnectfn) {
      _onDisconnectfn();
    }
  // Debug
  if ((int)_server == 0xfefefefe) {
      ets_printf("Dang!");
      abort();
  }
  _server->_handleDisconnect(this);
}




