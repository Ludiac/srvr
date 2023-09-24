#include "dialog.h"
#include "./ui_dialog.h"

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QtNetwork>
#include <QtWidgets>

Dialog::Dialog(QWidget *parent) : QDialog(parent), ui(new Ui::Dialog) {
  ui->setupUi(this);
  connect(&server, &Server::ui_new_online_client, this,
          &Dialog::ui_add_online_client);

  connect(&server, &Server::ui_pop_online_client, this,
          &Dialog::ui_remove_online_client);

  connect(&server, &Server::ui_new_registered_client, this,
          &Dialog::ui_add_registered_client);

  connect(&server, &Server::ui_pop_registered_client, this,
          &Dialog::ui_remove_registered_client);

  connect(&server, &Server::ui_new_room_message, this, &Dialog::ui_add_message);

  connect(&server, &Server::ui_new_room, this, &Dialog::ui_add_room);

  connect(&server, &Server::ui_new_room_member, this,
          &Dialog::ui_add_room_member);
}

Dialog::~Dialog() { delete ui; }

void Dialog::ui_add_online_client(qint32 client_id, QString client_tag) {
  auto &&a = clients.insert(client_id, Client{client_tag, false});
  auto &&item = new ClientLWItem{a, ui->onlineClientsList, 1002};
  a->item_in_onlineList = item;
  a->online = true;
}

void Dialog::ui_remove_online_client(qint32 client_id) {
  if (auto &&client = clients.find(client_id); client != clients.end()) {
    if (client->online) {
      ui->onlineClientsList->removeItemWidget(client->item_in_onlineList);
      delete client->item_in_onlineList;
      client->online = false;
      client->item_in_onlineList = nullptr;
    }
  }
}

void Dialog::ui_add_registered_client(QString client_tag, qint32 client_id) {
  auto &&a = clients.insert(client_id, Client{client_tag, false});
  auto &&item = new ClientLWItem{a, ui->registeredClientsList, 1002};
  a->item_in_registeredList = item;
}

void Dialog::ui_remove_registered_client() {}

void Dialog::ui_add_message(qint32 room_id, QString message_text) {
  if (current_room_active == room_id) {
    ui->roomMessagesTE->append(message_text + "\n");
  }
}

void Dialog::ui_add_room(qint32 room_id, QString room_name) {
  if (room_name == "") {
    QVector<qint32> clients_id = server.room_participations(room_id);
    QString client_one = clients.find(clients_id[0])->tag;
    QString client_two = clients.find(clients_id[1])->tag;
    new GeneralLWItem(room_id, QString{client_one + " : " + client_two},
                      ui->privateRoomsLst, 1001);
  } else {
    new GeneralLWItem(room_id, room_name, ui->publicRoomsLst, 1001);
  }
}

void Dialog::ui_add_room_member(qint32 room_id, qint32 client_id) {
  if (current_room_active != room_id)
    return;
  auto &&client = clients.find(client_id);
  new ClientLWItem{client, ui->roomMembersList, 1002};
}

void Dialog::ui_update_selected_client(
    const QHash<qint32, Client>::Iterator client) {
  selected_client = client;
  if (selected_client->ban_status == true) {
    ui->banBtn->setText("unban " + client->tag);
  } else {
    ui->banBtn->setText("ban " + client->tag);
  }

  if (selected_client->online == true) {
    ui->disconnectBtn->setText("disconnect" + client->tag);
    ui->disconnectBtn->setEnabled(true);
  } else {
    ui->disconnectBtn->setText("disconnect");
    ui->disconnectBtn->setDisabled(true);
  }
}

void Dialog::on_banBtn_clicked() {
  auto &&client = clients.find(selected_client.key());
  if (client->ban_status == false) {
    client->ban_status = true;
    server.change_ban_status(client.key(), true);
    auto &&item = new ClientLWItem{client, ui->bannedClientsList, 1002};
    client->item_in_bannedList = item;
  } else {
    server.change_ban_status(client.key(), false);
    ui->bannedClientsList->removeItemWidget(client->item_in_bannedList);
    delete client->item_in_bannedList;
    client->ban_status = false;
    client->item_in_bannedList = nullptr;
  }
}

void Dialog::on_privateRoomsLst_currentItemChanged(QListWidgetItem *current,
                                                   QListWidgetItem *previous) {
  qint32 room_id = current->data(1001).toInt();
  current_room_active = room_id;
  ui->roomMembersList->clear();
  ui->roomMessagesTE->clear();
  QStringList messages = server.ui_room_messages(room_id);
  for (auto &&message : messages) {
    ui->roomMessagesTE->append(message + "\n");
  }
}

void Dialog::on_publicRoomsLst_currentItemChanged(QListWidgetItem *current,
                                                  QListWidgetItem *previous) {
  qint32 room_id = current->data(1001).toInt();
  current_room_active = room_id;
  ui->roomMembersList->clear();
  QVector<qint32> clients_id = server.room_participations(room_id);
  for (auto client_id : clients_id) {
    auto &&clientei = clients.find(client_id);
    new ClientLWItem(clientei, ui->roomMembersList, 1002);
  }
  ui->roomMessagesTE->clear();
  QStringList messages = server.ui_room_messages(room_id);
  for (auto &&message : messages) {
    ui->roomMessagesTE->append(message + "\n");
  }
}

void Dialog::on_registeredClientsList_currentItemChanged(
    QListWidgetItem *current, QListWidgetItem *previous) {
  ui_update_selected_client(
      current->data(1001).value<QHash<qint32, Client>::Iterator>());
}

void Dialog::on_onlineClientsList_currentItemChanged(
    QListWidgetItem *current, QListWidgetItem *previous) {
  if (current)
    ui_update_selected_client(
        current->data(1001).value<QHash<qint32, Client>::Iterator>());
}

void Dialog::on_roomMembersList_currentItemChanged(QListWidgetItem *current,
                                                   QListWidgetItem *previous) {
  if (current)
    ui_update_selected_client(
        current->data(1001).value<QHash<qint32, Client>::Iterator>());
}

void Dialog::on_bannedClientsList_currentItemChanged(
    QListWidgetItem *current, QListWidgetItem *previous) {
  ui_update_selected_client(
      current->data(1001).value<QHash<qint32, Client>::Iterator>());
}

void Dialog::on_disconnectBtn_clicked() {
  auto &&client = clients.find(selected_client.key());
  server.force_disconnect_socket(client.key());
}

void Dialog::on_connectionDBReadyBtn_clicked() {
  QString host{ui->mariaHostLE->text()}, admin_name{ui->adminNameLE->text()},
      admin_password{ui->adminPasswordLE->text()},
      database_name{ui->databaseNameLE->text()};

  bool success = server.connect_to_db(host, 3306, admin_name, admin_password,
                                      database_name);
  if (!success) {
    QMessageBox msgBox;
    msgBox.setText("error. something is wrong");
    msgBox.exec();
    return;
  }
  ui->stackedWidget->setCurrentIndex(1);
}

void Dialog::on_defaultMariaServerBtn_clicked() {
  bool success =
      server.connect_to_db("188.242.64.99", 3306, "userrr", "1234", "serverr");
  if (!success) {
    QMessageBox msgBox;
    msgBox.setText("error. something is wrong");
    msgBox.exec();
    return;
  }
  ui->stackedWidget->setCurrentIndex(1);
}

void Dialog::on_listeningReadyBtn_clicked() {
  qint32 port{ui->serverPortLE->text().toInt()};
  bool success = server.start_listening(port);
  if (!success) {
    QMessageBox msgBox;
    msgBox.setText("error. something is wrong");
    msgBox.exec();
    return;
  }

  ui->listeningLabel->setText("listening to host: "
                              "localhost"
                              "; port: " +
                              QString{std::to_string(port).c_str()});
  ui->stackedWidget->setCurrentIndex(0);
  ui_initiation();
}

void Dialog::ui_initiation() {
  QVector<QPair<qint32, Client>> clientss = server.registered_clients();
  for (auto &&client : clientss) {
    auto &&a = clients.insert(client.first, client.second);
    auto &&item = new ClientLWItem{a, ui->registeredClientsList, 1002};
    a->item_in_registeredList = item;
    if (a->ban_status == true) {
      auto &&itemm = new ClientLWItem{a, ui->bannedClientsList, 1002};
      a->item_in_bannedList = itemm;
    }
  }

  QVector<QPair<qint32, QString>> rooms = server.all_open_rooms();
  for (auto &&room : rooms) {
    if (room.second == "") {
      QVector<qint32> clients_id = server.room_participations(room.first);
      QString client_one = clients.find(clients_id[0])->tag;
      QString client_two = clients.find(clients_id[1])->tag;
      new GeneralLWItem(room.first, QString{client_one + " : " + client_two},
                        ui->privateRoomsLst, 1001);
    } else {
      new GeneralLWItem(room.first, room.second, ui->publicRoomsLst, 1001);
    }
  }
}
