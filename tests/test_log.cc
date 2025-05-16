#include "log.h"
using namespace focus;

int main(){
    Logger::ptr logger=LoggerMgr::GetInstance()->getRoot();
    FOCUS_LOG_LEVEL(logger,LogLevel::DEBUG)<<"Test log";
    FOCUS_LOG_FMT_LEVEL(logger,LogLevel::DEBUG,"%s %d","Test log",3);
    FOCUS_LOG_FMT_DEBUG(logger, "Test");
    Logger::ptr testLogger = FOCUS_LOG_NAME("test");
    testLogger->setLogFormatter("[%c]%T[%p]%T[%t]%T[%N]%T[%m]%T%n");
    FOCUS_LOG_DEBUG(testLogger) << "Test Modfiy Formatter";
    return 0;
}