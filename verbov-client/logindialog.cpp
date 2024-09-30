#include "logindialog.h"
#include "ui_logindialog.h"

#include <QByteArray>
#include <QToolTip>
#include <QFile>

#include "mainwindow.h"
#include "api.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

    // Prevent resizing.
    setFixedSize(width(), height());

    QFile token_file("token.txt");
    if (token_file.exists()) {
        if (token_file.open(QFile::OpenModeFlag::ReadOnly)) {
            QString token(token_file.readAll());
            token.remove('\n');
            token.remove('\r');

            api::Result result = api::request("get_me", {}, {}, token);
            if (!result.success) {
                QToolTip::showText(QCursor::pos(), result.content);
            } else {
                // TODO: проверять здесь, что сессия с сохранненым токеном не истекла.
                // TODO: при просроченном токене на любой операции с API предупреждать,
                // что нужно перезапустить приложение.
                // TODO: на сервере в каждом типе запросов проверять, что токен валиден
                // с запасом в пару часов. За пару часов работы, если токен истек, такое
                // не так страшно, пользователь уже успел поработать.

                QDataStream stream(result.content);
                User user(0);
                stream >> user;

                onLoggedIn(std::move(user), std::move(token));
                return;
            }
        }
    }

    show();
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::on_regBtn_clicked()
{
    QString vkProfile = ui->vkEdit->text();
    // QString pass = ui->passEdit->text();
    // qInfo() << vkId << " " << pass;

    api::Result result = api::request("register", {"vk_profile"}, {vkProfile});

    QToolTip::showText(QCursor::pos(), result.content);
}

void LoginDialog::onLoggedIn(User user, QString token) {
    mainWnd.emplace(std::move(user), std::move(token));
    mainWnd->show();

    hide();
}

void LoginDialog::on_loginBtn_clicked()
{
    QString vkProfile = ui->vkEdit->text();
    QString pass = ui->passEdit->text();

    api::Result result = api::request("login", {"vk_profile", "password"}, {vkProfile, pass});

    if (result.success) {
        QDataStream stream(result.content);

        User user(0);
        QString token(result.content);

        stream >> user;
        stream >> token;

        QFile token_file("token.txt");
        if (token_file.open(QFile::OpenModeFlag::WriteOnly)) {
            token_file.write(token.toUtf8());
        }

        onLoggedIn(std::move(user), std::move(token));
    } else {
        QToolTip::showText(QCursor::pos(), result.content);
    }
}


void LoginDialog::on_helpBtn_clicked()
{
    QToolTip::showText(
        QCursor::pos(),
        "Профиль в формате id1234 или username.\n"
        "Для регистрации введите профиль и нажмите соответствующую кнопку."
        );
}
