#ifndef __FOCUS_LOG_H__
#define __FOCUS_LOG_H__

#include <memory>
#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <stdint.h>
#include <map>
#include <fstream>

#include "singleton.h"

namespace focus{

class Logger;
class LogEveneWrap;

// 日志流式输出 TODO
#define FOCUS_LOG_LEVEL(logger,level) \
    if(level>=logger->getLevel())     \
    LogEventWrap(LogEvent::ptr(new LogEvent(logger,__FILE__,__LINE__,0,0,0,time(0),"TT",level))).getSS()

#define FOCUS_LOG_DEBUG(logger) FOCUS_LOG_LEVEL(logger,LogLevel::DEBUG)
#define FOCUS_LOG_INFO(logger) FOCUS_LOG_LEVEL(logger,LogLevel::INFO)
#define FOCUS_LOG_WARN(logger) FOCUS_LOG_LEVEL(logger,LogLevel::WARN)
#define FOCUS_LOG_ERROR(logger) FOCUS_LOG_LEVEL(logger,LogLevel::ERROR)
#define FOCUS_LOG_FATAL(logger) FOCUS_LOG_LEVEL(logger,LogLevel::FATAL)

// 日志格式化输出 TODO
#define FOCUS_LOG_FMT_LEVEL(logger,level,fmt,...) \
    if(level>=logger->getLevel())                 \
    LogEventWrap(LogEvent::ptr(new LogEvent(logger,__FILE__,__LINE__,0,0,0,time(0),"TT",level))).getEvent()->format(fmt,__VA_ARGS__)

#define FOCUS_LOG_FMT_DEBUG(logger,fmt,...) FOCUS_LOG_FMT_LEVEL(logger,LogLevel::DEBUG,fmt,__VA_ARGS__)
#define FOCUS_LOG_FMT_INFO(logger,fmt,...) FOCUS_LOG_FMT_LEVEL(logger,LogLevel::INFO,fmt,__VA_ARGS__)
#define FOCUS_LOG_FMT_WARN(logger,fmt,...) FOCUS_LOG_FMT_LEVEL(logger,LogLevel::WARN,fmt,__VA_ARGS__)
#define FOCUS_LOG_FMT_ERROR(logger,fmt,...) FOCUS_LOG_FMT_LEVEL(logger,LogLevel::ERROR,fmt,__VA_ARGS__)
#define FOCUS_LOG_FMT_FATAL(logger,fmt,...) FOCUS_LOG_FMT_LEVEL(logger,LogLevel::FATAL,fmt,__VA_ARGS__)

// 获取日志器
#define FOCUS_LOG_ROOT LoggerMgr::GetInstance()->getRoot()
#define FOCUS_LOG_NAME(name) LoggerMgr::GetInstance()->getLogger(name)

// 日志级别
class LogLevel{
public:
    enum Level{
        UNKNOWN=0, //UNKNOW级别
        DEBUG, //DEBUG级别
        INFO, //INFO级别
        WARN, //WARN级别
        ERROR, //ERROR级别
        FATAL //FATAL级别
    };

    // 将日志级别输出为字符串
    static const char* ToString(LogLevel::Level level);

    // 将字符串输出为日志级别
    static LogLevel::Level FromString(const std::string& s);
};

// 日志事件
class LogEvent{
public:
    using ptr=std::shared_ptr<LogEvent>;

    LogEvent(std::shared_ptr<Logger> logger,const char* file,int32_t line,uint32_t elapse,uint32_t threadId,
            uint32_t fiberId,uint64_t time,const std::string& threadName,LogLevel::Level level);

    std::shared_ptr<Logger> getLogger() {return logger_;}

    const char* getFile() const {return file_;}

    int32_t getLine() const {return line_;}
    
    uint32_t getElapse() const {return elapse_;}
    
    uint32_t getThreadId() const {return threadId_;}
    
    uint32_t getFiberId() const {return fiberId_;}
    
    uint64_t getTime() const {return time_;}
    
    std::string getThreadName() const {return threadName_;}

    LogLevel::Level getLevel() const {return level_;}
    
    std::string getContext() const {return ss_.str();}
    
    std::stringstream& getSS() {return ss_;}

    void format(const char* fmt,...);

    void format(const char* fmt,va_list al);

private:
    std::shared_ptr<Logger> logger_; //日志器
    const char* file_=nullptr; //文件名
    int32_t line_=0; //行号
    uint32_t elapse_=0; //启动的毫秒数
    uint32_t threadId_=0; //线程ID
    uint32_t fiberId_=0; //协程ID
    uint64_t time_=0; //时间戳
    std::string threadName_; //线程名
    LogLevel::Level level_; //当前日志级别
    std::stringstream ss_; //日志流
};

// 日志格式化器
class LogFormatter{
public:
    using ptr=std::shared_ptr<LogFormatter>;
    
    /**
     * @brief 构造函数
     * @param[in] pattern 格式模板
     * @details 
     *  %m 消息
     *  %p 日志级别
     *  %r 累计毫秒数
     *  %c 日志名称
     *  %t 线程id
     *  %n 换行
     *  %d 时间
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %F 协程id
     *  %N 线程名称
     *
     *  默认格式 "%d%T%t%T%N%T%F%T%p%T%c%T%f%l%T%m%n"
     */
    LogFormatter(const std::string& pattern="%d%T%t%T%N%T%F%T%p%T%c%T%f%T%l%T%m%n");

    // 利用格式化字符串初始化格式单个类
    void init();

    bool isError() const {return iserror_;}

    std::string format(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event);
    std::ostream& format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event);

    class FormatItem{
    public:
        using ptr=std::shared_ptr<FormatItem>;

        virtual void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event)=0;
    };

private:
    std::string pattern_; //格式化字符串
    std::vector<FormatItem::ptr> items_; //多个单个操作输出流
    bool iserror_=false; //是否解析错误
};

// 日志输出器
class LogAppender{
public:
    using ptr=std::shared_ptr<LogAppender>;

    virtual ~LogAppender(){}

    void setLogFormatter(LogFormatter::ptr val) {logformatter_=val;}
    void setLevel(LogLevel::Level val) {level_=val;}
    bool isEmpty() {return logformatter_==nullptr;}

    virtual void log(std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event)=0;

protected:
    LogLevel::Level level_; //日志级别
    LogFormatter::ptr logformatter_=nullptr; //日志格式器
};

// 日志器
class Logger: public std::enable_shared_from_this<Logger>{
public:
    using ptr=std::shared_ptr<Logger>;

    Logger(const std::string& name="root",
        LogLevel::Level level=LogLevel::DEBUG):
        name_(name),
        level_(level){
            logformatter_.reset(new LogFormatter());   
        }
    ~Logger(){}

    const std::string& getName() const {return name_;}
    LogLevel::Level getLevel() const {return level_;}
    void setLevel(LogLevel::Level val) {level_=val;}
    void setLogFormatter(const std::string& pattern);

    void log(LogLevel::Level level,LogEvent::ptr event);

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppenders();

private:
    std::string name_; //名字
    LogLevel::Level level_; //当前日志器支持的最低日志级别
    std::list<LogAppender::ptr> appenders_; //多处输出地
    LogFormatter::ptr logformatter_; //日志格式器
};

// 日志包装类
class LogEventWrap{
public:
    LogEventWrap(LogEvent::ptr event):event_(event){}
    ~LogEventWrap(){
        event_->getLogger()->log(event_->getLevel(),event_);
    }

    LogEvent::ptr getEvent(){
        return event_;
    }

    std::stringstream& getSS(){
        return event_->getSS();
    }

private:
    LogEvent::ptr event_;
};

// 控制台输出器
class StdOutLogAppender:public LogAppender{
public:
    void log(Logger::ptr logger,LogLevel::Level level,LogEvent::ptr event) override;
};

// 文件输出器
class FileLogAppender: public LogAppender{
public:
    FileLogAppender(const std::string& file);
    ~FileLogAppender();

    bool reopen();

    void log(Logger::ptr logger,LogLevel::Level level,LogEvent::ptr event) override;

private:
    std::string file_;
    std::ofstream ofs;
};
 
// 日志管理器
class LogManager{
public:
    using ptr=std::shared_ptr<LogManager>;

    LogManager();

    void init();

    Logger::ptr getRoot() const {return root_;}
    Logger::ptr getLogger(const std::string& name);

private:
    std::map<std::string,Logger::ptr> loggers_;
    Logger::ptr root_;
};

using LoggerMgr=Singleton<LogManager>;

}

#endif