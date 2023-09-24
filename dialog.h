#pragma once

#include "LWItems.hpp"
#include "qlistwidget.h"
#include "srvr.hpp"
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class Dialog;
}
QT_END_NAMESPACE

class Dialog : public QDialog {
  Q_OBJECT

public:
  Dialog(QWidget *parent = nullptr);
  ~Dialog();

private slots:
  void ui_add_online_client(qint32 client_id, QString client_tag);
  void ui_remove_online_client(qint32 client_id);
  void ui_add_registered_client(QString client_tag, qint32 client_id);
  void ui_remove_registered_client();
  void ui_add_message(qint32 room_id, QString message_text);
  void ui_add_room(qint32 room_id, QString room_name);
  void ui_add_room_member(qint32 room_id, qint32 client_id);
  void ui_update_selected_client(const QHash<qint32, Client>::Iterator client);

  void on_privateRoomsLst_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_publicRoomsLst_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_banBtn_clicked();
  void on_registeredClientsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_onlineClientsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_roomMembersList_currentItemChanged(QListWidgetItem *current,
                                             QListWidgetItem *previous);
  void on_bannedClientsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_disconnectBtn_clicked();

  void on_connectionDBReadyBtn_clicked();

  void on_listeningReadyBtn_clicked();

  void on_defaultMariaServerBtn_clicked();

private:
  void ui_initiation();

private:
  Ui::Dialog *ui;
  Server server;
  qint32 current_room_active;
  QHash<qint32, Client>::Iterator selected_client;
  QHash<qint32, Client> clients;
};

