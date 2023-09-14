#pragma once

#include <QDialog>
#include "srvr.hpp"

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
  void on_quitButton_clicked();

  void on_sendButton_clicked();

private:
  Ui::Dialog *ui;
  Server server;
};
