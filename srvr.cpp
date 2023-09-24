#include "srvr.hpp"

Server::Server() {}

QVector<QPair<qint32, Client>> Server::registered_clients() {
  std::unique_ptr<sql::PreparedStatement> stmnt(
      conn->prepareStatement("select id, tag, ban_status from user"));
  sql::ResultSet *result = stmnt->executeQuery();
  QVector<QPair<qint32, Client>> clients;
  while (result->next()) {
    clients.append(QPair<qint32, Client>{
        result->getInt(1),
        Client{result->getString(2).c_str(), result->getBoolean(3)}});
  }
  return clients;
}

QVector<QPair<qint32, QString>> Server::all_open_rooms() {
  std::unique_ptr<sql::PreparedStatement> stmnt(
      conn->prepareStatement("select id, name from room"));
  sql::ResultSet *result = stmnt->executeQuery();
  QVector<QPair<qint32, QString>> rooms;
  while (result->next()) {
    rooms.append(
        QPair<qint32, QString>{result->getInt(1), result->getString(2)});
  }
  return rooms;
}

QVector<qint32> Server::room_participations(qint32 room_id) {
  std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
      "select user_id from participates where room_id = ?"));
  stmnt->setInt(1, room_id);
  sql::ResultSet *result = stmnt->executeQuery();
  QVector<qint32> participations;
  while (result->next()) {
    participations.append(result->getInt(1));
  }
  return participations;
}

QStringList Server::ui_room_messages(qint32 room_id) {
  std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
      "select u.tag, m.text from message m left join user u on u.id = "
      "m.user_id where m.room_id = ?"));
  stmnt->setInt(1, room_id);
  sql::ResultSet *result = stmnt->executeQuery();
  QStringList messages;
  while (result->next()) {
    messages.append(
        QString{result->getString(1) + ": " + result->getString(2)});
  }
  return messages;
}

void Server::change_ban_status(qint32 client_id, bool new_ban_status) {
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(
        conn->prepareStatement("update user set ban_status = ? where id = ?"));
    stmnt->setBoolean(1, new_ban_status);
    stmnt->setInt(2, client_id);
    stmnt->executeQuery();
    if (auto &&a = sockets.find_connected(client_id); a)
      emit a->send_this(make_block(message_type::ban));
  } catch (sql::SQLException &e) {
    qDebug() << "Error while changing ban status:" << e.what();
  }
}

void Server::ping(qintptr descriptor) {}

void Server::sign_in(QString &&tag, QString &&password, qintptr descriptor) {
  qint32 counter = 0;
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
        "select id, hash, ban_status from user where tag = ?"));
    stmnt->setString(1, tag.toStdString());
    sql::ResultSet *result = stmnt->executeQuery();
    if (!result->next()) {
      qDebug() << "invalid login info";
      emit sockets.find_connected(descriptor)
          ->send_this(make_block(message_type::sign_in, false, QString{}, 1));
      return;
    }
    if (result->getBoolean(3) == true) {
      qDebug() << "banned user tried to sign_in with tag" << tag;
      emit sockets.find_connected(descriptor)
          ->send_this(make_block(message_type::sign_in, false, QString{}, 2));
      return;
    }
    if (result->getString(2) != password) {
      qDebug() << "invalid password";
      emit sockets.find_connected(descriptor)
          ->send_this(make_block(message_type::sign_in, false, QString{}, 3));
      return;
    }
    qint32 client_id = result->getInt(1);
    emit sockets.register_connected(client_id, descriptor)
        ->send_this(make_block(message_type::sign_in, true, tag, client_id));
    emit ui_new_online_client(client_id, tag);
  } catch (sql::SQLException &e) {
    qDebug() << "Error while signing in:" << e.what();
  }
}

void Server::sign_up(QString &&tag, QString &&client_name, QString &&password,
                     qintptr descriptor) {
  qint32 counter = 0;
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
        "insert into user(tag, name, hash) values(?, ?, ?) returning id"));
    stmnt->setString(1, tag.toStdString());
    stmnt->setString(2, client_name.toStdString());
    stmnt->setString(3, password.toStdString());
    sql::ResultSet *result = stmnt->executeQuery();
    result->next();
    qint32 client_id = result->getInt(1);
    for (auto &&i : sockets.registered) {
      emit sockets.find_connected(i)->send_this(
          make_block(message_type::new_client_registered, client_id, tag));
    }
    emit sockets.register_connected(client_id, descriptor)
        ->send_this(make_block(message_type::sign_up, true, tag, client_id));
    emit ui_new_registered_client(tag, client_id);
    emit ui_new_online_client(client_id, tag);
  } catch (sql::SQLException &e) {
    qDebug() << "Error while signing up:" << e.what();
    emit sockets.find_connected(descriptor)
        ->send_this(make_block(message_type::sign_up, false, tag, 0));
  }
}

void Server::message(qint32 room_id, QString &&text, qintptr descriptor) {
  try {
    qint32 sender_id = sockets.find_connected(descriptor)->client_id_in_db;
    qint32 message_id;
    {
      std::unique_ptr<sql::PreparedStatement> stmnt(
          conn->prepareStatement("insert into message(user_id, room_id, text) "
                                 "values (?, ?, ?) returning id"));
      stmnt->setInt(1, sender_id);
      stmnt->setInt(2, room_id);
      stmnt->setString(3, text.toStdString());
      sql::ResultSet *result = stmnt->executeQuery();
      result->next();
      message_id = result->getInt(1);
    }
    {
      std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
          "select user_id from participates where room_id = ?"));
      stmnt->setInt(1, room_id);
      sql::ResultSet *result = stmnt->executeQuery();
      while (result->next()) {
        if (auto &&a = sockets.find_connected(result->getInt(1)); a)
          emit a->send_this(make_block(message_type::message, room_id,
                                       message_id, sender_id, text));
      }
    }
    emit ui_new_room_message(room_id, text);
  } catch (sql::SQLException &e) {
    qDebug() << "Error sending message:" << e.what();
  }
}

void Server::room_messages(qint32 room_id, qintptr descriptor) {
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(
        conn->prepareStatement("select message.id, message.user_id, "
                               "message.text from message where room_id = ?"));
    stmnt->setInt(1, room_id);
    sql::ResultSet *result = stmnt->executeQuery();
    QVector<QPair<qint32, QPair<qint32, QString>>> messages;
    while (result->next()) {
      messages.append({result->getInt(1),
                       {result->getInt(2), result->getString(3).c_str()}});
    }
    emit sockets.find_connected(descriptor)
        ->send_this(make_block(message_type::room_messages, room_id, messages));
  } catch (sql::SQLException &e) {
    qDebug() << "Error getting room_messages:" << e.what();
  }
}

void Server::rooms_of_client(qintptr descriptor) {
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
        "select r.id, r.name from participates p left join room r "
        "on p.room_id = r.id where p.user_id = ?"));
    stmnt->setInt(1, sockets.find_connected(descriptor)->client_id_in_db);
    sql::ResultSet *result = stmnt->executeQuery();
    QVector<QPair<qint32, QString>> rooms;
    while (result->next()) {
      rooms.append({result->getInt(1), result->getString(2).c_str()});
    }
    emit sockets.find_connected(descriptor)
        ->send_this(make_block(message_type::rooms_of_client, rooms));
  } catch (sql::SQLException &e) {
    qDebug() << "Error getting rooms of client:" << e.what();
  }
}

void Server::registered_clients(qintptr descriptor) {
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(
        conn->prepareStatement("select id, tag from user"));
    sql::ResultSet *result = stmnt->executeQuery();
    QVector<QPair<qint32, QString>> clients;
    while (result->next()) {
      clients.append({result->getInt(1), result->getString(2).c_str()});
    }
    emit sockets.find_connected(descriptor)
        ->send_this(make_block(message_type::registered_clients, clients));
  } catch (sql::SQLException &e) {
    qDebug() << "Error getting registered clientss:" << e.what();
  }
}

void Server::room_members(qint32 room_id, qintptr descriptor) {
  try {
    QVector<qint32> room_members;
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(
        "select user_id from participates where room_id = ?"));
    stmnt->setInt(1, room_id);
    sql::ResultSet *result = stmnt->executeQuery();
    while (result->next()) {
      room_members.append(result->getInt(1));
    }
    emit sockets.find_connected(descriptor)
        ->send_this(
            make_block(message_type::room_members, room_id, room_members));
  } catch (sql::SQLException &e) {
    qDebug() << "Error getting room members:" << e.what();
  }
}

void Server::create_private_room(qint32 other_client_id, qintptr descriptor) {
  try {
    auto &&room_creator = sockets.find_connected(descriptor)->client_id_in_db;
    {
      std::unique_ptr<sql::PreparedStatement> stmnt(
          conn->prepareStatement("call private_room(?, ?)"));
      qDebug() << room_creator << other_client_id;
      stmnt->setInt(1, room_creator);
      stmnt->setInt(2, other_client_id);
      stmnt->executeQuery();
      rooms_of_client(descriptor);
      if (auto &&other_client = sockets.find_connected(other_client_id);
          other_client) {
        rooms_of_client(other_client->descriptor);
      }
    }
    { // server ui stuff
      std::unique_ptr<sql::PreparedStatement> stmnt(
          conn->prepareStatement("SELECT MAX(Id) FROM room"));
      sql::ResultSet *result = stmnt->executeQuery();
      result->next();
      emit ui_new_room(result->getInt(1), "");
    }
  } catch (sql::SQLException &e) {
    qDebug() << "Error creating private room:" << e.what();
  }
}

void Server::create_public_room(QString &&name_for_room, qintptr descriptor) {
  try {
    auto &&room_creator = sockets.find_connected(descriptor)->client_id_in_db;
    {
      std::unique_ptr<sql::PreparedStatement> stmnt(
          conn->prepareStatement("call public_room(?, ?)"));
      stmnt->setInt(1, room_creator);
      stmnt->setString(2, name_for_room.toStdString());
      stmnt->executeQuery();
      rooms_of_client(descriptor); // inefficient
    }
    {                              // server ui stuff
      std::unique_ptr<sql::PreparedStatement> stmnt(
          conn->prepareStatement("SELECT MAX(Id) FROM room"));
      sql::ResultSet *result = stmnt->executeQuery();
      result->next();
      emit ui_new_room(result->getInt(1), name_for_room);
    }
  } catch (sql::SQLException &e) {
    qDebug() << "Error creating public room:" << e.what();
  }
}

void Server::invite_client(qint32 room_id, qint32 client_id,
                           qintptr descriptor) {
  try {
    {
      std::unique_ptr<sql::PreparedStatement> stmnt1(conn->prepareStatement(
          "insert into participates(room_id, user_id) values(?, ?)"));
      stmnt1->setInt(1, room_id);
      stmnt1->setInt(2, client_id);
      stmnt1->executeQuery();
    }
    {
      std::unique_ptr<sql::PreparedStatement> stmnt2(conn->prepareStatement(
          "select user_id from participates where room_id = ?"));
      stmnt2->setInt(1, room_id);
      sql::ResultSet *room_members = stmnt2->executeQuery();
      while (room_members->next()) {
        if (auto &&a = sockets.find_connected(room_members->getInt(1)); a) {
          if (room_members->getInt(1) == client_id) {
            std::unique_ptr<sql::PreparedStatement> stmnt3(
                conn->prepareStatement("select name from room where id = ?"));
            stmnt3->setInt(1, room_id);
            sql::ResultSet *result3 = stmnt3->executeQuery();
            result3->next();
            QString room_name = result3->getString(1).c_str();
            emit a->send_this(
                make_block(message_type::invitation_in_public_room, room_id,
                           client_id, room_name));
          } else {
            emit a->send_this(make_block(message_type::add_new_member_to_room,
                                         room_id, client_id));
          }
        }
      }
    }
    emit ui_new_room_member(room_id, client_id);
  } catch (sql::SQLException &e) {
    qDebug() << "Error inviting user:" << e.what();
  }
}

void Server::incomingConnection(qintptr socketDescriptor) {
  auto &&b = sockets.push(socketDescriptor, this);
  qDebug() << "new socket created";
}

bool Server::connect_to_db(QString host, qint32 port, QString admin_name,
                           QString admin_password, QString database_name) {
  try {
    std::string urll = "jdbc:mariadb://" + host.toStdString() + ':' +
                       std::to_string(port) + '/' + database_name.toStdString();
    driver = sql::mariadb::get_driver_instance();
    sql::SQLString url = urll;
    sql::Properties properties({{"user", admin_name.toStdString()},
                                {"password", admin_password.toStdString()}});
    conn.reset(driver->connect(url, properties));
  } catch (sql::SQLException &e) {
    qDebug() << "Error Connecting to MariaDB Platform:" << e.what();
    return false;
  }
  return true;
}

void Server::remove_socket(qintptr descriptor) {
  if (auto &&socket = sockets.find_connected(descriptor); !socket)
    return;
  else {
    emit ui_pop_online_client(socket->client_id_in_db);
    socket->deleteLater();
    sockets.remove_connected(descriptor);
  }
}

void Client::operator=(Client const &other) {
  this->tag = other.tag;
  this->ban_status = other.ban_status;
  this->online = other.online;
  this->item_in_registeredList = other.item_in_registeredList;
  this->item_in_onlineList = other.item_in_onlineList;
  this->item_in_bannedList = other.item_in_bannedList;
}

void Server::force_disconnect_socket(qint32 client_id) {
  remove_socket(sockets.find_connected(client_id)->descriptor);
}

bool Server::start_listening(qint32 port) {
  if (!listen(QHostAddress{"localhost"}, port)) {
    close();
    return false;
  }
  qDebug() << "server is listening";
  return true;
}
