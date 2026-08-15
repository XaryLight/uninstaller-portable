#include <language.hpp>

const QStringView& getlang(uint id, uint type) {
    if (type == 0xffffffffu) type = G.LANGUAGE;
	if (id < LANGSIZE) {
        return LANG_MAP[id].string[type];
    }
    return NONETEXT;
}