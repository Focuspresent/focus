#ifndef __FOCUS_ENV_H__
#define __FOCUS_ENV_H__

#include <map>
#include <vector>
#include <string>

#include "mutex.h"
#include "singleton.h"

namespace focus{

class Env {
public:
    using RWMutexType = RWMutex;

    /**
     * @brief 初始化
     * @param[in] argc 命令行的个数
     * @param[in] argv 命令行参数
     * @example -config /path/xx -file xxx
     */
    bool init(int argc,char** argv);

    /**
     * @brief 添加参数
     * @param[in] key 参数名
     * @param[in] val 参数值
     */
    void add(const std::string& key,const std::string& val);
    
    /**
     * @brief 查询参数
     * @param[in] key 参数名
     */
    bool has(const std::string& key);

    /**
     * @brief 删除参数
     * @param[in] key 参数名
     */
    void del(const std::string& key);
    
    /**
     * @brief 获取参数值
     * @param[in] key 参数
     * @param[in] defauleVal 默认值
     * @return 参数值
     */
    std::string get(const std::string& key,const std::string& defaultVal="");

    /**
     * @brief 获取帮助
     */
    void addHelp(const std::string& key,const std::string& desc);
    
    /**
     * @brief 删除帮助
     */
    void removeHelp(const std::string& key);
    
    /**
     * @brief 打印帮助
     */
    void printHelp();

    /**
     * @brief 获取程序运行路径
     */
    const std::string& getExe() const {
        return m_exe;
    }
    
    /**
     * @brief 获取程序运行的路径
     */
    const std::string& getCwd() const {
        return m_cwd;
    }

    /**
     * @brief 设置环境变量
     */
    bool setEnv(const std::string& key,const std::string& val);
    
    /**
     * @brief 获取环境变量
     */
    std::string getEnv(const std::string& key,const std::string& defaultVal="");
    
    /**
     * @brief 获取绝对路径
     */
    std::string getAbsolutePath(const std::string& path) const;
    
    /**
     * @brief 获取绝对工作路径
     */
    std::string getAbsoluteWorkPath(const std::string& path) const;
    
    /**
     * @brief 获取配置文件路径
     */
    std::string getConfigPath();

private:
    RWMutexType m_mutex; // 锁
    std::map<std::string,std::string> m_args; // 参数集合
    std::vector<std::pair<std::string,std::string>> m_helps; // 帮助集合

    std::string m_program; // 程序名
    std::string m_exe; // 程序运行的完全路径
    std::string m_cwd; // 程序运行的路径位置(去掉程序名)
};

using EnvMgr=Singleton<Env>;

}

#endif