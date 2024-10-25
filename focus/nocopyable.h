#ifndef __FOCUS_NOCOPYABLE_H__
#define __FOCUS_NOCOPYABLE_H__

namespace focus{

// 禁止拷贝
class Nocopyable{
public:
    Nocopyable()=default;
    ~Nocopyable()=default;

    Nocopyable(const Nocopyable&)=delete;
    Nocopyable& operator=(const Nocopyable&)=delete;
};

}

#endif