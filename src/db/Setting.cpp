#include "Setting.h"

namespace db {

void Setting::makeItSilent() {
    multiNetVerbose = VerboseLevelT::LOW;
    dbVerbose = VerboseLevelT::LOW;
}

Setting setting;
}