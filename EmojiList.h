#ifndef DOCK_EMOJILIST_H
#define DOCK_EMOJILIST_H

#include <QString>
#include <QStringList>
#include <QList>

struct EmojiData {
    QString emoji;
    QStringList keywords;
};

const QList<EmojiData>& getEmojiList();

#endif // DOCK_EMOJILIST_H