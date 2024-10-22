#include "log.h"
using namespace focus;

int main(){
    Logger::ptr logger(new Logger());
    LogAppender::ptr loggeraddpender(new StdOutLogAppender());
    logger->addAppender(loggeraddpender);
    logger->setLevel(LogLevel::DEBUG);
    FOCUS_LOG_LEVEL(logger,LogLevel::DEBUG)<<"Test log";
    FOCUS_LOG_FMT_LEVEL(logger,LogLevel::DEBUG,"%s %d","Test log",3);
    return 0;
}