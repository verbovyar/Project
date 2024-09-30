#ifndef ENROLLDIALOG_H
#define ENROLLDIALOG_H

#include <optional>

#include <QDialog>

#include "DB/event.h"

namespace Ui {
class EnrollDialog;
}

class EnrollDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EnrollDialog(const QString& token, QWidget *parent = nullptr);
    ~EnrollDialog();

private slots:
    void on_referEdit_textEdited(const QString &arg1);

private:
    const QString& token_;

    Ui::EnrollDialog *ui;

public:
    std::optional<Event> maybe_event;
};

#endif // ENROLLDIALOG_H
