#include <base/std.h>
#include <res/global.h>
#include <base/struct.h>

namespace informat{
    ll getsize(std::string& path);
}

namespace ioapi{
    void frewirte(std::string& include);
    void freread(
        std::set<std::pair<std::string,int64_t>>&,
        std::string& path
    );
}

