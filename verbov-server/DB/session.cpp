#include "session.h"

#include <QtSql/QSqlTableModel>
#include <QtSql/QSqlQuery>
#include <QCryptographicHash>
#include <QDateTime>
#include <QSqlError>

#include <random>
#include <optional>

#include "user.h"

const QString Session::table_name = "Sessions";

#define CHECK(expr) if (!(expr)) { return false; }
bool Session::run_tests(QSqlDatabase& test_db) {
    Session session1;

    User user1;
    user1.first_name = "Ярослав";
    user1.last_name = "Вербов";
    user1.vk_id = 2;
    user1.set_password(QString("234"));
    CHECK(user1.create(test_db));

    session1.user_id = user1.get_vk_id();
    CHECK(session1.generate_token(test_db));
    session1.set_time_started();
    CHECK(!session1.is_expired());

    // Test CRUD.

    // Create.
    CHECK(session1.create(test_db));
    // Create should fail, because such row already exists (primary key and unique constraints will fail).
    CHECK(!session1.create(test_db));

    // Read.
    std::optional<Session> session1_from_db;
    CHECK(fetch_by_id(test_db, session1.id, session1_from_db));
    CHECK(session1_from_db.has_value());
    CHECK(session1_from_db.value() == session1);

    CHECK(fetch_by_token(test_db, session1.token, session1_from_db));
    CHECK(session1_from_db.has_value());
    CHECK(session1_from_db.value() == session1);

    // Check non-existent session.
    std::optional<Session> nonexistent_session_from_db;
    CHECK(fetch_by_id(test_db, 0, nonexistent_session_from_db))
    CHECK(!nonexistent_session_from_db.has_value());
    CHECK(fetch_by_token(test_db, QString("hehe№haha?"), nonexistent_session_from_db))
    CHECK(!nonexistent_session_from_db.has_value());

    // Update.
    // Should be able to update.
    // Обязательно все поля меняем, чтобы протестировать update.
    session1.start_time = session1.start_time + 2;
    session1.token = QString('?').repeated(64);
    CHECK(session1.update(test_db));
    CHECK(fetch_by_id(test_db, session1.id, session1_from_db));
    CHECK(session1_from_db.value() == session1);

    // Delete.
    CHECK(session1.drop(test_db));
    CHECK(fetch_by_token(test_db, session1.token, nonexistent_session_from_db)); // ID was set to zero, so let's use token.
    CHECK(!nonexistent_session_from_db.has_value());

    CHECK(user1.drop(test_db));

    return true;
}
#undef CHECK

bool Session::unpack_from_query(QSqlQuery& query) {
    bool right_variant = true;
    id = query.value("id").toULongLong(&right_variant);
    if (!right_variant) {
        // Some programming or db error, treat as a failed query.
        return false;
    }

    user_id = query.value("user_id").toULongLong(&right_variant);
    if (!right_variant) {
        // Some programming or db error, treat as a failed query.
        return false;
    }

    token = query.value("token").toString();

    start_time = query.value("start_time").toULongLong(&right_variant);
    if (!right_variant) {
        // Some programming or db error, treat as a failed query.
        return false;
    }

    return true;
}

void Session::pack_into_query(QSqlQuery& query, bool fill_id) const {
    if (fill_id) {
        // If we are creating a row, then id is not known, we should not set it.
        query.bindValue(":id", QVariant::fromValue(id));
    }

    query.bindValue(":user_id", QVariant::fromValue(user_id));
    query.bindValue(":token", token);
    query.bindValue(":start_time", QVariant::fromValue(start_time));
}

bool Session::check_table(QSqlDatabase& db) {
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
        "CREATE TABLE IF NOT EXISTS " + table_name + "("
        "id INTEGER                NOT NULL PRIMARY KEY AUTOINCREMENT CHECK(id >= 1),"
        "user_id INTEGER           NOT NULL                           CHECK(user_id >= 1),"
        "token VARCHAR(64)         NOT NULL UNIQUE                    CHECK(LENGTH(token) = 64),"
        "start_time INTEGER(8)     NOT NULL                           CHECK(start_time >= 0),"
        "FOREIGN KEY (user_id) REFERENCES " + User::table_name + "(vk_id) ON DELETE RESTRICT"
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

bool Session::fetch_by_id(QSqlDatabase& db, quint64 id, std::optional<Session>& found_session) {
    Session session;
    QSqlQuery query(db);

    if (!query.prepare("SELECT * FROM " + table_name + " WHERE id = :id")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":id", QVariant::fromValue(id));

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    if (!query.first()) {
        // Haven't found anything. But the query is successful.
        found_session.reset();
        return true;
    }

    if (!session.unpack_from_query(query)) {
        // Failed to unpack. Treat as a failed query.
        return false;
    }

    found_session = std::move(session);

    return true;
}

bool Session::fetch_by_token(QSqlDatabase& db, const QStringView token, std::optional<Session>& found_session) {
    // Almost ctrl-c + ctrl-v from code for retrieval by id.
    // Query one row by an unique column.

    Session session;
    QSqlQuery query(db);

    if (!query.prepare("SELECT * FROM " + table_name + " WHERE token = :token")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":token", token.toString());

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    if (!query.first()) {
        // Haven't found anything. But the query is successful.
        found_session.reset();
        return true;
    }

    if (!session.unpack_from_query(query)) {
        // Failed to unpack. Treat as a failed query.
        return false;
    }

    found_session = std::move(session);

    return true;
}

bool Session::create(QSqlDatabase& db) {
    QSqlQuery query(db);

    if (!query.prepare(
        "INSERT INTO " + table_name + "(user_id, token, start_time)"
                                      " VALUES (:user_id, :token, :start_time)"
    )) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    pack_into_query(query, false);

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    bool right_variant = false;
    id = query.lastInsertId().toULongLong(&right_variant);
    if (!right_variant) {
        // The only other way is to throw an exception..
        // But then we have to create a custom exception.
        abort();
    }

    return true;
}

bool Session::update(QSqlDatabase& db) {
    QSqlQuery query(db);

    if (!query.prepare(
        "UPDATE " + table_name + " " +
        "SET user_id = :user_id, token = :token, start_time = :start_time "
        "WHERE id = :id"
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

bool Session::drop(QSqlDatabase& db) {
    QSqlQuery query(db);

    if (!query.prepare("DELETE FROM " + table_name + " WHERE id = :id")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":id", QVariant::fromValue(id));

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    id = 0; // id == 0 -- строка еще не в БД.

    return true;
}

bool Session::generate_token(QSqlDatabase& db) {
    if (!token.isEmpty()) {
        // Токен уже есть, генерировать не надо.
        return true;
    }

    static const int max_num_iters = 1000;
    static std::random_device rd;
    static std::mt19937_64 mt(rd());

    for (int i = 0; i < max_num_iters; ++i) {
        quint64 value = mt();

        // QCryptographicHash docs: https://doc.qt.io/qt-5/qcryptographichash.html
        QCryptographicHash hash(QCryptographicHash::Algorithm::Sha3_256);
        auto bytes = QByteArray::fromRawData(reinterpret_cast<char*>(&value), sizeof(value));
        hash.addData(bytes);
        token = hash.result().toHex();

        QSqlQuery query(db);
        if (!query.prepare("SELECT 1 FROM " + table_name + " WHERE token = (:token);")) {
            // Failed to execute the query.
            qCritical() << query.lastError().text();
            return false;
        }
        query.bindValue(":token", token);
        if (!query.exec()) {
            // Failed to execute the query.
            qCritical() << query.lastError().text();
            return false;
        }

        if (!query.first()) {
            // This is an unused token.
            return true;
        }
    }

    return false;
}

void Session::set_time_started() {
    // https://stackoverflow.com/a/4460647
    // There is also QDateTime::currentSecsSinceEpoch().
    start_time = QDateTime::currentSecsSinceEpoch();
}

bool Session::is_expired() {
    return start_time > QDateTime::currentSecsSinceEpoch() + max_duration_sec;
}


Session::Session() {}
