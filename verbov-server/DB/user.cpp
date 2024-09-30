#include "user.h"

#include <optional>

#include <QtSql/QSqlTableModel>
#include <QtSql/QSqlQuery>
#include <QCryptographicHash>
#include <QSqlError>

#include "eventparticipant.h"

const char User::table_name[] = "Users";

// Shouldn't use QString here.
// clazy complains it's a global static non-POD objeect.
// https://stackoverflow.com/questions/1538137/c-static-global-non-pod-theory-and-practice
// https://stackoverflow.com/a/1538193
// https://stackoverflow.com/a/1539744
// https://kde-frameworks-devel.kde.narkive.com/fzXydSnh/qstring-qstringliteral-conversions-might-make-applications-crash-on-exit
static const char password_salt[] = "#7923sdjjfkddrjkhjfjdflksjglmxc.,vmlk;g";
// Если БД утечет, подобрать именно пароли не получится путем подбора строки,
//   у которой такой же хеш. Вот бы еще код не утек.

#define CHECK(expr) if (!(expr)) { return false; }
bool User::run_tests(QSqlDatabase& test_db) {
    User user1;
    user1.first_name = "Ярослав";
    user1.last_name = "Вербов";
    user1.vk_id = 2;
    user1.reg_confirmed = false;
    user1.reg_code = 1234;
    user1.set_password(QString("234"));

    // Test password.
    CHECK(user1.check_password(QString("234")));
    CHECK(!user1.check_password(QString("235")));

    // Test CRUD.

    // Create.
    CHECK(user1.create(test_db));
    // Create should fail, because such row already exists (primary key and unique constraints will fail).
    CHECK(!user1.create(test_db));

    // Read.
    std::optional<User> user1_from_db;
    CHECK(fetch_by_vk_id(test_db, user1.vk_id, user1_from_db));
    CHECK(user1_from_db.has_value());
    CHECK(user1_from_db.value() == user1);

    // Check non-existent user.
    std::optional<User> nonexistent_user_from_db;
    CHECK(fetch_by_vk_id(test_db, 10000, nonexistent_user_from_db))
    CHECK(!nonexistent_user_from_db.has_value());

    // Update.
    // Should be able to update.
    // Обязательно все поля меняем, чтобы протестировать update.
    user1.first_name = user1.first_name.replace("с", "c"); // Русскую "с" заменил на латинскую, подмену не заметят)
    user1.last_name = user1.last_name.replace("о", "o"); // Русскую "о" заменил на латинскую, подмену не заметят)
    user1.reg_confirmed = true;
    user1.set_password(QString("heheHaHa?"));
    CHECK(user1.update(test_db));
    CHECK(fetch_by_vk_id(test_db, user1.vk_id, user1_from_db));
    CHECK(user1_from_db.value() == user1);

    // Delete.
    CHECK(user1.drop(test_db));
    CHECK(fetch_by_vk_id(test_db, user1.vk_id, nonexistent_user_from_db)); // ID was set to zero, so let's use email.
    CHECK(!nonexistent_user_from_db.has_value());

    return true;
}
#undef CHECK

static QString hash_password(const QStringView password) {
    // QCryptographicHash docs: https://doc.qt.io/qt-5/qcryptographichash.html
    QCryptographicHash hash(QCryptographicHash::Algorithm::Sha3_256);
    hash.addData(password.toUtf8());
    hash.addData(password_salt);
    return hash.result().toHex();
}

bool User::unpack_from_query(QSqlQuery& query) {
    // Unpack a returned user. First row is extracted.
    bool right_variant = true;
    vk_id = query.value("vk_id").toULongLong(&right_variant);
    if (!right_variant) {
        // Some programming or db error, treat as a failed query.
        return false;
    }

    first_name = query.value("first_name").toString();
    last_name = query.value("last_name").toString();

    right_variant = true;
    vk_id = query.value("vk_id").toULongLong(&right_variant);
    if (!right_variant) {
        // Some programming or db error, treat as a failed query.
        return false;
    }

    reg_confirmed = query.value("reg_confirmed").toBool();

    right_variant = true;
    reg_code = query.value("reg_code").toULongLong(&right_variant);
    if (!right_variant) {
        // Some programming or db error, treat as a failed query.
        return false;
    }

    password_hash = query.value("password_hash").toString();

    return true;
}
void User::pack_into_query(QSqlQuery& query) const {
    query.bindValue(":first_name", first_name);
    query.bindValue(":last_name", last_name);
    query.bindValue(":vk_id", vk_id);
    query.bindValue(":reg_confirmed", reg_confirmed);
    query.bindValue(":reg_code", reg_code);
    query.bindValue(":password_hash", password_hash);
}

bool User::check_table(QSqlDatabase& db) {
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
        "vk_id         INTEGER     NOT NULL PRIMARY KEY CHECK(vk_id >= 1),"
        "first_name    VARCHAR(64) NOT NULL             CHECK(first_name != ''),"
        "last_name     VARCHAR(64) NOT NULL             CHECK(last_name != ''),"
        "reg_confirmed BOOL        NOT NULL,"
        "reg_code      INTEGER     NOT NULL             CHECK(reg_code >= 0),"
        "password_hash VARCHAR(64) NOT NULL             CHECK(LENGTH(password_hash) = 64)"
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

bool User::fetch_by_vk_id(QSqlDatabase& db, quint64 vk_id, std::optional<User>& found_user) {
    // Almost ctrl-c + ctrl-v from code for retrieval by id.
    // Query one row by an unique column.

    User user;
    QSqlQuery query(db);

    if (!query.prepare("SELECT * FROM " + QString(table_name) + " WHERE vk_id = :vk_id")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":vk_id", vk_id);

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    if (!query.first()) {
        // Haven't found anything. But the query is successful.
        found_user.reset();
        return true;
    }

    if (!user.unpack_from_query(query)) {
        // Failed to unpack. Treat as a failed query.
        return false;
    }

    found_user = std::move(user);

    return true;
}

bool User::fetch_by_event_id(QSqlDatabase& db, quint64 event_id, QVector<User>& found_users) {
    QSqlQuery query(db);

    if (!query.prepare(
        "SELECT * FROM " + QString(table_name) + " WHERE vk_id IN ("
        "SELECT user_id FROM " + EventParticipant::table_name + " WHERE event_id = :event_id"
    ")")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":event_id", event_id);

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    found_users.clear();

    if (!query.first()) {
        // Haven't found anything. But the query is successful.
        return true;
    }

    do {
        User user;

        if (!user.unpack_from_query(query)) {
            // Failed to unpack. Treat as a failed query.
            return false;
        }

        found_users.push_back(std::move(user));
    } while (query.next());

    return true;
}

bool User::create(QSqlDatabase& db) {
    QSqlQuery query(db);

    if (!query.prepare(
        "INSERT INTO " + QString(table_name) + "(first_name, last_name, vk_id, reg_confirmed, reg_code, password_hash)"
        " VALUES (:first_name, :last_name, :vk_id, :reg_confirmed, :reg_code, :password_hash)"
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


bool User::update(QSqlDatabase& db) {
    QSqlQuery query(db);

    if (!query.prepare(
        "UPDATE " + QString(table_name) + " " +
        "SET first_name = :first_name, last_name = :last_name, vk_id = :vk_id, reg_confirmed = :reg_confirmed, reg_code = :reg_code, password_hash = :password_hash "
        "WHERE vk_id = :vk_id"
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

bool User::drop(QSqlDatabase& db) {
    QSqlQuery query(db);

    if (!query.prepare("DELETE FROM " + QString(table_name) + " WHERE vk_id = :vk_id")) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }
    query.bindValue(":vk_id", QVariant::fromValue(vk_id));

    if (!query.exec()) {
        // Failed to execute the query.
        qCritical() << query.lastError().text();
        return false;
    }

    return true;
}

void User::set_password(const QStringView password) {
    password_hash = hash_password(password);
}

bool User::check_password(const QStringView input_password) const {
    return password_hash == hash_password(input_password);
}

// Хотим уметь посылать клиенту информацию о пользователе.

QDataStream& operator<<(QDataStream& out, const User& entry) {
    out << entry.vk_id;
    out << entry.first_name;
    out << entry.last_name;
    out << entry.reg_confirmed;
    out << entry.reg_code;

    return out;
}

QDataStream& operator>>(QDataStream& in,  User& entry) {
    in >> entry.vk_id;
    in >> entry.first_name;
    in >> entry.last_name;
    in >> entry.reg_confirmed;
    in >> entry.reg_code;

    return in;
}
