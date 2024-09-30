#include "api.h"

#include <QString>
#include <QVector>
#include <QUrl>
#include <QUrlQuery>
#include <QScopedPointer>
#include <QEventLoop>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

static const QString protocol = "http";
static const QString host = "localhost:8080";

api::Result api::request(const QString& method, const QVector<QString>& args, const QVector<QString>& values, const QString& token, const HttpMethod& httpMethod) {
    // Не выделяем на стеке, т.к. объект может быть большой.
    // Выделяем на куче, даем владеть умному указателю, который сам освободит.
    QScopedPointer<QNetworkAccessManager> netmanager(new QNetworkAccessManager());

    // Этот выделяют на стеке. Видимо, он не такой уж и большой.
    // Заходим внутрь и видим, что внутри много методов и лишь один
    // указатель на какой-то другой объект. Уж 8 байт можно на стеке :)
    QNetworkRequest request;

    // https://forum.qt.io/topic/137547/sending-parameters-by-get-method-to-rest-api/6
    QUrl url(protocol + "://" + host + "/" + method);
    QUrlQuery query;
    query.addQueryItem("token", token);

    Q_ASSERT(args.size() == values.size());
    for (size_t i = 0; i < args.size(); ++i) {
        query.addQueryItem(args[i], values[i]);
    }
    url.setQuery(query);

    request.setUrl(url);

    // Родителем этого объекта должен быть QNetworkManager, по идее, потому
    // с ним освободиться. Не мы выделяли, не мы освобождаем.
    QNetworkReply* reply = [&]() {
        switch (httpMethod) {
        case HttpMethod::Get:  return netmanager->get(request);
        case HttpMethod::Post: {
            // Уберем жалобу qt на отсутствие content type.
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            return netmanager->post(request, QByteArray());
        }
        case HttpMethod::Patch: {
            return netmanager->sendCustomRequest(request, "PATCH");
        }
        case HttpMethod::Delete: {
            return netmanager->deleteResource(request);
        }
        }
        return netmanager->get(request);
    }();


    // Ничего по сути не хранит, просто QObject с методами, которые ждут
    // события -- завершения запроса в интернет.
    QEventLoop loop;
    QObject::connect(netmanager.get(), &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);
    loop.exec();

    Result result;

    // https://stackoverflow.com/a/35853741
    QVariant status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (status_code.isValid()) {
        result.success = status_code.toString() == "200";
    } else {
        result.success = false;
    }

    result.content = reply->readAll();

    return result;
}
