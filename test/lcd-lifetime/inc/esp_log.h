#pragma once
#include <stdio.h>
#define ESP_LOGI(t, ...) do{}while(0)
#define ESP_LOGW(t, ...) do{}while(0)
#define ESP_LOGE(t, ...) do{fprintf(stderr,__VA_ARGS__);fputc(10,stderr);}while(0)
#define ESP_LOGD(t, ...) do{}while(0)
