#pragma once
#include <functional>
#include <quickjs.h>
namespace qjs::detail
{

using Closure = std::function<JSValue(JSContext*,
                                      JSValueConst /*this*/,
                                      int /*argc*/,
                                      JSValueConst* /*argv*/)>;
void InitJsClosureClass(JSRuntime* rt);

/**
 * @note closure must be allocated with new
 *       closure will be deleted when JS GC
*/
JSValue CreateClosure(JSContext* ctx, Closure* closure);
} // namespace qjs::detail