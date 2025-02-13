#include "env.h"
#include "log.h"

using namespace focus;

static Logger::ptr g_logger = FOCUS_LOG_NAME("test_env");

int main(int argc, char* argv[]) {
    EnvMgr::GetInstance()->init(argc, argv);
    FOCUS_LOG_DEBUG(g_logger) << EnvMgr::GetInstance()->get("config", "/test");
    FOCUS_LOG_DEBUG(g_logger) << EnvMgr::GetInstance()->get("file", "/test");
    FOCUS_LOG_DEBUG(g_logger) << EnvMgr::GetInstance()->getAbsolutePath("test");
    return 0;
}