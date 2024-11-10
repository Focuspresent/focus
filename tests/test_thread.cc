#include <iostream>
#include <vector>

#include "thread.h"

using namespace std;
using namespace focus;

int main(int argc,char** argv) {
    vector<Thread::ptr> thr;
    static int cnt=0;
    for(int i=0;i<5;i++) {
        thr.emplace_back(new Thread([&]()->void{
            cout<<(cnt++)<<endl;
            return ;
        },"test_"+std::to_string(i)));
    }
    for(int i=0;i<thr.size();i++){
        thr[i]->join();
    }
    return 0;
}