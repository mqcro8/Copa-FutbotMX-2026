#pragma once
#include <cstdint>
// After installing the AsyncTCP library, replace the include below with the correct header name.
// Common names: AsyncTCP.h, AsyncTCP_S3.h, ESPAsyncTCP.h
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

enum class WebServerState : uint8_t {
    STOPPED,
    STARTING,
    RUNNING,
    ERROR
};

struct WebServerConfig {
    uint16_t port;
    const char* www_dir;
    const char* index_file;
    bool enable_cors;
    uint32_t request_timeout_ms;
};

constexpr WebServerConfig DEFAULT_WEB_CONFIG = {
    .port = 80,
    .www_dir = "/www",
    .index_file = "/index.html",
    .enable_cors = true,
    .request_timeout_ms = 5000
};

namespace WebSrv {
    void init(const WebServerConfig& config = DEFAULT_WEB_CONFIG);
    void update();
    void stop();
    WebServerState state();
    uint32_t requestCount();
    uint32_t uptime();
}