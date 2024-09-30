#ifndef PARTICIPANTSDIALOG_H
#define PARTICIPANTSDIALOG_H

#include <QDialog>

#include "DB/event.h"
#include "DB/user.h"

namespace Ui {
class ParticipantsDialog;
}

class ParticipantsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ParticipantsDialog(const QString& token, Event& event, QWidget *parent = nullptr);
    ~ParticipantsDialog();

private slots:
    void on_pushButton_clicked();

private:
    void fill_table();

private:
    const QString& token_;
    Event& event_;

    Ui::ParticipantsDialog *ui;

    QVector<User> participants_;
};

#endif // PARTICIPANTSDIALOG_H
