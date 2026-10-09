#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    int count = 0;
    while (true) {
        printf("ludo001: hello #%d\n", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
