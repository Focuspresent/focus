#include <iostream>
#include <functional>

#include "config.h"

using namespace std;
using namespace focus;

int main(){
    std::string str="4";
    ConfigVar<int> var("tt",0,"test config");
    std::function<void(const int& ,const int&)> cb=[&](const int& a,const int& b)->void{
        cout<<a<<" to "<<b<<endl;
    };
    int id=var.addCallBack(cb);
    var.fromString(str);
    cout<<var.getVal()<<endl;
    auto test_var=Config::LookUp<int>("test.var",55,"test var");
    cout<<test_var->toString()<<endl;
    return 0;
}