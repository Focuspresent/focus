#include "scheduler.h"
#include <iostream>
#include <vector>
#include <functional>

using namespace focus;

static Logger::ptr g_logger = FOCUS_LOG_NAME("test_scheduler");

void fun(int i) {
    FOCUS_LOG_DEBUG(g_logger) << "CurFiberId = " << GetFiberId() << "input param " << i;
}

int main(int argc, char* argv[]) {
    Scheduler::ptr scheduler(new Scheduler(2));
    scheduler->start();
    std::vector<Fiber::ptr> vecWorkFibers;
    for(int i = 0; i < 10; ++i) {
        vecWorkFibers.emplace_back(new Fiber(std::bind(&fun, i)));
    }
    scheduler->schedule(vecWorkFibers.begin(), vecWorkFibers.end());
    sleep(1);
    scheduler->stop();
    FOCUS_LOG_DEBUG(g_logger) << "main end";
    return 0;
}