#ifndef VK_H
#define VK_H

#include <QString>

namespace vk {
    bool get_user(const QString& vk_profile, QString& first_name, QString& last_name, qint64& vk_id, int& error_code, QString& error_msg);
    bool send_message(qint64 vk_id, const QString& content, qint64 uniqueness_id, int& error_code, QString& error_msg);
}

#endif // VK_H
