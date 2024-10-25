#include "log.h"
#include <iostream>
#include <time.h>
#include <unordered_map>
#include <functional>
#include <cstdarg>

namespace focus{

const char* LogLevel::ToString(LogLevel::Level level){
    switch (level){
#define XX(name) \
    case LogLevel::name: \
        return #name; \
        break
    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX
    default:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

LogLevel::Level LogLevel::FromString(const std::string& s){
#define XX(level,v) \
    if(s==#v) \
        return LogLevel::level;
    XX(DEBUG,debug);
    XX(INFO,info);
    XX(WARN,warn);
    XX(ERROR,error);
    XX(FATAL,fatal);

    XX(DEBUG,DEBUG);
    XX(INFO,INFO);
    XX(WARN,WARN);
    XX(ERROR,ERROR);
    XX(FATAL,FATAL);
#undef XX
    return LogLevel::UNKNOWN;
}

LogEvent::LogEvent(std::shared_ptr<Logger> logger,const char* file,std::int32_t line,uint32_t elapse,uint32_t threadId,
            uint32_t fiberId,uint64_t time,const std::string& threadName,LogLevel::Level level):
            logger_(logger),file_(file),line_(line),elapse_(elapse),threadId_(threadId),
            fiberId_(fiberId),time_(time),threadName_(threadName),level_(level)
            {}

void LogEvent::format(const char* fmt,...){
    va_list al;
    va_start(al,fmt);
    format(fmt,al);
    va_end(al);
}

void LogEvent::format(const char* fmt,va_list al){
    char* buf=nullptr;
    int len=vasprintf(&buf,fmt,al);
    if(len!=-1){
        ss_<<std::string(buf,len);
        free(buf);
    }
}


void Logger::log(LogLevel::Level level,LogEvent::ptr event){
    if(level_>level) return ;
    for(auto& it: appenders_){
        auto self=shared_from_this();
        if(it->isEmpty()) it->setLogFormatter(logformatter_);
        it->log(self,level_,event);
    }
}

void Logger::setLogFormatter(const std::string& pattern){
    logformatter_.reset(new LogFormatter(pattern));
    for(auto& it: appenders_){
        it->setLogFormatter(logformatter_);
    }
}


void Logger::debug(LogEvent::ptr event){
    log(LogLevel::DEBUG,event);
}

void Logger::info(LogEvent::ptr event){
    log(LogLevel::INFO,event);
}

void Logger::warn(LogEvent::ptr event){
    log(LogLevel::WARN,event);
}

void Logger::error(LogEvent::ptr event){
    log(LogLevel::ERROR,event);
}

void Logger::fatal(LogEvent::ptr event){
    log(LogLevel::FATAL,event);
}

void Logger::addAppender(LogAppender::ptr appender){
    appenders_.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender){
    for(auto it=appenders_.begin();it!=appenders_.end();++it){
        if(*it==appender){
            appenders_.erase(it);
            break;
        }
    }
}   

void Logger::clearAppenders(){
    appenders_.clear();
}

LogFormatter::LogFormatter(const std::string& pattern):pattern_(pattern){
    init();
}


std::string LogFormatter::format(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event){
    std::stringstream ss;
    for(auto& it: items_){
        it->format(ss,logger,level,event);
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event){
    for(auto& it: items_){
        it->format(os,logger,level,event);
    }
    return os;
}

void StdOutLogAppender::log(Logger::ptr logger,LogLevel::Level level,LogEvent::ptr event){
    if(level_>level) return ;
    logformatter_->format(std::cout,logger,level,event);
}

FileLogAppender::FileLogAppender(const std::string& file):file_(file){
    reopen();
}

FileLogAppender::~FileLogAppender(){
    ofs.close();
}

bool FileLogAppender::reopen(){
    if(ofs.is_open()){
        return true;
    }
    ofs.open(file_,std::ios::app);
    if(ofs.fail()){
        std::cerr<<"file open fail"<<std::endl;
        return false;
    }
    return true;
}

void FileLogAppender::log(Logger::ptr logger,LogLevel::Level level,LogEvent::ptr event){
    if(level_>level||!reopen()) return ;
    logformatter_->format(ofs,logger,level,event);
}

// 日志器名字
class LoggerNameFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getLogger()->getName();
    }
};

// 文件名
class FileNameFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getFile();
    }
};

// 文件行
class LineFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getLine();
    }
};

// 启动
class ElapseFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getElapse();
    }
};

// 线程ID
class ThreadIdFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getThreadId();
    }
};

// 协程ID
class FiberIDFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getFiberId();
    }
};

// 时间
class DateTimeFormatItem: public LogFormatter::FormatItem{
public:
    DateTimeFormatItem(const std::string& pattern="%Y-%m-%d %H:%M:%S"):
        pattern_(pattern)
    {}

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        struct tm tm;
        time_t time=event->getTime();
        localtime_r(&time,&tm);
        char buf[64];
        strftime(buf,sizeof(buf),pattern_.c_str(),&tm);
        os<<buf;
    }
private:
    std::string pattern_;
};

// 线程名
class ThreadNameFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getThreadName();
    }
};

// 消息
class MessageFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getContext();
    }
};

// 日志级别
class LogLevelFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<LogLevel::ToString(event->getLevel());
    }
};

// \t
class TabFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<"\t";
    }
};

// \n
class NewLineFormatItem: public LogFormatter::FormatItem{
public:
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<std::endl;
    }
};

void LogFormatter::init(){  
    // 分割
    std::vector<std::string> vec;
    for(int i=0;i<pattern_.size();){
        if(pattern_[i]=='%'){
            i++;
            int st=i;
            while(i<pattern_.size()&&isalnum(pattern_[i])) i++;
            vec.emplace_back(pattern_.substr(st,i-st));
        }else i++;
    }
    // 生成Item的fun簇
    std::unordered_map<std::string,std::function<LogFormatter::FormatItem::ptr()>> str2func={
#define XX(key,val) \
        {#key,[]()->LogFormatter::FormatItem::ptr{return LogFormatter::FormatItem::ptr(new val);}}
        XX(m,MessageFormatItem),
        XX(p,LogLevelFormatItem),
        XX(r,ElapseFormatItem),
        XX(t,ThreadIdFormatItem),
        XX(n,NewLineFormatItem),
        XX(d,DateTimeFormatItem),
        XX(f,FileNameFormatItem),
        XX(l,LineFormatItem),
        XX(T,TabFormatItem),
        XX(F,FiberIDFormatItem),
        XX(N,ThreadNameFormatItem),
        XX(c,LoggerNameFormatItem),
#undef XX
    };
    // 按顺序填充items
    for(auto& s: vec){
        auto it=str2func.find(s);
        if(it==str2func.end()){
            iserror_=true;
            break;
        }
        items_.emplace_back(it->second());
    }
}

LogManager::LogManager(){
    init();
}

void LogManager::init(){
    root_.reset(new Logger());
    root_->addAppender(LogAppender::ptr(new StdOutLogAppender()));
    root_->addAppender(LogAppender::ptr(new FileLogAppender("/home/zdc/Code/Git/focus/logs/log.txt")));
}

Logger::ptr LogManager::getLogger(const std::string& name){
    // TODO
    auto it=loggers_.find(name);
    if(it!=loggers_.end()){
        return it->second;
    }

    Logger::ptr logger(new Logger(name));
    loggers_[name]=logger;
    return logger;
}   

}