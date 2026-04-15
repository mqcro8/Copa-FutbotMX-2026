#pragma once
#include "freertos/FreeRTOS.h"
#include "task.h"

class TaskFactory {
public:
    template<typename TaskFunc>
    static bool create(const char* name, TaskFunc&& taskFn, uint32_t stackSize = 4096, UBaseType_t priority = 5, BaseType_t core = 0) {
        static_assert(sizeof(TaskFunc) > 0, "Invalid task function");
        
        TaskHandle_t handle = nullptr;
        BaseType_t result = xTaskCreatePinnedToCore(
            [](void* param) {
                auto fn = static_cast<TaskFunc*>(param);
                (*fn)();
                delete fn;
                vTaskDelete(nullptr);
            },
            name,
            stackSize,
            new TaskFunc(std::forward<TaskFunc>(taskFn)),
            priority,
            &handle,
            core
        );
        
        return result == pdPASS;
    }
};