#include <stdexcept>

#include "mutex.h"

namespace focus{

Semaphore::Semaphore(uint32_t count){
    if(sem_init(&semaphore_,0,count)){
        throw std::logic_error("sem_t init error");
    }
}

Semaphore::~Semaphore(){
    sem_destroy(&semaphore_);
}

void Semaphore::wait(){
    if(sem_wait(&semaphore_)){
        throw std::logic_error("sem_t wait error");
    }
}

void Semaphore::notify(){
    if(sem_post(&semaphore_)){
        throw std::logic_error("sem_t post error");
    }
}

}