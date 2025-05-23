#include "env.h"
#include "config.h"
#include "log.h"
#include <iostream>
#include <iomanip>
#include <string>

namespace focus {

// 全局日志器
static Logger::ptr g_logger = FOCUS_LOG_NAME("system");

bool Env::init(int argc,char** argv) {
    char link[1024] = {0};
    char path[1024] = {0};
    sprintf(link, "/proc/%d/exe", getpid());
    readlink(link, path, sizeof(path));
    // /path/xxx/exe
    m_exe = path;

    auto pos = m_exe.find_last_of("/");
    m_cwd = m_exe.substr(0, pos) + "/";

    m_program = argv[0];

    // -config /path/to/config -file /test
    const char* nowKey = nullptr;
    for(int i = 1; i < argc; ++i) {
        if('-' == argv[i][0]) {
            if(strlen(argv[i]) > 1) {
                if(nowKey) {
                    add(nowKey, "");
                }
                nowKey = argv[i] + 1;
            }else {
                FOCUS_LOG_ERROR(g_logger) << "invalid arg idx = " << i
                                          << " val = " << argv[i];
                return false;
            }
        }else {
            if(nowKey) {
                add(nowKey, argv[i]);
                nowKey = nullptr;
            }else {
                FOCUS_LOG_ERROR(g_logger) << "invalid arg idx = " << i
                                          << " val = " << argv[i];
                return false;
            }
        }
    }
    if(nowKey) {
        add(nowKey, "");
    }
    return true;
}

void Env::add(const std::string& key,const std::string& val) {
    RWMutexType::WriteLock lock(m_mutex);
    m_args[key] = val;
}
    
bool Env::has(const std::string& key) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_args.find(key);
    return m_args.end() != it;
}
    
void Env::del(const std::string& key) {
    RWMutexType::WriteLock lock(m_mutex);
    m_args.erase(key);
}

std::string Env::get(const std::string& key,const std::string& defaultVal) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_args.find(key);
    return m_args.end() != it? it->second: defaultVal;
}

void Env::addHelp(const std::string& key,const std::string& desc) {
    removeHelp(key);
    RWMutexType::WriteLock lock(m_mutex);
    m_helps.emplace_back(key, desc);
}

void Env::removeHelp(const std::string& key) {
    RWMutexType::WriteLock lock(m_mutex);
    for(auto it = m_args.begin(); m_args.end() != it;) {
        if(key == it->first) {
            it = m_args.erase(it);
        }else {
            ++it;
        }
    }
}

void Env::printHelp() {
    RWMutexType::ReadLock lock(m_mutex);
    std::cout << "Usage: " << m_program << " [options] " << std::endl;
    for(auto& help: m_helps) {
        std::cout << std::setw(5) << "-" << help.first << " : " << help.second << std::endl;
    }
}

bool Env::setEnv(const std::string& key,const std::string& val) {
    return !setenv(key.c_str(), val.c_str(), 1);
}
    
std::string Env::getEnv(const std::string& key,const std::string& defaultVal) {
    char* val = getenv(key.c_str());
    if(nullptr == val) {
        return defaultVal;
    }
    return val;
}

std::string Env::getAbsolutePath(const std::string& path) const {
    if(path.empty()) {
        return "/";
    }
    if('/' == path[0]) {
        return path;
    }
    return m_cwd + path;
}

std::string Env::getAbsoluteWorkPath(const std::string& path) const {
    if(path.empty()) {
        return "/";
    }
    if('/' == path[0]) {
        return path;
    }
    static ConfigVar<std::string>::ptr g_server_work_path = Config::LookUp<std::string>("server.work_path");
    return g_server_work_path->getVal() + "/" +path;
}

std::string Env::getConfigPath() {
    return getAbsolutePath(get("c", "conf"));
}

} // end namespace focus