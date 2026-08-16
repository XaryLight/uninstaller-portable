//
// Created by xubow on 2026/7/20.
//

#ifndef UNINSTALLER_CONSTEXPR_H
#define UNINSTALLER_CONSTEXPR_H

#include <base/std.h>

// Const int exprcoding
constexpr int LANGSIZE{63};             //Language array size (实际条目数，避免越界与常量膨胀).
constexpr int LANGTYPE{2};              //Language type size.
constexpr short SOFTWARETYPE{5};        //Application type size.
constexpr short SIZEUNITESLEN{6};       //Size types.
constexpr short OSERRORTYPES{10};

// Const string exprcoding
constexpr const char * PATH_ICON{""};
constexpr std::array<const char *, SOFTWARETYPE> SWSORTS{
    "Normal",
    "WindowsInstaller",
    "SystemComponent",
    "RunningTime",
    "Unknown"
};// Application type and size.
constexpr std::array<const char *,SIZEUNITESLEN> SIZEUNITS {
    "B", "KB", "MB", "GB", "TB", "PB"
};// File size format units set;
#endif //UNINSTALLER_CONSTEXPR_H
