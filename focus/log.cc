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
    LoggerNameFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getLogger()->getName();
    }
private:
    std::string m_string;
};

// 文件名
class FileNameFormatItem: public LogFormatter::FormatItem{
public:
    FileNameFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getFile();
    }
private:
    std::string m_string;
};

// 文件行
class LineFormatItem: public LogFormatter::FormatItem{
public:
    LineFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getLine();
    }
private:
    std::string m_string;
};

// 启动
class ElapseFormatItem: public LogFormatter::FormatItem{
public:
    ElapseFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getElapse();
    }
private:
    std::string m_string;
};

// 线程ID
class ThreadIdFormatItem: public LogFormatter::FormatItem{
public:
    ThreadIdFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getThreadId();
    }
private:
    std::string m_string;
};

// 协程ID
class FiberIDFormatItem: public LogFormatter::FormatItem{
public:
    FiberIDFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getFiberId();
    }
private:
    std::string m_string;
};

// 时间
class DateTimeFormatItem: public LogFormatter::FormatItem{
public:
    DateTimeFormatItem(const std::string& pattern="%Y-%m-%d %H:%M:%S"):
        pattern_(pattern)
    {
        if(pattern_.empty()) {
            pattern_ = "%Y-%m-%d %H:%M:%S";
        }
    }

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
    ThreadNameFormatItem(const std::string& string):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getThreadName();
    }
private:
    std::string m_string;
};

// 消息
class MessageFormatItem: public LogFormatter::FormatItem{
public:
    MessageFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<event->getContext();
    }
private:
    std::string m_string;
};

// 日志级别
class LogLevelFormatItem: public LogFormatter::FormatItem{
public:
    LogLevelFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<LogLevel::ToString(event->getLevel());
    }
private:
    std::string m_string;
};

// \t
class TabFormatItem: public LogFormatter::FormatItem{
public:
    TabFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<"\t";
    }
private:
    std::string m_string;
};

// \n
class NewLineFormatItem: public LogFormatter::FormatItem{
public:
    NewLineFormatItem(const std::string& string = ""):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override{
        os<<std::endl;
    }
private:
    std::string m_string;
};

// 原始字符串
class StrignFormatItem: public LogFormatter::FormatItem {
public:
    StrignFormatItem(const std::string& string):
        m_string(string) {
    }

    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << m_string;
    }

private:
    std::string m_string;
};

// %xxx %xxx{xxx} %%
void LogFormatter::init(){  
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr;
    for(size_t i = 0; i < pattern_.size(); ++i) {
        // 普通字符
        if('%' != pattern_[i]) {
            nstr.append(1, pattern_[i]);
            continue;
        }

        // %%
        if((i + 1) < pattern_.size()) {
            if('%' == pattern_[i + 1]) {
                nstr.append(1, '%');
                continue;
            }
        }

        // %开头
        size_t j = i + 1;
        int fmtStatus = 0;
        size_t fmtBegin = 0;
        std::string str;
        std::string fmt;

        while(j < pattern_.size()) {
            // %xxx
            if(!fmtStatus && (!isalpha(pattern_[j])) && '{' != pattern_[j] && '}' != pattern_[j]) {
                str = pattern_.substr(i + 1, j - i - 1);
                break;
            }
            if(0 == fmtStatus) {
                // 找到{
                if('{' == pattern_[j]) {
                    str = pattern_.substr(i + 1, j - i - 1);
                    fmtStatus = 1;
                    fmtBegin = j;
                    ++j;
                    continue;
                }
            }else if(1 == fmtStatus) {
                // 找到}
                if(pattern_[j] == '}') {
                    fmt = pattern_.substr(fmtBegin + 1, j - fmtBegin - 1);
                    fmtStatus = 0;
                    ++j;
                    break;
                }
            }
            ++j;
            if(j == pattern_.size()) {
                if(str.empty()) {
                    str = pattern_.substr(i + 1);
                }
            }
        }

        if(0 == fmtStatus) {
            if(!nstr.empty()) {
                vec.emplace_back(nstr, "", 0);
                nstr.clear();
            }
            vec.emplace_back(str, fmt, 1);
            i = j - 1;
        }else if(1 == fmtStatus) {
            vec.emplace_back("<<parser-error>>", fmt, 1);
        }
    }
    if(!nstr.empty()) {
        vec.emplace_back(nstr, "", 0);
    }

    // 对应的生成器
    static std::map<std::string, std::function<LogFormatter::FormatItem::ptr(const std::string& str)>> s_strToFunc = {
#define XX(str, type) \
    {#str, [](const std::string& fmt) {return LogFormatter::FormatItem::ptr(new type(fmt));}}
        XX(m, MessageFormatItem),
        XX(p, LogLevelFormatItem),
        XX(r, ElapseFormatItem),
        XX(c, LoggerNameFormatItem),
        XX(t, ThreadIdFormatItem),
        XX(n, NewLineFormatItem),
        XX(d, DateTimeFormatItem),
        XX(f, FileNameFormatItem),
        XX(l, LineFormatItem),
        XX(T, TabFormatItem),
        XX(F, FiberIDFormatItem),
        XX(N, ThreadNameFormatItem),
#undef XX
    };

    for(auto& t: vec) {
        if(0 == std::get<2>(t)) {
            items_.emplace_back(new StrignFormatItem(std::get<0>(t)));
        }else { 
            auto it = s_strToFunc.find(std::get<0>(t));
            if(s_strToFunc.end() == it) {
                items_.emplace_back(new StrignFormatItem("<<error_format %" + std::get<0>(t) + ">>"));
                iserror_ = true;
            }else {
                items_.emplace_back(it->second(std::get<1>(t)));
            }
        }   
    }
}

LogManager::LogManager(){
    init();
}

void LogManager::init(){
    root_.reset(new Logger());
    root_->addAppender(LogAppender::ptr(new StdOutLogAppender()));
    // TODO 绝对路径
    root_->addAppender(LogAppender::ptr(new FileLogAppender("/home/zdc/Code/Git/focus/logs/log.txt")));
}

Logger::ptr LogManager::getLogger(const std::string& name){
    // TODO
    auto it=loggers_.find(name);
    if(it!=loggers_.end()){
        return it->second;
    }

    Logger::ptr logger(new Logger(name));
    // 添加测试输出
    logger->addAppender(LogAppender::ptr(new StdOutLogAppender()));
    loggers_[name]=logger;
    return logger;
}   

}