
By Blake Felt - blake.w.felt@gmail.com

ESP32 WebSocket
==================

A component for WebSockets on ESP-IDF using lwip netconn.
For an example, see https://github.com/Molorius/ESP32-Examples.

To add to a project, type:
`git submodule add https://github.com/Molorius/esp32-websocket.git components/websocket`
into the base directory of your project.

Some configuration options for the Server can be found in menuconfig in:
Component config ---> WebSocket Server

This presently only has the WebSocket server code working, but client code will be added in the future (the groundwork is there).

The code only allows one WebSocket server at a time, but this merely handles all incoming reads. New connections are added externally, so this can be used to hold various WebSocket connections.

While this can theoretically handle very large messages, hardware constraints (RAM) limits the size of messages. I highly recommend not using more than 5000 bytes per message, but no constraint is in place for this. 

Any suggestions or fixes are gladly appreciated.

Table of Contents
=================
* [Enumerations](#enumerations)
  * [WEBSOCKET_TYPE_t](#enum-websocket_type_t)
* [Functions](#functions)
  * [ws_server_start](#int-ws_server_start)
  * [ws_server_stop](#int-ws_server_stop)
  * [ws_server_add_client](#int-ws_server_add_clientstruct-netconn-connchar-msguint16_t-lenchar-urlvoid-callback)
  * [ws_server_add_client_protocol](#int-ws_server_add_client_protocolstruct-netconn-connchar-msguint16_t-lenchar-urlchar-protocolvoid-callback)
  * [ws_server_len_url](#int-ws_server_len_urlchar-url)
  * [ws_server_len_all](#int-ws_server_len_all)
  * [ws_server_remove_client](#int-ws_server_remove_clientint-num)
  * [ws_server_remove_clients](#int-ws_server_remove_clientschar-url)
  * [ws_server_remove_all](#int-ws_server_remove_all)
  * [ws_server_send_text_client](#int-ws_server_send_text_clientint-numchar-msguint64_t-len)
  * [ws_server_send_text_clients](#int-ws_server_send_text_clientschar-urlchar-msguint64_t-len)
  * [ws_server_send_text_all](#int-ws_server_send_text_allchar-msguint64_t-len)
  * [ws_server_send_text_client_from_callback](#int-ws_server_send_text_client_from_callbackint-numchar-msguint64_t-len)
  * [ws_server_send_text_clients_from_callback](#int-ws_server_send_text_clients_from_callbackchar-urlchar-msguint64_t-len)
  * [ws_server_send_text_all_from_callback](#int-ws_server_send_text_all_from_callbackchar-msguint64_t-len)

Enumerations
============

enum WEBSOCKET_TYPE_t
---------------------

The different types of WebSocket events.

*Values*
  * `WEBSOCKET_CONNECT`: A new client has successfully connected.
  * `WEBSOCKET_DISCONNECT_EXTERNAL`: The other side sent a disconnect message.
  * `WEBSOCKET_DISCONNECT_INTERNAL`: The esp32 server sent a disconnect message.
  * `WEBSOCKET_DISCONNECT_ERROR`: Disconnect due to a connection error.
  * `WEBSOCKET_TEXT`: Incoming text.
  * `WEBSOCKET_BIN`: Incoming binary.
  * `WEBSOCKET_PING`: The other side sent a ping message.
  * `WEBSOCKET_PONG`: The other side successfully replied to our ping.

Functions
=========

int ws_server_start()
---------------------

Starts the WebSocket Server. Use this function before attempting any
sort of transmission or adding a client.

*Returns*
  * 1: successful start
  * 0: server already running

int ws_server_stop()
--------------------

Stops the WebSocket Server. New clients can still be added and
messages can be sent, but new messages will not be received.

*Returns*
  * 1: successful stop
  * 0: server was not running before

int ws_server_add_client(struct netconn* conn,char* msg,uint16_t len,char* url,void *callback)
----------------------------------------------------------------------------------------------

Adds a client to the WebSocket Server handler and performs the necessary handshake.

*Parameters*
  * `conn`: the lwip netconn connection.
  * `msg`: the entire incoming request message to join the server. Necessary for the handshake.
  * `len`: the length of `msg`.
  * `url`: the NULL-terminated url. Used to keep track of clients, not required.
  * `callback`: the callback that is used to run WebSocket events. This must be with parameters(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) where "num" is the client number, "type" is the event type, "msg" is the incoming message, and "len" is the message length. The callback itself is optional.

*Returns*
  * -2: not enough information in `msg` to perform handshake.
  * -1: server full, or connection issue.
  * 0 or greater: connection number

int ws_server_add_client_protocol(struct netconn* conn,char* msg,uint16_t len,char* url,char* protocol,void *callback)
----------------------------------------------------------------------------------------------------------------------

Adds a client to the WebSocket Server handler and performs the necessary handshake. Will also send
the specified protocol.

*Parameters*
  * `conn`: the lwip netconn connection.
  * `msg`: the entire incoming request message to join the server. Necessary for the handshake.
  * `len`: the length of `msg`.
  * `url`: the NULL-terminated url. Used to keep track of clients, not required.
  * `protocol`: the NULL-terminated protocol. This will be sent to the client in the header.
  * `callback`: the callback that is used to run WebSocket events. This must be with parameters(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) where "num" is the client number, "type" is the event type, "msg" is the incoming message, and "len" is the message length. The callback itself is optional.

*Returns*
  * -2: not enough information in `msg` to perform handshake.
  * -1: server full, or connection issue.
  * 0 or greater: connection number

int ws_server_len_url(char* url)
--------------------------------

Returns the number of clients connected to the specified URL.

*Parameters*
  * `url`: the NULL-terminated string of the desired URL.

*Returns*
  * The number of clients connected to the specified URL.

int ws_server_len_all()
-----------------------

*Returns*
  * The number of connected clients.

int ws_server_remove_client(int num)
------------------------------------

Removes the desired client.

*Parameters*
  * `num`: the client number

*Returns*
  * 0: not a valid client number
  * 1: client disconnected

int ws_server_remove_clients(char* url)
---------------------------------------

Removes all clients connect to the desired URL.

*Parameters*
  * `url`: the NULL-terminated URL.

*Returns*
  * The number of clients that were disconnected.

int ws_server_remove_all()
--------------------------

Removes all clients from server.

*Returns*
  * The number of clients that were disconnected.

int ws_server_send_text_client(int num,char* msg,uint64_t len)
--------------------------------------------------------------

Sends the desired message to the client.

*Parameters*
  * `num`: the client's number.
  * `msg`: the desired message.
  * `len`: the length of the message.

*Returns*
  * 0: message not sent properly
  * 1: message sent

int ws_server_send_text_clients(char* url,char* msg,uint64_t len)
-----------------------------------------------------------------

Sends the message to clients connected to the desired URL.

*Parameters*
  * `url`: the NULL-terminated URL.
  * `msg`: the desired message.
  * `len`: the length of the message.

*Returns*
  * The number of clients that the message was sent to.

int ws_server_send_text_all(char* msg,uint64_t len)
---------------------------------------------------

Sends the message to all connected clients.

*Parameters*
  * `msg`: the desired message
  * `len`: the length of the message

*Returns*
  * The number of clients that the message was sent to.

int ws_server_send_text_client_from_callback(int num,char* msg,uint64_t len)
----------------------------------------------------------------------------

Sends the desired message to the client. Only use this inside the callback function.

*Parameters*
  * `num`: the client's number.
  * `msg`: the desired message.
  * `len`: the length of the message.

*Returns*
  * 0: message not sent properly
  * 1: message sent

int ws_server_send_text_clients_from_callback(char* url,char* msg,uint64_t len)
-------------------------------------------------------------------------------

Sends the message to clients connected to the desired URL. Only use this inside the callback function.

*Parameters*
  * `url`: the NULL-terminated URL.
  * `msg`: the desired message.
  * `len`: the length of the message.

*Returns*
  * The number of clients that the message was sent to.

int ws_server_send_text_all_from_callback(char* msg,uint64_t len)
-----------------------------------------------------------------

Sends the message to all connected clients. Only use this inside the callback function.

*Parameters*
  * `msg`: the desired message
  * `len`: the length of the message

*Returns*
  * The number of clients that the message was sent to.
