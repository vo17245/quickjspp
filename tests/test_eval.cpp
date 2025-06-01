#include "../quickjs.h"
#include <iostream>
#include <string>
// 1. C 函数实现，参数和返回值都是 JSValue
static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
  for (int i = 0; i < argc; i++) {
    const char *str = JS_ToCString(ctx, argv[i]);
    if (str) {
      printf("%s", str);
      JS_FreeCString(ctx, str);
    }
    if (i != argc - 1)
      printf(" ");
  }
  printf("\n");
  return JS_UNDEFINED;
}
int test1() {
  // 1. 创建 JS 运行时
  JSRuntime *rt = JS_NewRuntime();
  if (!rt) {
    std::cerr << "Failed to create JS runtime\n";
    return 1;
  }

  // 2. 创建 JS 上下文
  JSContext *ctx = JS_NewContext(rt);
  if (!ctx) {
    std::cerr << "Failed to create JS context\n";
    JS_FreeRuntime(rt);
    return 1;
  }

  // 3. 准备要执行的 JS 代码
  const char *js_code = "var x = 10; var y = 20; x + y;";

  // 4. 执行 JS 代码
  JSValue result =
      JS_Eval(ctx, js_code, strlen(js_code), "<input>", JS_EVAL_TYPE_GLOBAL);

  // 5. 检查是否异常
  if (JS_IsException(result)) {
    JSValue exc = JS_GetException(ctx);
    const char *err_str = JS_ToCString(ctx, exc);
    std::cerr << "JavaScript exception: " << (err_str ? err_str : "unknown")
              << "\n";
    JS_FreeValue(ctx, exc);
    JS_FreeValue(ctx, result);
  } else {
    // 6. 读取结果（示例是数字）
    int32_t int_result;
    if (JS_ToInt32(ctx, &int_result, result) == 0) {
      std::cout << "Result: " << int_result << "\n";
    } else {
      std::cout << "Result is not an int\n";
    }
    JS_FreeValue(ctx, result);
  }

  // 7. 释放 JS 上下文和运行时
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);

  return 0;
}

int test2() {
  JSRuntime *rt = JS_NewRuntime();
  JSContext *ctx = JS_NewContext(rt);

  // 2. 创建 JS 函数对象，3 表示最多参数个数（任意写）
  JSValue func = JS_NewCFunction(ctx, js_print, "print", 1);

  // 3. 绑定到全局对象，global.print = func
  JS_SetPropertyStr(ctx, JS_GetGlobalObject(ctx), "print", func);
  // 4. 调用 JS 代码测试
  std::string js_code = "print('Hello, QuickJS!');"
                        "print('This is a test of the print function.');"
                        "print(1, 2, 3);";
  JSValue result = JS_Eval(ctx, js_code.c_str(), js_code.size(), "<input>",
                           JS_EVAL_TYPE_GLOBAL);
  JS_FreeValue(ctx, result);
  std::cout << "begin free context" << std::endl;
  JS_FreeContext(ctx);
  std::cout << "begin free runtime" << std::endl;
  JS_FreeRuntime(rt);
  return 0;
}
int test3() {
  JSRuntime *rt = JS_NewRuntime();
  JSContext *ctx = JS_NewContext(rt);

  JS_RunGC(rt);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  return 0;
}
int main() 
{
   return test3(); }
