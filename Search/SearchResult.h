#ifndef SEARCHRESULT_H
#define SEARCHRESULT_H

#include <QString>
#include <QVariant>

struct SearchResult {
    QString name;
    QString path;
    QString type; // "app", "calc", "web", "copy"
    QString description;
    QVariant data;
};

#endif // SEARCHRESULT_H