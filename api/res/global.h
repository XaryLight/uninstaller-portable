//
// Created by xubow on 2026/7/20.
//

#ifndef UNINSTALLER_GLABE_H
#define UNINSTALLER_GLABE_H
#include "constexpr.h"
#include <base/std.h>
#include <base/qt.h>

inline struct global {
    short LANGUAGE = 1;// The language option. 0: us-en, 1: zh-cn
    std::array<short, 2> WINDOWS_SIZE{1200, 600};// Windows size
    QTranslator * TRANSLATOR;
} G;


#endif //UNINSTALLER_GLABE_H
