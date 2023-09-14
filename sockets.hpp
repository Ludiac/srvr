  #pragma once

#include <QTcpSocket>
#include <QMutex>

class Server;

struct MSocket : QObject {
  Q_OBJECT

public:
  MSocket(qintptr socketDescriptor, Server* server);
  ~MSocket() {
    qDebug() << descriptor << "socket destroyed";
  }

  std::unique_ptr<QTcpSocket> socket = std::make_unique<QTcpSocket>();
  qintptr descriptor;
  qint32 id_in_db;

signals:
  void ready_to_parse(qint32 mes_type, QString str, qintptr descriptor);
  void send_this(QByteArray block);

private slots:
  void start_reading();
  void send(QByteArray block);
  void disconnectt();

private:
  QMutex mutex;
  Server* server;
};

class SocketsCollection {
public:
  MSocket* push(qintptr socketDescriptor, Server* server);
  MSocket* register_connected(qint32 id, qintptr descriptor);
  MSocket* find_connected(qintptr descriptor);
  MSocket* find_connected(qint32 id);
  void remove_connected(qintptr descriptor);
  QString find_connected_tag(qintptr descriptor);
private:
  std::unordered_map<qint32, qintptr> registered;
  std::unordered_map<qintptr, std::unique_ptr<MSocket>> unregistered; //we hold a pointer to MSocket because it enables move semantics for MSocket (Q_OBJECT's are not movable)
};
