#ifndef EVENTLISTITEM_H
#define EVENTLISTITEM_H

#include <QListWidgetItem>

#include "DB/event.h"

class EventListItem : public QListWidgetItem {
public:
    EventListItem(const Event& event, QListWidget* parent)
        : QListWidgetItem(event.name, parent), event_id(event.get_id()) {}

    virtual ~EventListItem() = default;
public:
    quint64 event_id;

};

#endif // EVENTLISTITEM_H
