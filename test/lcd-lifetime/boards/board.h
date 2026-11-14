#pragma once
#include "../displays/images/themes/themes.h"
class Board { public: Theme* theme; const char* model; const char* asic;
  Theme* getTheme(){return theme;} const char* getDeviceModel(){return model;} const char* getAsicModel(){return asic;} };
