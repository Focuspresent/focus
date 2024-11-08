#ifndef __FOCUS_CONFIG_H__
#define __FOCUS_CONFIG_H__

#include <string>
#include <memory>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <functional>
#include <map>
#include <stdint.h>
#include <exception>

#include "mutex.h"
#include "log.h"
#include "util.h"

namespace focus{

// 配置变量基类
class ConfigVarBase{
public:
    using ptr=std::shared_ptr<ConfigVarBase>;

    // 构造
    ConfigVarBase(const std::string& name,const std::string& description="")
        :name_(name),
        description_(description) {
        std::transform(name_.begin(),name_.end(),name_.begin(),::tolower);
    }

    // 虚析构
    virtual ~ConfigVarBase() {}

    // 读取配置变量名称
    const std::string& getName() const {return name_;}

    // 读取配置变量描述
    const std::string& getDescription() const {return description_;}

    // 转成字符串
    virtual std::string toString()=0;

    // 从字符串中读取
    virtual bool fromString(const std::string& val)=0;

    // 返回配置变量的类型
    virtual std::string getTypeName() const =0;

protected:
    std::string name_; //名称
    std::string description_; // 描述
};

/**
 * @brief 转换模版类(F to T)
 */
template <class F,class T>
class LexicalCast{
public:
    /**
     * @brief 类型转换
     * @param[in] v 源类型值
     * @return T 目标类型
     * @exception 会出现异常
     */
    T operator()(const F& v) {
        return boost::lexical_cast<T>(v);
    }
};

// 模版特化 TODO

// 配置变量子类
template<class T,class FromStr=LexicalCast<std::string,T>,
                class ToStr=LexicalCast<T,std::string>>
class ConfigVar: public ConfigVarBase{
public:
    using RWMutexType=RWMutex;
    using ptr=std::shared_ptr<ConfigVar>;
    using onchangecb=std::function<void(const T& oldvalue,const T& newvalue)>;

    ConfigVar(const std::string& name,const T& val,
            const std::string& description)
            :ConfigVarBase(name,description),
            val_(val) {
    }

    // 转成字符串
    std::string toString() override {
        try{
            RWMutexType::ReadLock lock(mutex_);
            return ToStr()(val_);
        }catch(std::exception& e){
            FOCUS_LOG_ERROR(FOCUS_LOG_ROOT())<<"ConfigVar:toString excption "
                <<e.what()<<"convert:"<<TypeToName<T>()<<" to string"
                <<" name="<<name_;
        }
        return "";
    }

    // 从字符串中读取
    bool fromString(const std::string& val) override {
        try{
            setVal(FromStr()(val));
            return true;
        }catch(std::exception& e){
            FOCUS_LOG_ERROR(FOCUS_LOG_ROOT())<<"ConfigVar:fromString excption"
                <<e.what()<<" convert: string to"<<TypeToName<T>()
                <<" name="<<name_
                <<" - "<<val_;
        }
        return false;
    }

    // 返回配置变量的类型 TODO
    std::string getTypeName() const override {
        return TypeToName<T>();
    }

    // 读取参数值
    const T getVal() {
        RWMutexType::ReadLock lock(mutex_);
        return val_;
    }

    // 更该参数值
    void setVal(const T& v) {
        {
            RWMutexType::ReadLock lock(mutex_);
            if(v==val_){
                return ;
            }
            for(auto& cb: cbs_){
                cb.second(val_,v);
            }
        }
        RWMutexType::WriteLock lock(mutex_);
        val_=v;
    }

    // 添加回调函数，并返回唯一id
    uint64_t addCallBack(onchangecb cb) {
        static uint64_t funid=0;
        RWMutexType::WriteLock lock(mutex_);
        cbs_[funid++]=cb;
        return funid;
    }

    // 获取回调函数
    onchangecb getCallBack(uint64_t key) {
        RWMutexType::ReadLock lock(mutex_);
        auto it=cbs_.find(key);
        return it==cbs_.end()?nullptr:it->second;
    }

    // 删除回调函数
    void delCallBack(uint64_t key) {
        RWMutexType::WriteLock lock(mutex_);
        cbs_.erase(key);
    }

    // 清除回调函数
    void clearCallBack() {
        RWMutexType::WriteLock lock(mutex_);
        cbs_.clear();
    }

private:
    RWMutexType mutex_; //读写锁
    T val_; //变量值
    std::map<uint64_t,onchangecb> cbs_; //修改时触发的回调函数
};

// 配置变量管理类 TODO
class Config{
public:
    using ConfigVarMap=std::map<std::string,ConfigVarBase::ptr>;
    using RWMutexType=RWMutex;

    /**
     * @brief 获取配置变量
     * 
     */
    template<class T>
    static typename ConfigVar<T>::ptr LookUp(const std::string& name,const T& defaultval,const std::string& description="") {
        RWMutexType::WriteLock lock(GetMutex());
        auto it=GetDatas().find(name);
        // 存在
        if(it!=GetDatas().end()){
            // 下转型
            auto temp=std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
            if(temp!=nullptr){
                // 获取成功
                FOCUS_LOG_INFO(FOCUS_LOG_ROOT())<<"LookUp name= "<<name<<" exists";
                return temp;
            }else{
                // 类型不匹配
                FOCUS_LOG_ERROR(FOCUS_LOG_ROOT())<<"LookUp name= "<<name<<" exists but type not"
                        <<TypeToName<T>()<<" real type= "<<it->second->getTypeName()
                        <<" "<<it->second->toString();
                return nullptr;
            }
        }
        // 没有
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name,defaultval,description));
        GetDatas()[name]=v;
        return v;
    }

    // 获取配置变量
    template<class T>
    static typename ConfigVar<T>::ptr LookUp(const std::string& name) {
        auto it=GetDatas().find(name);
        if(it==GetDatas().end()){
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    }
private:
    // 获取键值对
    static ConfigVarMap& GetDatas() {
        static ConfigVarMap s_datas;
        return s_datas;
    }

    // 获取互斥量
    static RWMutexType& GetMutex() {
        static RWMutexType s_mutex;
        return s_mutex;
    }
};

}

#endif