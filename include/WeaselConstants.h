#pragma once

#define WEASEL_CODE_NAME "Fluxing"
#define WEASEL_REG_KEY L"Software\\Fluxing\\Fluxing"
#define RIME_REG_KEY L"Software\\Fluxing"

#define STRINGIZE(x) #x
#define VERSION_STR(x) STRINGIZE(x)
#define FLUXING_VERSION VERSION_STR(VERSION_MAJOR.VERSION_MINOR.VERSION_PATCH)
