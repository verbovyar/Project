#include "enrolldialog.h"
#include "ui_enrolldialog.h"

#include "DB/event.h"

#include "api.h"

EnrollDialog::EnrollDialog(const QString& token, QWidget *parent)
    : token_(token)
    , QDialog(parent)
    , ui(new Ui::EnrollDialog)
{
    ui->setupUi(this);
}

EnrollDialog::~EnrollDialog()
{
    delete ui;
}

void EnrollDialog::on_referEdit_textEdited(const QString &text)
{
    static const QString rejectedReferStylesheet = "color: red;";
    if (text.length() > 9) {
        ui->referEdit->setStyleSheet(rejectedReferStylesheet);
        ui->msgLbl->clear();
        return;
    } else if (text.length() < 9) {
        ui->referEdit->setStyleSheet(QString());
        ui->msgLbl->clear();
        return;
    }

    const QString& refer = text;

    api::Result result = api::request("/event_register", {"refer"}, {refer}, token_, api::HttpMethod::Post);

    if (!result.success) {
        ui->referEdit->setStyleSheet(rejectedReferStylesheet);
        ui->msgLbl->setText(QString(result.content));
    } else {
        Event event;

        QDataStream stream(result.content);
        stream >> event;

        maybe_event = std::move(event);

        // Сообщим, что ввод принят, окно можно закрывать.
        accept();
    }
}
