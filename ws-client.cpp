#include "ws-client.hpp"

#include <giomm-2.4/giomm.h>

void WebSocketClient::RegisterSignals(const NotifyConnectedSlot &connect_slot,
                                         const NotifyMessageSlot &message_slot,
                                         const NotifyDataSlot &data_slot,
                                         const NotifyClosedSlot &closed_slot,
                                         const NotifyPingPongSlot &pingpong_slot)
{
    connect_signal.clear();
    connect_signal.connect(connect_slot);

    message_signal.clear();
    message_signal.connect(message_slot);

    data_signal.clear();
    data_signal.connect(data_slot);

    closed_signal.clear();
    closed_signal.connect(closed_slot);

    pingpong_signal.clear();
    pingpong_signal.connect(pingpong_slot);
}

bool WebSocketClient::SendMessage(const std::string &message)
{
    printf("SendMessage ->\n");

    if (message.empty()) {
        printf("SendMessage <- Empty message\n");
        return true;
    }

    if (!websocket_connection) {
        printf("SendMessage <- WebSocket connection is NULL\n");
        return false;
    }

    printf("message: %s\n", message.c_str());

    soup_websocket_connection_send_text(websocket_connection, message.c_str());

    printf("SendMessage <-\n");
    return true;
}

bool WebSocketClient::SendData(const unsigned char *data, unsigned int data_size)
{
    printf("SendData -> Size: %d\n", data_size);

    if (data == nullptr || data_size == 0) {
        printf("SendData <- Empty data\n");
        return true;
    }

    if (!websocket_connection) {
        printf("SendData <- WebSocket connection is NULL\n");
        return false;
    }

    soup_websocket_connection_send_binary(websocket_connection, data, data_size);

    printf("SendData <-\n");
    return true;
}

bool WebSocketClient::Close()
{
    if (websocket_connection)
        soup_websocket_connection_close(websocket_connection, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);

    return true;
}

bool WebSocketClient::IsConnected() const
{
    return (websocket_connection != nullptr);
}

bool WebSocketClient::Connect(const std::string &server_addr)
{
    this->websocket_connection = nullptr;
    this->server_addr = server_addr;

    // Create the soup session with WS or WSS
    SoupSession *session = soup_session_new();
    if (!session) {
        printf("Fail to create SoupSession\n");
        return false;
    }

    // Build the connect Uri and SSL certificate
    char *uri = g_strdup_printf("%s://%s", "http",  server_addr.c_str());

    // Create the soup message
    SoupMessage *msg = soup_message_new("GET", uri);
    if (!msg) {
        printf("Fail to create SoupMessage\n");
        g_object_unref(session);
        g_free(uri);
        return false;
    }

    // Connect to our websocket server
    printf("Async connect to websocket -> %s\n", uri);
    soup_session_websocket_connect_async(
            session,
            msg,
            NULL, NULL, NULL,
            (GAsyncReadyCallback) &OnConnection,
            this
    );
    printf("Async connect to websocket <-\n");

    g_clear_object(&msg);
    g_free(uri);
    g_clear_object(&session);
    return true;
}

bool WebSocketClient::OnReconnect()
{
    Connect(server_addr);
    return false;
}

void WebSocketClient::OnConnectionImp(SoupSession *session, GAsyncResult *res)
{
    GError *error = NULL;
    SoupWebsocketConnection *conn = soup_session_websocket_connect_finish(session, res, &error);
    if (error) {

        // Try to reconnect to SS. Random the waiting time to prevent DDoS our own server.
        unsigned int sleep_time = static_cast<unsigned int>(Glib::Rand().get_int_range(3000, 8000));

        printf("Fail to connect to websocket server: %s. Try to reconnect after %d ms\n", error->message, sleep_time);
        Glib::signal_timeout().connect(sigc::mem_fun(*this, &WebSocketClient::OnReconnect), sleep_time);

        g_error_free(error);
        g_clear_object(&conn);
        return;
    }
    printf("Success to connect to Websocket server\n");

    g_signal_connect(conn, "message",  G_CALLBACK(OnMessage),  this);
    g_signal_connect(conn, "closing",  G_CALLBACK(OnClosing),  this);
    g_signal_connect(conn, "closed",   G_CALLBACK(OnClose),    this);
    g_signal_connect(conn, "error",    G_CALLBACK(OnError),    this);
    g_signal_connect(conn, "pingpong", G_CALLBACK(OnPingPong), this);

    websocket_connection = conn;

    // Emit the websocket connect signal
    if (!connect_signal.empty()) connect_signal.emit();
}

void WebSocketClient::OnMessageImp(SoupWebsocketConnection *conn, int type, GBytes *gbytes)
{
    if (type == SOUP_WEBSOCKET_DATA_TEXT) {

        unsigned int message_size;
        std::string message = static_cast<const char *>(g_bytes_get_data(gbytes, (gsize *)&message_size));
        printf("Received text data: %s\n", message.c_str());
        message_signal.emit(message);
    }
    else if (type == SOUP_WEBSOCKET_DATA_BINARY) {

        unsigned int data_size;
        const unsigned char *data = static_cast<const unsigned char *>(g_bytes_get_data(gbytes, (gsize *)&data_size));
        printf("Received binary data: (%d bytes)\n", data_size);
        data_signal.emit(data, data_size);
    }
    else {
        printf("Invalid data type: %d\n", type);
    }
}

void WebSocketClient::OnClosingImp(SoupWebsocketConnection *conn)
{
    printf("Websocket is closing\n");
}

void WebSocketClient::OnCloseImp(SoupWebsocketConnection *conn)
{
    // Disconnect all signals
    g_signal_handlers_disconnect_by_data(conn, this);
    g_clear_object(&conn);
    websocket_connection = nullptr;
    printf("Websocket is closed\n");

    if (!closed_signal.empty()) closed_signal.emit();
}

void WebSocketClient::OnErrorImp(SoupWebsocketConnection *conn, GError *error)
{
    g_signal_handlers_disconnect_by_data(conn, this);
    g_clear_object(&conn);
    websocket_connection = nullptr;


    // Try to reconnect to SS. Random the waiting time to prevent DDoS our own server.
    unsigned int sleep_time = static_cast<unsigned int>(Glib::Rand().get_int_range(3000, 8000));
    printf("Websocket is error (%s). Try to reconnect after %d ms\n", error->message, sleep_time);
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &WebSocketClient::OnReconnect), sleep_time);
}

void WebSocketClient::OnPingPongImp(SoupWebsocketConnection *conn)
{
    if (!pingpong_signal.empty()) pingpong_signal.emit();
}

/*
 * Callbacks for libsoup
 */
void WebSocketClient::OnConnection(SoupSession *session, GAsyncResult *res, gpointer data)
{
    ((WebSocketClient *) data)->OnConnectionImp(session, res);
}

void WebSocketClient::OnMessage(SoupWebsocketConnection *conn, int type, GBytes *message, gpointer data)
{
    ((WebSocketClient *) data)->OnMessageImp(conn, type, message);
}

void WebSocketClient::OnClosing(SoupWebsocketConnection *conn, gpointer data)
{
    ((WebSocketClient *) data)->OnClosingImp(conn);
}

void WebSocketClient::OnClose(SoupWebsocketConnection *conn, gpointer data)
{
    ((WebSocketClient *) data)->OnCloseImp(conn);
}

void WebSocketClient::OnError(SoupWebsocketConnection *conn, GError *error, gpointer data)
{
    ((WebSocketClient *)data)->OnErrorImp(conn, error);
}

void WebSocketClient::OnPingPong(SoupWebsocketConnection *conn, gpointer data)
{
    ((WebSocketClient *) data)->OnPingPongImp(conn);
}