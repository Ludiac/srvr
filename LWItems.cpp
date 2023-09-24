#include "LWItems.hpp"

GeneralLWItem::GeneralLWItem(qint32 id, QString text, QListWidget *parent,
                             int type)
    : id{id}, QListWidgetItem{text, parent, type} {}

QVariant GeneralLWItem::data(int role) const {
  if (role == 1001) {
    return id;
  }
  return QListWidgetItem::data(role);
}

ClientLWItem::ClientLWItem(QHash<qint32, Client>::Iterator client,
                           QListWidget *parent, int type)
    : client_ptr{client}, QListWidgetItem{client->tag, parent, type} {}

QVariant ClientLWItem::data(int role) const {
  if (role == 1001) {
    QVariant variant;
    variant.setValue(client_ptr);
    return variant;
  }
  return QListWidgetItem::data(role);
}
