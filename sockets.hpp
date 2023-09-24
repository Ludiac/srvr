  #pragma once

#include <QTcpSocket>
#include <QMutex>
#include <QPointer>

class Server;

struct MSocket : QObject {
  Q_OBJECT

public:
  MSocket(qintptr socketDescriptor, Server* server, QObject* parent);
  ~MSocket() {
    qDebug() << descriptor << "socket destroyed";
  }

  QTcpSocket socket;
  qintptr descriptor;
  qint32 client_id_in_db;

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

public: //public for convenience, should be private
  QHash<qint32, qintptr> registered;
  QHash<qintptr, QPointer<MSocket>> unregistered;
};
