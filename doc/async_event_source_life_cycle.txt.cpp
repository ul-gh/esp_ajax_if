/* ESPAsyncWebServer
 * AsyncEventSource set-up and operation life cycle in more-or-less
 * chronological order.
 */


///// Types
class AsyncEventSource;
class AsyncEventSourceResponse;
class AsyncEventSourceClient;
typedef std::function<void(AsyncEventSourceClient *client)> ArEventHandlerFunction;


class AsyncEventSource: public AsyncWebHandler {
  private:
    String _url;
    LinkedList<AsyncEventSourceClient *> _clients;
    ArEventHandlerFunction _connectcb;
  public:
    AsyncEventSource(const String& url);
    ~AsyncEventSource();


///// The SSE source live cycle starts with a specialised instance of
///// an AsyncWebHandler
AsyncEventSource::AsyncEventSource(const String& url)
  : _url(url)
  , _clients(LinkedList<AsyncEventSourceClient *>([](AsyncEventSourceClient *c){ delete c; }))
  , _connectcb(NULL)
{}

// This means the handler methods are overridden as follows
bool AsyncEventSource::canHandle(AsyncWebServerRequest *request){
  if(request->method() != HTTP_GET || !request->url().equals(_url)) {
    return false;
  }
  request->addInterestingHeader("Last-Event-ID");
  return true;
}

// A browser request for registering the SSE source is handled here.
// This is called from the request instance in AsyncWebServerRequest::_onData()
void AsyncEventSource::handleRequest(AsyncWebServerRequest *request){
  if((_username != "" && _password != "") && !request->authenticate(_username.c_str(), _password.c_str()))
    return request->requestAuthentication();
  // The AsyncEventSourceResponse is a specialised AsyncWebServerResponse.
  request->send(new AsyncEventSourceResponse(this));
}

void AsyncWebServerRequest::send(AsyncWebServerResponse *response){
    _client->setRxTimeout(0);
    _response->_respond(this);
}

// This sends the authentication/registration response and waits for
// an ACK back from the browser.
void AsyncEventSourceResponse::_respond(AsyncWebServerRequest *request){
  String out = _assembleHead(request->version());
  request->client()->write(out.c_str(), _headLength);
  _state = RESPONSE_WAIT_ACK;
}


// When the browser sends back an ACK package, this goes through a
// chain of handlers after which finally the client object is instantiated.
AsyncWebServerRequest::AsyncWebServerRequest(AsyncWebServer* s, AsyncClient* c)
{
  // [...]
  c->onAck(
    [debug](void *r, AsyncClient* c, size_t len, uint32_t time){
        (void)c;
        // [...]
        AsyncWebServerRequest *req = (AsyncWebServerRequest*)r;
        req->_onAck(len, time);
    }
  // [...]
}

void AsyncWebServerRequest::_onAck(size_t len, uint32_t time){
  if(_response != NULL){
    if(!_response->_finished()){
      _response->_ack(this, len, time);
    }
    if(_response->_finished()){
      AsyncWebServerResponse* r = _response;
      _response = NULL;
      delete r;
      _client->close();
    }
  }
}

// AsyncEventSourceClient allocated here
size_t AsyncEventSourceResponse::_ack(AsyncWebServerRequest *request, size_t len, uint32_t time __attribute__((unused))){
  if(len){
    new AsyncEventSourceClient(request, _server);
  }
  return 0;
}

// See type:
class AsyncEventSourceClient {
  private:
    AsyncClient *_client;
    AsyncEventSource *_server;
    uint32_t _lastId;
    LinkedList<AsyncEventSourceMessage *> _messageQueue;
    void _queueMessage(AsyncEventSourceMessage *dataMessage);
    void _runQueue();
    AsyncWebLockMQ _lockmq; // ArFi 2020/08/27 for protecting/serializing _messageQueue
  public:
    AsyncEventSourceClient(AsyncWebServerRequest *request, AsyncEventSource *server);
    ~AsyncEventSourceClient();

// The backend AsyncClient _client is taken from the original request object
// which the request handler received before.
// Importantly, the AsyncClient instance runs on the async_tcp task
// and has the essential TCP callbacks registered for async operation:
AsyncEventSourceClient::AsyncEventSourceClient(AsyncWebServerRequest *request, AsyncEventSource *server)
: _messageQueue(LinkedList<AsyncEventSourceMessage *>([](AsyncEventSourceMessage *m){ delete  m; }))
{
  _client = request->client();
  _server = server;
  _lastId = 0;
  if(request->hasHeader("Last-Event-ID"))
    _lastId = atoi(request->getHeader("Last-Event-ID")->value().c_str());
    
  _client->setRxTimeout(0);
  _client->onError(NULL, NULL);
  //_client->onAck([](void *r, AsyncClient* c, size_t len, uint32_t time){ (void)c; ((AsyncEventSourceClient*)(r))->_onAck(len, time); }, this);
  //FIXME: Debug
  _client->onAck(
    [](void *r, AsyncClient* c, size_t len, uint32_t time){
        (void)c;
        AsyncEventSourceClient* aesc = (AsyncEventSourceClient*)r;
        aesc->_onAck(len, time);
        },
    this);
  _client->onPoll([](void *r, AsyncClient* c){ (void)c; ((AsyncEventSourceClient*)(r))->_onPoll(); }, this);
  _client->onData(NULL, NULL);
  _client->onTimeout([this](void *r, AsyncClient* c __attribute__((unused)), uint32_t time){ ((AsyncEventSourceClient*)(r))->_onTimeout(time); }, this);
  _client->onDisconnect([this](void *r, AsyncClient* c){ ((AsyncEventSourceClient*)(r))->_onDisconnect(); delete c; }, this);

  _server->_addClient(this); // This seems to do nothing
  delete request; // ?! Is this a good idea? Are there no callbacks registered?
}

// Event emission is activated once an ACK from the remote web browser client is
// received.
void AsyncEventSourceClient::_onAck(size_t len, uint32_t time){
  _lockmq.lock(); // protect _messageQueue
  while(len && !_messageQueue.isEmpty()){
    len = _messageQueue.front()->ack(len, time);
    if(_messageQueue.front()->finished())
      _messageQueue.remove(_messageQueue.front());
  }
  _lockmq.unlock(); // protect _messageQueue

  _runQueue();
}

// Buggy, but anyways...
// (*i) is AsyncEventSourceMessage* item from LinkedList _messageQueue.
void AsyncEventSourceClient::_runQueue(){
  while(!_messageQueue.isEmpty() && _messageQueue.front()->finished()){
    _messageQueue.remove(_messageQueue.front());
 
  for(auto i = _messageQueue.begin(); i != _messageQueue.end(); ++i)
  {
    if(!(*i)->sent())
      (*i)->send(_client);
  }
}

size_t AsyncEventSourceMessage::send(AsyncClient *client) {
  // [...]
  // client->add() is where data is already begun to be sent via TCP WRITE.
  size_t sent = client->add((const char *)_data, len);
  if(client->canSend())
    client->send();
  _sent += sent;
  return sent; 
}

size_t AsyncClient::add(const char* data, size_t size, uint8_t apiflags) {
    // [...]
    err = _tcp_write(_pcb, _closed_slot, data, will_send, apiflags);
    if(err != ERR_OK) {
        return 0;
    }
    return will_send;
}

// A quick look through messages on this subject indicates that tcp_output()
// lets lwip know that you're done making tcp_write() calls for now, and that
// it can transmit the accumulated packet immediately, rather than waiting for
// any additional writes. In other words, its use is solely a matter of
// efficiency, rather than correctness of operation.
// jasonharper Aug 26 '18 at 17:45 on stackoverflow

bool AsyncClient::send(){
    int8_t err = ERR_OK;
    err = _tcp_output(_pcb, _closed_slot);
    if(err == ERR_OK){
        _pcb_busy = true;
        _pcb_sent_at = millis();
        return true;
    }
    return false;
}