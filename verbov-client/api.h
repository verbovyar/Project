#ifndef API_H
#define API_H

#include <QString>

namespace api {
    struct Result {
        bool success = false;
        QByteArray content;
    };

    enum class HttpMethod {
        Get    = 0,
        Post   = 1,
        Patch  = 2,
        Delete = 3,
    };

    Result request(const QString& method, const QVector<QString>& args, const QVector<QString>& values, const QString& token = "", const HttpMethod& httpMethod = HttpMethod::Get);
}

#endif // API_H
