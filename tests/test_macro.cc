#include "macro.h"
using namespace focus;

static void fun(int i) {
    FOCUS_ASSERT(i == 2);
}

int main(int argc, char* argv[]) {
    fun(2);
    fun(3);
    return 0;
}