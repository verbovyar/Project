#ifndef EVENTPARTICIPANT_H
#define EVENTPARTICIPANT_H

#include <cstdint>

#include <QString>
#include <QStringView>

#include <QtSql/QSqlQuery>

class EventParticipant
{
private:
    quint64 event_id = 0;
    quint64 user_id = 0;
public:
    quint64 registered_time = 0; // UTC+0 unix time when the participant was registered onto the event.
    // It is used to not send excessive notification. E.g. user registers the day before the event, we shouldn't notify him twice like
    // "hey, it's less than a week" before the event and then "hey, it's less than a day before the event".
    // Also this only works if server is up at least every day, so that notification duties don't pile up.
public:
    static const char table_name[];
public:
    bool unpack_from_query(QSqlQuery& query);
    void pack_into_query(QSqlQuery& query) const;

    // Проверяет, что таблица существует. Если нет, создает.
    static bool check_table(QSqlDatabase& db);

    static bool run_tests(QSqlDatabase& test_db);

    static bool fetch(QSqlDatabase& db, quint64 event_id, quint64 user_id, std::optional<EventParticipant>& found_participation);
    static bool fetch_all_for_event(QSqlDatabase& db, quint64 event_id, QVector<EventParticipant>& found_participations);
    static bool fetch_all_for_user(QSqlDatabase& db, quint64 user_id, QVector<EventParticipant>& found_participations);
public:
    // Public plain methods.

    quint64 get_event_id() const { return event_id; }
    quint64 get_user_id()  const { return user_id; }

    // Если объекта нет в БД, то update и drop могут вернуть успех, ничего не сделав.
    // Обновить или удалить 0 строк вполне нормально по мнению запроса SQL. Можно проверять
    // что реально какие-то операции были типо RETURNING id добавить какой-нибудь в запросы.
    // Но пока так.

    bool create(QSqlDatabase& db);
    bool update(QSqlDatabase& db);
    bool drop(QSqlDatabase& db);

    bool operator==(const EventParticipant& other) const = default;

    friend QDataStream& operator<<(QDataStream& out, const EventParticipant& entry);
    friend QDataStream& operator>>(QDataStream& in,  EventParticipant& entry);

    EventParticipant(quint64 event_id, quint64 user_id)
        : event_id(event_id), user_id(user_id) {}

    // ...
private:
    EventParticipant();
};

#endif // EVENTPARTICIPANT_H
