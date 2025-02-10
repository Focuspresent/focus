#include "fiber.h"
#include "log.h"
#include <iostream>
#include <functional>
#include <vector>
#include <random>

using namespace focus;
using namespace std;

static Logger::ptr g_logger = FOCUS_LOG_NAME("test");

void func1(int a) {
    for(int i = 0; i < 50; ++i) {
        cout << "a = " << a << " i = " << i << endl;
    }
}

int main(int argc, char* argv[]) {
    Fiber::GetThis();
    function<void()> cb1 = bind(func1, 13456);
    function<void()> cb2 = bind(func1, 12345);
    vector<Fiber::ptr> vecs;
    vecs.emplace_back(new Fiber(cb1, 10 * 1024, false));
    vecs.emplace_back(new Fiber(cb2, 10 * 1024, false));
    srand(time(0));
    while(vecs[0]->getState() != Fiber::TERM || vecs[1]->getState() != Fiber::TERM) {
        int i = rand() % 2;
        vecs[i]->resume();
        vecs[i]->yield();
    }
    return 0;
}