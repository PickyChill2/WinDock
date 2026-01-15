#include "EmojiList.h"

// Включаем все категории
#include "EmojiCategories/EmojiSmileys.h"
#include "EmojiCategories/EmojiHearts.h"
#include "EmojiCategories/EmojiAnimals.h"
#include "EmojiCategories/EmojiFish.h"
#include "EmojiCategories/EmojiFood.h"
#include "EmojiCategories/EmojiObjects.h"
#include "EmojiCategories/EmojiSports.h"
#include "EmojiCategories/EmojiTransport.h"

const QList<EmojiData>& getEmojiList() {
    static const QList<EmojiData> emojis = []() {
        QList<EmojiData> all;
        all.append(emojiSmileys);
        all.append(emojiHearts);
        all.append(emojiAnimals);
        all.append(emojiFood);
        all.append(emojiSports);
        all.append(emojiFish);
        all.append(emojiTransport);
        all.append(emojiObjects);
        return all;
    }();
    return emojis;
}
