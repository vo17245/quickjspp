#pragma once
#include <quickjs.h>
#include "closure.h"
#include <string>
#include <vector>
#include <functional>
namespace qjs::detail
{
using ClassMethod=std::function<JSValue(JSContext* ,void* /*cpp this*/ ,int /*argc*/,JSValue* /*argv*/)>;
struct Class
{
    std::vector<std::pair<std::string, ClassMethod>> methods;    // 成员函数
    // 成员属性
    std::vector<std::function<JSValue(JSContext* ,void*)>> getter; 
    std::vector<std::function<void(JSContext*,void*,JSValue)>> setter;
    std::vector<std::string> properties;
    std::function<void*()> constructor;                                     // 构造函数
    std::string className;                                  // 类名
    std::function<void(void*)> destructor;                                      // 析构函数
};
void RegisterClass(Class&& clazz);
void InitJsClassClass(JSRuntime* rt);
void EnableCreator(JSContext* ctx);
} // namespace qjs::detail