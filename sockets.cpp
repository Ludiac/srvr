 #include "sockets.hpp"
#include "srvr.hpp"

MSocket::MSocket(qintptr socketDescriptor, Server* server, QObject *parent) : server{server}, QObject(parent) {
  socket.setSocketDescriptor(socketDescriptor);
  descriptor = socket.socketDescriptor();
  qDebug() << "socket descriptor: " << socket.socketDescriptor();
  connect(&socket, &QTcpSocket::readyRead, this, &MSocket::start_reading);
  connect(&socket, &QTcpSocket::disconnected, this, &MSocket::disconnectt);
  connect(this, &MSocket::send_this, this, &MSocket::send);
}

void MSocket::start_reading() {
  QThreadPool::globalInstance()->start([&]() {
    QDataStream in(&socket);
    in.setVersion(QDataStream::Qt_6_5);
    message_type mes_type;
    while (socket.bytesAvailable()) {
      mutex.lock();
      in.startTransaction();
      in >> mes_type;
      qDebug() << "received from descriptor:" << descriptor << "mes_type:" << static_cast<std::underlying_type_t<message_type>>(mes_type) << "...";
      switch(mes_type) {
      case message_type::ping: {
        //not implemented
      }; break;
      case message_type::sign_in: {
        QString tag, password;
        in >> tag >> password;
        in.commitTransaction();
        mutex.unlock();
        qDebug() << "..." << "tag:" << tag << "password:"<< password;
        server->sign_in(std::move(tag), std::move(password), descriptor);
      }; break;
      case message_type::sign_up: {
        QString tag, name, password;
        in >> tag >> name >> password;
        in.commitTransaction();
        mutex.unlock();
        qDebug() << "..." << "tag:" << tag << "name:" << name << "password:"<< password;
        //QThreadPool::globalInstance()->start([=, this]() mutable {
        server->sign_up(std::move(tag), std::move(name), std::move(password), descriptor);
        //});
      }; break;
      case message_type::message: {
        qint32 room_id;
        QString text;
        in >> room_id >> text;
        in.commitTransaction();
        mutex.unlock();
        qDebug() << "..." << "room_id:" << room_id << "text:" << text;
        server->message(std::move(room_id), std::move(text), descriptor);
      }; break;
      case message_type::room_messages: {
        qint32 room_id;
        in >> room_id;
        in.commitTransaction();
        mutex.unlock();
        qDebug() << "..." << "room_id:" << room_id;
        server->room_messages(std::move(room_id), descriptor);
      }; break;
      case message_type::rooms_of_client: {
        in.commitTransaction();
        mutex.unlock();
        server->rooms_of_client(descriptor);
      }; break;
      case message_type::registered_clients: {
        in.commitTransaction();
        mutex.unlock();
        server->registered_clients(descriptor);
      }; break;
      case message_type::room_members: {
        qint32 room_id;
        in >> room_id;
        in.commitTransaction();
        mutex.unlock();
        qDebug() << "..." << "room_id:" << room_id;
        server->room_members(std::move(room_id), descriptor);
      }; break;
      case message_type::create_private_room: {
        qint32 client_id;
        in >> client_id;
        in.commitTransaction();
        mutex.unlock();
        qDebug() << "..." << "client_id:" << client_id;
        server->create_private_room(std::move(client_id), descriptor);
      }; break;
      case message_type::create_public_room: {
        QString name_for_room;
        in >> name_for_room;
        in.commitTransaction();
        mutex.unlock();
        qDebug() << "..." << "name_for_room:" << name_for_room;
        server->create_public_room(std::move(name_for_room), descriptor);
      }; break;
      case message_type::invitation_in_public_room: {
        qint32 room_id, client_id;
        in >> room_id >> client_id;
        in.commitTransaction();
        mutex.unlock();
        qDebug() << "..." << "room_id:" << room_id << ", client_id:" << client_id;
        server->invite_client(room_id, client_id, descriptor);
      }; break;
      default: std::terminate();
      }
    }
  });
}

void MSocket::send(QByteArray block) {
  mutex.lock();
  socket.write(block);
  mutex.unlock();
  qDebug() << "written to" << descriptor;
}

void MSocket::disconnectt() {
  server->remove_socket(descriptor);
}

MSocket* SocketsCollection::push(qintptr socketDescriptor, Server* server) {
  auto&& b = unregistered.emplace(socketDescriptor, new MSocket(socketDescriptor, server, server));
  return b.value().get();
}

MSocket* SocketsCollection::register_connected(qint32 id, qintptr descriptor)
{
  auto&& a = unregistered.find(descriptor);
  a.value()->client_id_in_db = id;
  if (a != unregistered.end()) {
    registered.emplace(id, descriptor);
    return a.value().get();
  } else {
    qDebug() << "bug occured. register_connected failed. terminating";
    std::terminate();
  }
}

MSocket* SocketsCollection::find_connected(qintptr descriptor) {
  if (auto&& b = unregistered.find(descriptor); b != unregistered.end()) {
    return b.value().get();
  } else {
    qDebug() << "bug occured! find_connected(descriptor) failed";
    return nullptr;
    //std::terminate();
  }
}

MSocket* SocketsCollection::find_connected(qint32 id) {
  if (auto&& b = registered.find(id); b != registered.end()) {
    return find_connected(b.value());
  } else {
    return nullptr;
    //qDebug() << "bug occured! find_connected(qint32) did not find. terminating";
  }
}

void SocketsCollection::remove_connected(qintptr descriptor) {
  registered.remove(unregistered.find(descriptor).value()->client_id_in_db);
  unregistered.remove(descriptor);
}

