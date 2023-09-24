#pragma once
#include "sockets.hpp"
#include <unordered_set>

#include <QTcpServer>
#include <QThreadPool>
#include <mariadb/conncpp.hpp>

class Dialog;
class ClientLWItem;

struct Client {
  // qint32 id;
  QString tag;
  bool ban_status;
  bool online;
  ClientLWItem *item_in_registeredList, *item_in_onlineList,
      *item_in_bannedList;

  void operator=(const Client &other);
};

class Server : public QTcpServer {
  Q_OBJECT

public:
  Server();

  qint32 server_port;
  bool connect_to_db(QString host, qint32 port, QString admin_name,
                     QString admin_password, QString database_name);
  bool start_listening(qint32 port);

  void remove_socket(qintptr descriptor);

  QVector<QPair<qint32, Client>> registered_clients();
  QVector<QPair<qint32, QString>> all_open_rooms();
  QVector<qint32> room_participations(qint32 room_id);
  QStringList ui_room_messages(qint32 room_id);
  void change_ban_status(qint32 client_id, bool new_ban_status);

  void ping(qintptr descriptor);
  void sign_in(QString && tag, QString && password, qintptr descriptor);
  void sign_up(QString &&login, QString &&client_name, QString && password, qintptr descriptor);
  void message(qint32 room_id, QString && text, qintptr descriptor);
  void room_messages(qint32 room_id, qintptr descriptor);
  void rooms_of_client(qintptr descriptor);
  void registered_clients(qintptr descriptor);
  void room_members(qint32 room_id, qintptr descriptor);
  void create_private_room(qint32 client, qintptr descriptor);
  void create_public_room(QString &&name_for_room, qintptr descriptor);
  void invite_client(qint32 room_id, qint32 client_id, qintptr descriptor);
  void force_disconnect_socket(qint32 client_id);

protected:
  void incomingConnection(qintptr socketDescriptor) override;

signals:
  void ui_new_online_client(qint32 client_id, QString client_tag);
  void ui_pop_online_client(qint32 client_id);
  void ui_new_registered_client(QString client_tag, qint32 client_id);
  void ui_pop_registered_client();
  void ui_new_room_message(qint32 room_id, QString message_text);
  void ui_new_room(qint32 room_id, QString room_name);
  void ui_new_room_member(qint32 room_id, qint32 client_id);

private:
  SocketsCollection sockets;
  sql::Driver* driver;
  std::unique_ptr<sql::Connection> conn;
};

enum class message_type : qint32 {
  ping = 1,
  sign_in,
  sign_up,
  message,
  room_messages,
  rooms_of_client,
  registered_clients,
  room_members,
  create_private_room,
  create_public_room,
  invitation_in_public_room,
  add_new_member_to_room,
  new_client_registered,
  ban,
  force_disconnect
};

template<typename... Targs>
QByteArray make_block(Targs... targs) {
  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_6_5);
  (out << ... << targs);
  return block;
}




