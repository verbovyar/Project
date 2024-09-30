#include "participantsdialog.h"
#include "ui_participantsdialog.h"

#include <algorithm>

#include <QString>
#include <QToolTip>
#include <QTimer>

#include "api.h"

ParticipantsDialog::ParticipantsDialog(const QString& token, Event& event, QWidget *parent)
    : token_(token)
    , event_(event)
    , QDialog(parent)
    , ui(new Ui::ParticipantsDialog)
{
    ui->setupUi(this);

    ui->tableWidget->resizeColumnsToContents();

    api::Result result = api::request(
        "event_get_participants",
        {"event_id"},
        {QString::number(event_.get_id())},
        token_
    );

    if (result.success) {
        QVector<User> participants;

        QDataStream stream(result.content);
        stream >> participants;

        participants_ = std::move(participants);

        fill_table();
    } else {
        // Покажем ошибку.
        // Без задержки не работает...
        QTimer::singleShot(std::chrono::milliseconds(500), [result]() {
            QToolTip::showText(QCursor::pos(), QString(result.content));
        });

        // qInfo() << QString(result.content);

        // Закроем окно.
        reject();
    }
}

void ParticipantsDialog::fill_table() {
    int num_rows = 0;
    assert(ui->tableWidget->rowCount() == num_rows);

    for (const auto& participant: participants_) {
        int row_index = num_rows; // Вставляем в конец.

        ui->tableWidget->insertRow(row_index);
        ui->tableWidget->setItem(row_index, 0, new QTableWidgetItem(participant.first_name));
        ui->tableWidget->setItem(row_index, 1, new QTableWidgetItem(participant.last_name));
        ui->tableWidget->setItem(row_index, 2, new QTableWidgetItem("https://vk.com/id" + QString::number(participant.get_vk_id())));
    }

    ui->tableWidget->resizeColumnsToContents();
}

ParticipantsDialog::~ParticipantsDialog()
{
    delete ui;
}

void ParticipantsDialog::on_pushButton_clicked()
{
    QItemSelectionModel *selection = ui->tableWidget->selectionModel();

    QVector<size_t> selected_participants;

    for (const QModelIndex& index: selection->selectedIndexes()) {
        selected_participants.push_back(index.row());
    }

    std::sort(selected_participants.begin(), selected_participants.end());

    // Мы удаляем элементы массива, индексы сдвигаются на один у всех элементов,
    // у которых индекс был больше удаляемого.
    size_t index_shift = 0;
    for (size_t participant_index: selected_participants) {
        size_t shifted_index = participant_index - index_shift;

        api::Result result = api::request(
            "event_delete_participant",
            {"event_id", "user_id"},
            {QString::number(event_.get_id()), QString::number(participants_[shifted_index].get_vk_id())},
            token_
        );

        if (result.success) {
            participants_.erase(participants_.begin() + shifted_index);
            ui->tableWidget->removeRow(shifted_index);
            index_shift += 1;
        } else {
            QToolTip::showText(QCursor::pos(), QString(result.content));
            break;
        }
    }
}

