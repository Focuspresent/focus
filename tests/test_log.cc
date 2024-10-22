#include "log.h"
using namespace focus;

int main(){
    Logger::ptr logger=LoggerMgr::GetInstance()->getRoot();
    FOCUS_LOG_LEVEL(logger,LogLevel::DEBUG)<<"Test log";
    FOCUS_LOG_FMT_LEVEL(logger,LogLevel::DEBUG,"%s %d","Test log",3);
    return 0;
}