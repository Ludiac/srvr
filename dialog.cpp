#include "dialog.h"
#include "./ui_dialog.h"

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QtNetwork>

Dialog::Dialog(QWidget *parent) : QDialog(parent), ui(new Ui::Dialog) {
  ui->setupUi(this);

  if (!server.listen(QHostAddress{"localhost"}, 7999)) {
    close();
    return;
  } else {
    ui->infoDisplay->appendPlainText(QString{"server is listening\n"});
  }
}

Dialog::~Dialog() { delete ui; }

void Dialog::on_quitButton_clicked() { QApplication::quit(); }

void Dialog::on_sendButton_clicked() {}
