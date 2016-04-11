#pragma once

#include <string>

#include <glib.h>
#include <glib-unix.h>
#include <libsoup/soup.h>
#include <sigc++/signal.h>

typedef sigc::slot<void> NotifyConnectedSlot;
typedef sigc::slot<void, const std::string&> NotifyMessageSlot;
typedef sigc::slot<void, const unsigned char *, unsigned int> NotifyDataSlot;
typedef sigc::slot<void> NotifyClosedSlot;
typedef sigc::slot<void> NotifyPingPongSlot;

typedef sigc::signal<void> NotifyConnectedSignal;
typedef sigc::signal<void, const std::string&> NotifyMessageSignal;
typedef sigc::signal<void, const unsigned char*, unsigned int> NotifyDataSignal;
typedef sigc::signal<void> NotifyClosedSignal;
typedef sigc::signal<void> NotifyPingPongSignal;

class WebSocketClient
{
public:

    WebSocketClient() : websocket_connection(nullptr) {}

    ~WebSocketClient() {}

    void RegisterSignals(const NotifyConnectedSlot &connect_slot,
                         const NotifyMessageSlot &message_slot,
                         const NotifyDataSlot &data_slot,
                         const NotifyClosedSlot &closed_slot,
                         const NotifyPingPongSlot &pingpong_slot);

    bool IsConnected() const;

    bool Connect(const std::string &server_addr);

    bool SendMessage(const std::string &message);

    bool SendData(const unsigned char *data, unsigned int data_size);

    bool Close();

private:

    /*
     * Callbacks for libsoup
     */
    static void OnConnection(SoupSession *session, GAsyncResult *res, gpointer data);

    static void OnMessage(SoupWebsocketConnection *conn, int type, GBytes *message, gpointer data);

    static void OnClosing(SoupWebsocketConnection *conn, gpointer data);

    static void OnClose(SoupWebsocketConnection *conn, gpointer data);

    static void OnError(SoupWebsocketConnection *conn, GError *error, gpointer data);

    static void OnPingPong(SoupWebsocketConnection *conn, gpointer data);

    /*
     * Implementation for those callbacks
     */
    void OnConnectionImp(SoupSession *session, GAsyncResult *res);

    void OnMessageImp(SoupWebsocketConnection *conn, int type, GBytes *gBytes);

    void OnClosingImp(SoupWebsocketConnection *conn);

    void OnCloseImp(SoupWebsocketConnection *conn);

    void OnErrorImp(SoupWebsocketConnection *conn, GError *error);

    void OnPingPongImp(SoupWebsocketConnection *conn);

    bool OnReconnect();

    std::string server_addr;
    NotifyConnectedSignal connect_signal;
    NotifyMessageSignal message_signal;
    NotifyDataSignal data_signal;
    NotifyClosedSignal closed_signal;
    NotifyPingPongSignal pingpong_signal;
    SoupWebsocketConnection* websocket_connection;
};