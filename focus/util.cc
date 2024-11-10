#include "util.h"

namespace focus {

pid_t GetThreadId() {
    return syscall(SYS_gettid);
}

}