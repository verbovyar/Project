#ifndef EVENT_H
#define EVENT_H

#include <QDataStream>
#include <QMutex>
#include <QString>
#include <QStringView>

#include <QtSql/QSqlQuery>

class Event
{
private:
    quint64 id = 0;
public:
    QString name;
    quint64 creator_user_id = 0;
    quint64 timestamp; // UTC+0 unix time when the event will happen
    QString refer_str;

    // 0 -- never notified
    // 1 -- notified week before the event (if between event creation and it's occurence there is a week)
    // 2 -- notified 3 days before the event
    // 3 -- notified day before the event
    // 4 -- notified 6 hours before the event
    // 5 -- notified an hour before the event
    // 6 -- notified 20 minutes before the event.
    quint8 last_notification_level = 0;
public:
    static const QString table_name;
    static QMutex notification_mutex;
public:
    bool unpack_from_query(QSqlQuery& query);
    void pack_into_query(QSqlQuery& query, bool fill_id = true) const;

    // Проверяет, что таблица существует. Если нет, создает.
    static bool check_table(QSqlDatabase& db);

    static bool run_tests(QSqlDatabase& test_db);

    static bool fetch_by_id(QSqlDatabase& db, quint64 id, std::optional<Event>& found_event);
    static bool fetch_all_for_user(QSqlDatabase& db, quint64 user_id, QVector<Event>& found_events);
    static bool fetch_by_refer(QSqlDatabase& db, const QString& refer_str, std::optional<Event>& found_event);

    static void send_notifications(QSqlDatabase& db);
private:
    static void send_notifications_of_level(QSqlDatabase& db, quint64 now, int level);
public:
    // Public plain methods.

    quint64 get_id() const { return id; }

    // Если объекта нет в БД, то update и drop могут вернуть успех, ничего не сделав.
    // Обновить или удалить 0 строк вполне нормально по мнению запроса SQL. Можно проверять
    // что реально какие-то операции были типо RETURNING id добавить какой-нибудь в запросы.
    // Но пока так.

    bool create(QSqlDatabase& db);
    bool update(QSqlDatabase& db);
    bool drop(QSqlDatabase& db);

    bool generate_refer(QSqlDatabase& db);

    bool operator==(const Event& other) const = default;

    friend QDataStream& operator<<(QDataStream& out, const Event& entry);
    friend QDataStream& operator>>(QDataStream& in,  Event& entry);

// ...
public:
    Event();
};

// Serialization and deserialization.
QDataStream& operator<<(QDataStream& out, const Event& entry);
QDataStream& operator>>(QDataStream& in,  Event& entry);

#endif // EVENT_H
