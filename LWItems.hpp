#pragma once

#include "qlistwidget.h"
#include "srvr.hpp"

class GeneralLWItem : public QListWidgetItem {
public:
  GeneralLWItem(qint32 id, QString text, QListWidget *parent, int type);
  virtual QVariant data(int role) const;

private:
  qint32 id;
};

class ClientLWItem : public QListWidgetItem {
public:
  ClientLWItem(QHash<qint32, Client>::Iterator client, QListWidget *parent, int type);

  virtual QVariant data(int role) const;

private:
  QHash<qint32, Client>::Iterator client_ptr;
};
