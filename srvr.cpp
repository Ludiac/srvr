 #include "srvr.hpp"

Server::Server() {
  if (!connect_to_db()) {
    qDebug() << "Oops, u couldn't connect to database. terminating program";
    std::terminate();
  }
}

void Server::ping(qintptr descriptor)
{

}

void Server::sign_in(QString && tag, QString && password, qintptr descriptor)
{
  qint32 counter = 0;
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("select id from user where tag = ? and hash = ?"));
    stmnt->setString(1, tag.toStdString());
    stmnt->setString(2, password.toStdString());
    sql::ResultSet *result = stmnt->executeQuery();
    if (result->next()) {
      qint32 client_id = result->getInt(1);
      emit sockets.register_connected(client_id, descriptor)->send_this(make_block(message_type::sign_in, true, tag, client_id));
    } else {
      qDebug() << "invalid login info";
      emit sockets.find_connected(descriptor)->send_this(make_block(message_type::sign_in, false, tag, 0));
    }
  } catch(sql::SQLException& e) {
    qDebug() << "Error while signing in:" << e.what();
  }
}

void Server::sign_up(QString && tag, QString && client_name, QString && password, qintptr descriptor)
{
  qint32 counter = 0;
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("insert into user(tag, name, hash) values(?, ?, ?) returning id"));
    stmnt->setString(1, tag.toStdString());
    stmnt->setString(2, client_name.toStdString());
    stmnt->setString(3, password.toStdString());
    sql::ResultSet *result = stmnt->executeQuery();
    result->next();
    qint32 client_id = result->getInt(1);
    emit sockets.register_connected(client_id, descriptor)->send_this(make_block(message_type::sign_up, true, tag, client_id));
  } catch(sql::SQLException& e) {
    qDebug() << "Error while signing up:" << e.what();
    emit sockets.find_connected(descriptor)->send_this(make_block(message_type::sign_up, false, tag, 0));
  }
}

void Server::message(qint32 && room_id, QString && text, qintptr descriptor)
{
  try {
    qint32 sender_id = sockets.find_connected(descriptor)->id_in_db;
    qint32 message_id;
    {
      std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("insert into message(user_id, room_id, text) values ((select id from user where tag = ?), ?, ?) returning id"));
      stmnt->setInt(1, sender_id);
      stmnt->setInt(2, room_id);
      stmnt->setString(3, text.toStdString());
      sql::ResultSet *result = stmnt->executeQuery();
      result->next();
      message_id = result->getInt(1);
    }
    {
      std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("select user_id from participates where room_id = ?"));
      stmnt->setInt(1, room_id);
      sql::ResultSet *result = stmnt->executeQuery();
      while (result->next()) {
        auto&& a = sockets.find_connected(result->getInt(1));
        if (a) emit a->send_this(make_block(message_type::message, room_id, message_id, sender_id, text));
      }
    }
  } catch(sql::SQLException& e) {
    qDebug() << "Error sending message:" << e.what();
  }
}

void Server::room_messages(qint32 && room_id, qintptr descriptor)
{
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("select message.id, message.user_id, message.text from message where room_id = ?"));
    stmnt->setInt(1, room_id);
    sql::ResultSet *result = stmnt->executeQuery();
    QVector<QPair<qint32, QPair<qint32, QString>>> messages;
    while (result->next()) {
      messages.append({result->getInt(1), {result->getInt(2), result->getString(3).c_str()}});
    }
    emit sockets.find_connected(descriptor)->send_this(make_block(message_type::room_messages, room_id, messages));
  } catch(sql::SQLException& e) {
    qDebug() << "Error getting room_messages:" << e.what();
  }
}

void Server::rooms_of_client(qintptr descriptor)
{
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("select r.id, r.name from participates p left join room r "
                                                                         "on p.room_id = r.id where p.user_id = ?"));
    stmnt->setInt(1, sockets.find_connected(descriptor)->id_in_db);
    sql::ResultSet *result = stmnt->executeQuery();
    QVector<QPair<qint32, QString>> rooms;
    while (result->next()) {
      rooms.append({result->getInt(1), result->getString(2).c_str()});
    }
    emit sockets.find_connected(descriptor)->send_this(make_block(message_type::rooms_of_client, rooms));
  } catch(sql::SQLException& e) {
    qDebug() << "Error getting rooms of client:" << e.what();
  }
}

void Server::registered_clients(qintptr descriptor)
{
  try {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("select id, tag from user"));
    sql::ResultSet *result = stmnt->executeQuery();
    QVector<QPair<qint32, QString>> clients;
    while (result->next()) {
      clients.append({result->getInt(1), result->getString(2).c_str()});
    }
    emit sockets.find_connected(descriptor)->send_this(make_block(message_type::registered_clients, clients));
  } catch(sql::SQLException& e) {
    qDebug() << "Error getting registered clientss:" << e.what();
  }
}

void Server::room_members(qint32 && room_id, qintptr descriptor)
{
  try {
    QVector<qint32> room_members;
    std::unique_ptr<sql::PreparedStatement> stmnt2(conn->prepareStatement("select user_id from participates where room_id = ?"));
    stmnt2->setInt(1, room_id);
    sql::ResultSet *result2 = stmnt2->executeQuery();
    while (result2->next()) {
      room_members.append(result2->getInt(1));
    }
    emit sockets.find_connected(descriptor)->send_this(make_block(message_type::room_members, room_id, room_members));
  } catch(sql::SQLException& e) {
    qDebug() << "Error getting room members:" << e.what();
  }
}

void Server::create_private_room(qint32 && client, qintptr descriptor)
{
  try {
    auto&& room_creator = sockets.find_connected(descriptor)->id_in_db;
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("call private_room(?, ?)"));
    qDebug() << room_creator << client;
    stmnt->setInt(1, room_creator);
    stmnt->setInt(2, client);
    stmnt->executeQuery();
    rooms_of_client(descriptor);
  } catch(sql::SQLException& e) {
    qDebug() << "Error creating private room:" << e.what();
  }
}

void Server::create_public_room(QString && name_for_room, qintptr descriptor)
{
  try {
    //    auto&& room_creator = sockets.find_connected(descriptor)->id_in_db;
    //    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("call private_room(?, ?)"));
    //    stmnt->setInt(1, room_creator);
    //    stmnt->setInt(2, client);
    //    stmnt->executeQuery();
    //    rooms_of_client(descriptor);
  } catch(sql::SQLException& e) {
    qDebug() << "Error creating private room:" << e.what();
  }
}

void Server::incomingConnection(qintptr socketDescriptor) {
  auto&& b = sockets.push(socketDescriptor, this);
  qDebug() <<  "new socket created";
}

bool Server::connect_to_db() {
  try {
    driver = sql::mariadb::get_driver_instance();
    sql::SQLString url("jdbc:mariadb://localhost:3306/serverr");
    sql::Properties properties({{"user", "userrr"}, {"password", "1234"}});
    conn.reset(driver->connect(url, properties));
  } catch(sql::SQLException& e) {
    qDebug() << "Error Connecting to MariaDB Platform:" << e.what();
    return false;
  }
  return true;
}

void Server::remove_socket(qintptr descriptor) {
  sockets.find_connected(descriptor)->deleteLater();
  sockets.remove_connected(descriptor);
}
