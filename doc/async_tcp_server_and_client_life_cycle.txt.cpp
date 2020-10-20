/* AsyncTCP API
 * AsyncServer and AsyncClient connection life cycle in more-or-less
 * chronological order. AsyncClient and AsyncServer functions/methods
 * run in two tasks:
 * LWIP task ("tiT") is part of ESP-IDF.
 * TCP/IP event task ("async_tcp") is defined and launched here.
 */

//Types:
using lwip_event_packet_t = struct {
    // arg member used as reinterpret_cast<AsyncClient*>(arg)
    // or reinterpret_cast<AsyncServer*>(arg)
    void *arg;
    union struct {
        void* pcb;
        pbuf * pb;
        int8_t err;
        //…
    }
}

//Globals:
xQueueHandle _async_queue;

//AsyncServer members:
    _addr;
    _port;
    _pcb{0};
    _connect_cb{0};
    _connect_cb_arg{0};


// Called from app_main()
// Starts async task concurrently with LWIP API setup, for async task see further below
void AsyncServer::begin(){
    _start_async_task();
    _pcb = tcp_new_ip_type();
    _tcp_bind(_pcb, &local_addr, _port);
    _pcb = _tcp_listen_with_backlog(_pcb, backlog = 5); // Performs LWIP API call
    // LWIP API call stores the single (there can be only one) AsyncServer instance into the initial TCP pcb
    tcp_arg(_pcb, (void*) this);
    // LWIP API call registers static callback _s_accept which then calls the AsyncServer->_accept() method
    tcp_accept(_pcb, &_s_accept);
}


// TCP/IP API call implementation.
///// TCP BIND
static esp_err_t _tcp_bind(tcp_pcb * pcb, ip_addr_t * addr, uint16_t port) {
    if(!pcb){
        return ESP_FAIL;
    }
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.closed_slot = -1;
    msg.bind.addr = addr;
    msg.bind.port = port;
    tcpip_api_call(_tcp_bind_api, (struct tcpip_api_call_data*)&msg);
    return msg.err;
}
static err_t _tcp_bind_api(struct tcpip_api_call_data *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = tcp_bind(msg->pcb, msg->bind.addr, msg->bind.port);
    return msg->err;
}

///// TCP LISTEN
_tcp_listen_with_backlog(tcp_pcb *pcb, uint8_t backlog){
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.backlog = 5;
    tcpip_api_call(_tcp_listen_api, (struct tcpip_api_call_data*)msg); // TCP/IP API call to LWIP
}
// TCP/IP API call implementation, registered into LWIP as shown above.
// This runs on the LWIP thread!
_tcp_listen_api((struct tcpip_api_call_data*)&msg){
    // call to LWIP, see file espidf/components/lwip/src/include/lwip/tcp.h
    tcp_listen_with_backlog(msg->pcb, msg->backlog);
}

AsyncServer::_s_accept(void *pcb, int8_t err) {
    reinterpret_cast<AsyncServer*>(arg)->_accept(pcb, err);
}
AsyncServer::_accept(tcp_pcb *pcb, int8_t err) {
    if (_connect_cb) {
        // AsyncClient is constructed HERE!
        AsyncClient *c = new AsyncClient(pcb); // ==> AsyncClient lives on heap memory!
        return _tcp_accept(this, c);
    }

}

///// TCP ACCEPT
// Global again:
// void *arg is the AsyncServer instance
static int8_t _tcp_accept(void *arg, AsyncClient *client) {
    //…
    lwip_event_packet_t e = malloc(…)
    e->arg = arg; // AsyncServer instance
    e->accept.client = client;
    _prepend_async_event(&e);
}

static inline bool _prepend_async_event(lwip_event_packet_t **e){
    xQueueSendToFront(_async_queue, e, portMAX_DELAY);
}


// TCP/IP Event Task "async_tcp" does event handling
static bool _start_async_task(){
    if(!_init_async_event_queue()){
        return false;
    }
    if(!_async_service_task_handle){
        xTaskCreateUniversal(_async_service_task, "async_tcp", 8192 * 2, NULL, 3, &_async_service_task_handle, CONFIG_ASYNC_TCP_RUNNING_CORE);
        if(!_async_service_task_handle){
            return false;
        }
    }
    return true;
}

_async_service_task(){
    _get_async_event(&packet); // xQueueReceive(_async_queue, packet)
    _handle_async_event(packet);
}

static inline bool _get_async_event(lwip_event_packet_t ** e){
    return _async_queue && xQueueReceive(_async_queue, e, portMAX_DELAY) == pdPASS;
}
_handle_async_event(lwip_event_packet_t *e){
    //if…else if… else if... else if….
        // Calls (one AFAIK) server method:
        AsyncServer::_s_accepted();
        // Calls multiple different client methods:
        AsyncClient::_s_connected();
        AsyncClient::_s_recv();
}

// AsyncServer acts when tcp "accept" package is received:
int8_t AsyncServer::_s_accepted(void *arg, AsyncClient* client){
    return reinterpret_cast<AsyncServer*>(arg)->_accepted(client);
}
int8_t AsyncServer::_accepted(AsyncClient* client){
    if(_connect_cb){
        _connect_cb(_connect_cb_arg, client);
    }
    return ERR_OK;
}

// This connected in WebServer.cpp, constructor AsyncWebServer::AsyncWebServer()
void AsyncServer::onClient(AcConnectHandler cb, void* arg){
    _connect_cb = cb;
    _connect_cb_arg = arg;
}


// AsyncClient contructor connects further LWIP API callbacks
AsyncClient::AsyncClient(tcp_pcb* pcb)
    _pcb = pcb;
    if(_pcb){
        tcp_arg(_pcb, this);
        tcp_recv(_pcb, &_tcp_recv);
        tcp_sent(_pcb, &_tcp_sent);
        tcp_err(_pcb, &_tcp_error);
        tcp_poll(_pcb, &_tcp_poll, 1);
    }
////// END connection establishing life cycle

////// TCP RECV life cycle
static int8_t _tcp_recv(void * arg, struct tcp_pcb * pcb, struct pbuf *pb, int8_t err) {
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->arg = arg;
    if(pb){
        //ets_printf("+R: 0x%08x\n", pcb);
        e->event = LWIP_TCP_RECV;
        e->recv.pcb = pcb;
        e->recv.pb = pb;
        e->recv.err = err;
    } else {
        //ets_printf("+F: 0x%08x\n", pcb);
        e->event = LWIP_TCP_FIN;
        e->fin.pcb = pcb;
        e->fin.err = err;
        //close the PCB in LwIP thread
        AsyncClient::_s_lwip_fin(e->arg, e->fin.pcb, e->fin.err);
    }
    if (!_send_async_event(&e)) {
        free((void*)(e));
    }
    return ERR_OK;
}

static inline bool _send_async_event(lwip_event_packet_t ** e){
    return _async_queue && xQueueSend(_async_queue, e, portMAX_DELAY) == pdPASS;
}

// Called then from event handler:
int8_t AsyncClient::_s_recv(void * arg, struct tcp_pcb * pcb, struct pbuf *pb, int8_t err) {
    return reinterpret_cast<AsyncClient*>(arg)->_recv(pcb, pb, err);
}

int8_t AsyncClient::_recv(tcp_pcb* pcb, pbuf* pb, int8_t err) {
    while(pb != NULL) {
        _rx_last_packet = millis();
        //we should not ack before we assimilate the data
        _ack_pcb = true;
        pbuf *b = pb;
        pb = b->next;
        b->next = NULL;
        if(_pb_cb){
            _pb_cb(_pb_cb_arg, this, b);
        } else {
            if(_recv_cb) {
                // User data callback function AsyncClient::onData, see below
                _recv_cb(_recv_cb_arg, this, b->payload, b->len);
            }
            if(!_ack_pcb) {
                _rx_ack_len += b->len;
            } else if(_pcb) {
                _tcp_recved(_pcb, _closed_slot, b->len);
            }
            pbuf_free(b);
        }
    }
    return ERR_OK;
}

// Again LWIP API call:
static esp_err_t _tcp_recved(tcp_pcb * pcb, int8_t closed_slot, size_t len) {
    if(!pcb){
        return ERR_CONN;
    }
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.closed_slot = closed_slot;
    msg.received = len;
    tcpip_api_call(_tcp_recved_api, (struct tcpip_api_call_data*)&msg);
    return msg.err;
}
static err_t _tcp_recved_api(struct tcpip_api_call_data *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = ERR_CONN;
    if(msg->closed_slot == -1 || !_closed_slots[msg->closed_slot]) {
        msg->err = 0;
        tcp_recved(msg->pcb, msg->received);
    }
    return msg->err;
}

// User data callback function AsyncClient::onData registered from application
void AsyncClient::onData(AcDataHandler cb, void* arg){
    _recv_cb = cb;
    _recv_cb_arg = arg;
}
////// END receive life cycle


////// TCP WRITE life cycle
size_t AsyncClient::write(const char* data) {
    if(data == NULL) {
        return 0;
    }
    return write(data, strlen(data));
}
size_t AsyncClient::write(const char* data, size_t size, uint8_t apiflags) {
    size_t will_send = add(data, size, apiflags);
    if(!will_send || !send()) {
        return 0;
    }
    return will_send;
}
size_t AsyncClient::add(const char* data, size_t size, uint8_t apiflags) {
    if(!_pcb || size == 0 || data == NULL) {
        return 0;
    }
    size_t room = space();
    if(!room) {
        return 0;
    }
    size_t will_send = (room < size) ? room : size;
    int8_t err = ERR_OK;
    err = _tcp_write(_pcb, _closed_slot, data, will_send, apiflags);
    if(err != ERR_OK) {
        return 0;
    }
    return will_send;
}

static esp_err_t _tcp_write(tcp_pcb * pcb, int8_t closed_slot, const char* data, size_t size, uint8_t apiflags) {
    if(!pcb){
        return ERR_CONN;
    }
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.closed_slot = closed_slot;
    msg.write.data = data;
    msg.write.size = size;
    msg.write.apiflags = apiflags;
    tcpip_api_call(_tcp_write_api, (struct tcpip_api_call_data*)&msg);
    return msg.err;
}
static err_t _tcp_write_api(struct tcpip_api_call_data *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = ERR_CONN;
    if(msg->closed_slot == -1 || !_closed_slots[msg->closed_slot]) {
        msg->err = tcp_write(msg->pcb, msg->write.data, msg->write.size, msg->write.apiflags);
    }
    return msg->err;
}
////// End WRITE life cycle. This is followed by TCP ACK...


////// TCP ACK / TCP SENT
// LWIP TASK ("tiT")
// From LWIP API, _tcp_sent() callback is called.
// This was registered before into the LWIP API in the constructor of AsyncClient.
static int8_t _tcp_sent(void * arg, struct tcp_pcb * pcb, uint16_t len) {
    //ets_printf("+S: 0x%08x\n", pcb);
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_SENT;
    e->arg = arg;
    e->sent.pcb = pcb;
    e->sent.len = len;
    if (!_send_async_event(&e)) {
        free((void*)(e));
    }
    return ERR_OK;
}

// "async_tcp" TASK
// The event is then taken up by TCP/IP event task "async_tcp" (_async_service_task())
  } else if(e->event == LWIP_TCP_SENT){
      AsyncClient::_s_sent(e->arg, e->sent.pcb, e->sent.len);

int8_t AsyncClient::_s_sent(void * arg, struct tcp_pcb * pcb, uint16_t len) {
    return reinterpret_cast<AsyncClient*>(arg)->_sent(pcb, len);
}
int8_t AsyncClient::_sent(tcp_pcb* pcb, uint16_t len) {
    _rx_last_packet = millis();
    //log_i("%u", len);
    _pcb_busy = false;
    if(_sent_cb) {
        _sent_cb(_sent_cb_arg, this, len, (millis() - _pcb_sent_at));
    }
    return ERR_OK;
}

// _sent_cb was registered before from user side using:
void AsyncClient::onAck(AcAckHandler cb, void* arg){
    _sent_cb = cb;
    _sent_cb_arg = arg;
}
////// End TCP ACK / TCP SENT life cycle.


///// Close lifecycle:
void AsyncClient::close(bool now){
    if(_pcb){
        _tcp_recved(_pcb, _closed_slot, _rx_ack_len);
    }
    _close();
}
int8_t AsyncClient::_close(){
    //ets_printf("X: 0x%08x\n", (uint32_t)this);
    int8_t err = ERR_OK;
    if(_pcb) {
        //log_i("");
        tcp_arg(_pcb, NULL);
        tcp_sent(_pcb, NULL);
        tcp_recv(_pcb, NULL);
        tcp_err(_pcb, NULL);
        tcp_poll(_pcb, NULL, 0);
        _tcp_clear_events(this);
        err = _tcp_close(_pcb, _closed_slot);
        if(err != ERR_OK) {
            err = abort();
        }
        _pcb = NULL;
        if(_discard_cb) {
            _discard_cb(_discard_cb_arg, this);
        }
    }
    return err;
}
static int8_t _tcp_clear_events(void * arg) {
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_CLEAR;
    e->arg = arg;
    if (!_prepend_async_event(&e)) {
        free((void*)(e));
    }
    return ERR_OK;
}

///// Event loop then calls _remove_events_with_arg()
//    } else if(e->event == LWIP_TCP_CLEAR){
//        _remove_events_with_arg(e->arg);
static bool _remove_events_with_arg(void * arg){
    lwip_event_packet_t * first_packet = NULL;
    lwip_event_packet_t * packet = NULL;

    if(!_async_queue){
        return false;
    }
    //figure out which is the first packet so we can keep the order
    while(!first_packet){
        if(xQueueReceive(_async_queue, &first_packet, 0) != pdPASS){
            return false;
        }
        //discard packet if matching
        if((int)first_packet->arg == (int)arg){
            free(first_packet);
            first_packet = NULL;
        //return first packet to the back of the queue
        } else if(xQueueSend(_async_queue, &first_packet, portMAX_DELAY) != pdPASS){
            return false;
        }
    }

    while(xQueuePeek(_async_queue, &packet, 0) == pdPASS && packet != first_packet){
        if(xQueueReceive(_async_queue, &packet, 0) != pdPASS){
            return false;
        }
        if((int)packet->arg == (int)arg){
            frele(packet);
            packet = NULL;
        } else if(xQueueSend(_async_queue, &packet, portMAX_DELAY) != pdPASS){
            return false;
        }
    }
    return true;
}


///// TCP CLOSE API implementation and call
static esp_err_t _tcp_close(tcp_pcb * pcb, int8_t closed_slot) {
    if(!pcb){
        return ERR_CONN;
    }
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.closed_slot = closed_slot;
    tcpip_api_call(_tcp_close_api, (struct tcpip_api_call_data*)&msg);
    return msg.err;
}
static err_t _tcp_close_api(struct tcpip_api_call_data *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = ERR_CONN;
    if(msg->closed_slot == -1 || !_closed_slots[msg->closed_slot]) {
        msg->err = tcp_close(msg->pcb);
    }
    return msg->err;
}