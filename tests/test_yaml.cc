#include <iostream>
#include <yaml-cpp/yaml.h>

using namespace std;

int main(int argc, char** argv) {
    YAML::Node node=YAML::LoadFile("/home/zdc/Code/Git/focus/tests/test.yaml");
    if(!node["server"].IsNull()){
        cout<<node["server"]["ip"].as<std::string>()<<endl;
        cout<<node["server"]["port"].as<int>()<<endl;
    }
    return 0;
}