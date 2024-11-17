#include <list>

#include "config.h"

namespace focus {

static Logger::ptr g_logger=FOCUS_LOG_NAME("system");

ConfigVarBase::ptr Config::LookUpBase(const std::string& name) {
    RWMutexType::ReadLock lock(GetMutex());
    auto it=GetDatas().find(name);
    return GetDatas().end()==it?nullptr:it->second;
}

// 将树状yaml扁平化
static void ListAllMembers(const std::string& prefix,
                           const YAML::Node& node,
                           std::list<std::pair<std::string,const YAML::Node>>& output) {
    if(prefix.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._0123456789")
        !=std::string::npos){
        FOCUS_LOG_ERROR(g_logger)<<"Config invalid name: "<<prefix<<" : "<<node;
        return ;
    }
    output.emplace_back(prefix,node);
    if(node.IsMap()){
        for(auto it=node.begin();it!=node.end();++it){
            ListAllMembers(prefix.empty()?it->first.Scalar():prefix+"."+it->first.Scalar(),it->second,output);
        }
    }
}

void Config::LoadFromYaml(const YAML::Node& root) {
    std::list<std::pair<std::string,const YAML::Node>> nodes;
    ListAllMembers("",root,nodes);

    for(auto& node: nodes){
        std::string key=node.first;
        if(key.empty()){
            continue;
        }

        std::transform(key.begin(),key.end(),key.begin(),::tolower);
        ConfigVarBase::ptr var=LookUpBase(key);
        
        if(nullptr!=var){
            if(node.second.IsScalar()){
                var->fromString(node.second.Scalar());
            }else{
                std::stringstream ss;
                ss<<node.second;
                var->fromString(ss.str());
            }
        }
    }
}

}