#include "DB/event.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QDateTime>

#include "DB/user.h"
#include "vk.h"

QMutex Event::notification_mutex;

void Event::send_notifications_of_level(QSqlDatabase& db, quint64 now, int level) {
    assert(level >= 1 && level <= 6);

    QSqlQuery pending_events;
    if (!pending_events.prepare(
            "SELECT * FROM " + table_name + " WHERE last_notification_level < :level AND "
                                            "timestamp  > :now AND "
                                            "timestamp <= :now + :level_time_advance AND "
                                            "timestamp  > :now + :next_level_time_advance"
            )) {
        // Failed to execute the query.
        qCritical() << pending_events.lastError().text();
        return;
    }

    // 0 -- never notified
    // 1 -- notified week before the event (if between event creation and it's occurence there is a week)
    // 2 -- notified 3 days before the event
    // 3 -- notified day before the event
    // 4 -- notified 6 hours before the event
    // 5 -- notified an hour before the event
    // 6 -- notified 20 minutes before the event.
    static quint64 level_time_advances[] = {
        0,                 // Никогда не читается.
        7 * 24 * 60 * 60,
        3 * 24 * 60 * 60,
        1 * 24 * 60 * 60,
        6 * 60 * 60,
        1 * 60 * 60,
        20 * 60,
        0                  // Это значит, что при проверке для 6-го типа уведомлений
        // будут учитываться события, которые еще не наступили.
        // но поскольку такая проверка и так есть, все в порядке.
        // По факту получаем дублирующуюся проверку.
    };
    static QString level_description[] = {
        "",
        "осталась неделя",
        "осталось 3 дня",
        "остался день",
        "осталось 6 часов",
        "остался час",
        "осталось менее 20 минут"
    };

    pending_events.bindValue(":level", level);
    pending_events.bindValue(":now", now);
    pending_events.bindValue(":level_time_advance", level_time_advances[level]);
    pending_events.bindValue(":next_level_time_advance", level_time_advances[level + 1]);

    if (!pending_events.exec()) {
        // Failed to execute the query.
        qCritical() << pending_events.lastError().text();
        return;
    }

    // https://doc.qt.io/qt-6/qsqlquery.html#next
    while (pending_events.next()) {
        Event event;

        if (!event.unpack_from_query(pending_events)) {
            // Failed to unpack. Treat as a failed query.
            break;
        }

        assert(event.last_notification_level < level);

        // TODO: хранить часовой пояс пользователя, проставлять в уведомлении
        // его время. На компьютере используется часовой пояс компьютера. А
        // для уведомлений должно быть можно выставлять в настройках пользователя..
        // Просто брать часовой пояс компьютера может быть не удобно.. Хотя можно
        // попробовать сначала так делать, брать часовой пояс компьютера, с которого
        // человек в последний раз заходил.
        const auto event_dt_tz = QTimeZone(3 * 60 * 60); // GMT+3
        const auto event_dt = QDateTime::fromSecsSinceEpoch(event.timestamp, event_dt_tz);

        const QString msg_text =
            "До события \"" + event.name + "\" (" + event_dt.toString()
            + ") " + level_description[level] + ".";

        // Отсылаем сообщение создателю события.

        std::optional<User> maybe_user;
        if (!User::fetch_by_vk_id(db, event.creator_user_id, maybe_user)) {
            // Какая-то ошибка с БД. Выходим. Многим событиям
            // не будет доставлено уведомление, зато мы увидим
            // ошибку быстро.
            break;
        }

        // Ну, это всегда должно выполняться.
        // ON DELETE RESTRICT же на creator_user_id стоит в БД.
        // Если это не так, узнаем в отладочной версии.
        assert(maybe_user.has_value());
        if (maybe_user.has_value()) {
            int vk_error_code = 0;
            QString vk_error_msg;
            if (!vk::send_message(maybe_user->get_vk_id(), msg_text, 0, vk_error_code, vk_error_msg)) {
                qInfo() << "Ошибка VK api при отправке уведомлений. " << vk_error_code << vk_error_msg;
            }
        }

        QVector<User> participants;
        if (!User::fetch_by_event_id(db, event.get_id(), participants)) {
            // Какая-то ошибка с БД. Выходим. Многим событиям
            // не будет доставлено уведомление, зато мы увидим
            // ошибку быстро.
            break;
        }

        for (const User& user: participants) {
            int vk_error_code = 0;
            QString vk_error_msg;
            if (!vk::send_message(maybe_user->get_vk_id(), msg_text, 0, vk_error_code, vk_error_msg)) {
                qInfo() << "Ошибка VK api при отправке уведомлений. " << vk_error_code << vk_error_msg;
            }
        }

        event.last_notification_level = level;
        if (!event.update(db)) {
            // Failed to save new notification level.
            // Stop for now..
            // This way any problem is easy to see:
            // many events won't get their notifications,
            // if something is wrong.
            // Хотя иногда компании выберут, чтобы
            // для всех все работало, чтобы не жаловались.
            // А в одном из случаев работать не будет. И
            // потом эту жалобу тяжело будет отловить...
            // Зато в целом работает... Нет, давайте лучше
            // все ловить проблемы, т.к. при нормальной
            // работе их здесь не должно быть вообще.
            break;
        }
    }
}

void Event::send_notifications(QSqlDatabase& db) {
    QMutexLocker locker(&Event::notification_mutex);

    // Фиксируем текущее время. Так проще рассуждать о корректности
    // (должно быть).
    const quint64 now = QDateTime::currentSecsSinceEpoch();

    for (int level = 1; level <= 6; ++level) {
        Event::send_notifications_of_level(db, now, level);
    }
}
