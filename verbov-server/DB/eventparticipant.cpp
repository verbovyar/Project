#include "eventparticipant.h"

#include <QtSql/QSqlTableModel>
#include <QtSql/QSqlQuery>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QSqlError>

#include <optional>

#include "user.h"
#include "event.h"

const char EventParticipant::table_name[] = "EventParticipants";

#define CHECK(expr) if (!(expr)) { return false; }
bool EventParticipant::run_tests(QSqlDatabase& test_db) {
    return true;
}
#undef CHECK

bool EventParticipant::unpack_from_query(QSqlQuery& query) {
    bool right_variant = true;

    event_id = query.value("event_id").toULongLong(&right_variant);
    if (!right_variant) {
        // Some programming or db error, treat as a failed query.
        return false;
    }

    user_id = query.value("user_id").toULongLong(&right_variant);
    if (!right_variant) {
        // Some programming or db error, treat as a failed query.
        return false;
    }

    registered_time = query.value("registered_time").toULongLong(&right_variant);
    if (!right_variant) {
        // Some programming or db error, treat as a failed query.
        return false;
    }

    return true;
}

void EventParticipant::pack_into_query(QSqlQuery& query) const {
    query.bindValue(":event_id", QVariant::fromValue(event_id));
    query.bindValue(":user_id", QVariant::fromValue(user_id));
    query.bindValue(":registered_time", QVariant::fromValue(registered_time));
}

bool EventParticipant::check_table(QSqlDatabase& db) {
    // QSqlQuery docs: https://doc.qt.io/qt-6/sql-sqlstatements.html
    QSqlQuery query(db);

    // SQL LIKE operator. https://www.w3schools.com/sql/sql_like.asp
    // Turns out ids in sqlite start from 1. That's great.
    //   https://stackoverflow.com/questions/692856/set-start-value-for-autoincrement-in-sqlite
    //   The initial value for SQLITE_SEQUENCE.seq must be null or 0. But from tests seems like ids
    //   start from 1.
    //   https://stackoverflow.com/a/9053277
    // We have to write NOT NULL at PRIMARY KEYS of not integer type for sqlite due to a bug.
    //   https://stackoverflow.com/a/64778551
    if (!query.prepare(
        "CREATE TABLE IF NOT EXISTS " + QString(table_name) + "("
            "event_id        INTEGER    NOT NULL                           CHECK(event_id >= 1),"
            "user_id         INTEGER    NOT NULL                           CHECK(user_id >= 1),"
            "registered_time INTEGER(8) NOT NULL                           CHECK(registered_time >= 0),"
            "PRIMARY KEY (event_id, user_id),"
            "FOREIGN KEY (event_id) REFERENCES " + QString(Event::table_name) + "(id) ON DELETE CASCADE,"
            "FOREIGN KEY (user_id) REFERENCES "  + QString(User::table_name)  + "(vk_id) ON DELETE RESTRICT"
    ");")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    return true;
}

bool EventParticipant::fetch(QSqlDatabase& db, quint64 event_id, quint64 user_id, std::optional<EventParticipant>& found_participation) {
    QSqlQuery query(db);

    if (!query.prepare("SELECT * FROM " + QString(table_name) + " WHERE event_id = :event_id AND user_id = :user_id")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":event_id", QVariant::fromValue(event_id));
    query.bindValue(":user_id", QVariant::fromValue(user_id));

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    if (!query.first()) {
        // Haven't found anything. But the query is successful.
        found_participation.reset();
        return true;
    }

    EventParticipant participant;

    if (!participant.unpack_from_query(query)) {
        // Failed to unpack. Treat as a failed query.
        return false;
    }

    found_participation = std::move(participant);

    return true;
}

bool EventParticipant::fetch_all_for_event(QSqlDatabase& db, quint64 event_id, QVector<EventParticipant>& found_participations) {
    QSqlQuery query(db);

    if (!query.prepare("SELECT * FROM " + QString(table_name) + " WHERE event_id = :event_id")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":event_id", QVariant::fromValue(event_id));

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    found_participations.clear();

    if (query.first()) {
        do {
            EventParticipant participant;

            if (!participant.unpack_from_query(query)) {
                // Failed to unpack. Treat as a failed query.
                return false;
            }

            found_participations.push_back(std::move(participant));
        } while (query.next());
    }

    return true;
}

bool EventParticipant::fetch_all_for_user(QSqlDatabase& db, quint64 user_id, QVector<EventParticipant>& found_participations) {
    QSqlQuery query(db);

    if (!query.prepare("SELECT * FROM " + QString(table_name) + " WHERE user_id = :user_id")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":user_id", QVariant::fromValue(user_id));

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    found_participations.clear();

    if (query.first()) {
        do {
            EventParticipant participant;

            if (!participant.unpack_from_query(query)) {
                // Failed to unpack. Treat as a failed query.
                return false;
            }

            found_participations.push_back(std::move(participant));
        } while (query.next());
    }

    return true;
}

bool EventParticipant::create(QSqlDatabase& db) {
    QSqlQuery query(db);

    if (!query.prepare(
        "INSERT INTO " + QString(table_name) + "(event_id, user_id, registered_time)"
        " VALUES (:event_id, :user_id, :registered_time)"
    )) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    pack_into_query(query);

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    return true;
}

bool EventParticipant::update(QSqlDatabase& db) {
    QSqlQuery query(db);

    // По-хорошему, надо запоминать, какие поля обновлялись и их
    //   только обновлять. Чтобы над разными полями можно было
    //   работать параллельно. Или, например, добавлять участников
    //   события параллельно, т.к. это вставка в базу данных участников.
    if (!query.prepare(
        "UPDATE " + QString(table_name) + " "
        "SET registered_time = :registered_time "
        "WHERE event_id = :event_id AND user_id = :user_id"
    )) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    pack_into_query(query);

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    return true;
}

bool EventParticipant::drop(QSqlDatabase& db) {
    QSqlQuery query(db);

    if (!query.prepare(
        "DELETE FROM " + QString(table_name) + " "
        "WHERE event_id = :event_id AND user_id = :user_id"
    )) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":event_id", QVariant::fromValue(event_id));
    query.bindValue(":user_id", QVariant::fromValue(user_id));

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    return true;
}

// ..., аналогично предыдущим моделям

EventParticipant::EventParticipant() {}

// Хотим уметь посылать такие объекты клиенту.
QDataStream& operator<<(QDataStream& out, const EventParticipant& entry) {
    out << entry.event_id;
    out << entry.user_id;
    out << entry.registered_time;
    return out;
}

QDataStream& operator>>(QDataStream& in, EventParticipant& entry) {
    in >> entry.event_id;
    in >> entry.user_id;
    in >> entry.registered_time;
    return in;
}
