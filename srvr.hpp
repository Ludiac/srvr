#pragma once
#include <unordered_set>
#include "sockets.hpp"

#include <QTcpServer>
#include <QThreadPool>
#include <mariadb/conncpp.hpp>

class Server : public QTcpServer {
  Q_OBJECT

public:
  Server();
  void ping(qintptr descriptor);
  void sign_in(QString && tag, QString && password, qintptr descriptor);
  void sign_up(QString &&login, QString &&client_name, QString && password, qintptr descriptor);
  void message(qint32 && room_id, QString && text, qintptr descriptor);
  void room_messages(qint32 && room_id, qintptr descriptor);
  void rooms_of_client(qintptr descriptor);
  void registered_clients(qintptr descriptor);
  void room_members(qint32 &&room_id, qintptr descriptor);
  void create_private_room(qint32 &&client, qintptr descriptor);
  void create_public_room(QString &&name_for_room, qintptr descriptor);
  void remove_socket(qintptr descriptor);
protected:
  void incomingConnection(qintptr socketDescriptor) override;

private:
  bool connect_to_db();

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
  create_public_room
};

template<typename... Targs>
QByteArray make_block(Targs... targs) {
  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_6_5);
  (out << ... << targs);
  return block;
}




