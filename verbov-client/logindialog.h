#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

#include <optional>

#include "mainwindow.h"

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

private slots:
    void on_regBtn_clicked();

    void on_loginBtn_clicked();

    void on_helpBtn_clicked();

private:
    void onLoggedIn(User user, QString token);

private:
    Ui::LoginDialog *ui;

    std::optional<MainWindow> mainWnd;
};

#endif // LOGINDIALOG_H
