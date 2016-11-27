/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebAssemblyFunction.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "JSCInlines.h"
#include "JSFunctionInlines.h"
#include "JSObject.h"
#include "JSWebAssemblyInstance.h"
#include "LLintThunks.h"
#include "ProtoCallFrame.h"
#include "VM.h"
#include "WasmFormat.h"
#include "WebAssemblyFunctionCell.h"

namespace JSC {

const ClassInfo WebAssemblyFunction::s_info = { "WebAssemblyFunction", &Base::s_info, nullptr, CREATE_METHOD_TABLE(WebAssemblyFunction) };

WebAssemblyFunction::WebAssemblyFunction(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    : Base(vm, globalObject, structure)
{
}

void WebAssemblyFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    WebAssemblyFunction* thisObject = jsCast<WebAssemblyFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_instance);
    visitor.append(&thisObject->m_functionCell);
}

void WebAssemblyFunction::finishCreation(VM& vm, NativeExecutable* executable, int length, const String& name, WebAssemblyFunctionCell* functionCell)
{
    Base::finishCreation(vm, executable, length, name);
    ASSERT(inherits(info()));
    m_functionCell.set(vm, this, functionCell);
}

static EncodedJSValue JSC_HOST_CALL callWebAssemblyFunction(ExecState* state)
{
    auto& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    const WebAssemblyFunction* callee = jsDynamicCast<WebAssemblyFunction*>(state->callee());
    if (!callee)
        return JSValue::encode(throwException(state, scope, createTypeError(state, "expected a WebAssembly function", defaultSourceAppender, runtimeTypeForValue(state->callee()))));
    const CallableWebAssemblyFunction& callable = callee->webAssemblyFunctionCell()->function();
    const B3::Compilation* jsEntryPoint = callable.jsEntryPoint;
    const Wasm::Signature* signature = callable.signature;

    // FIXME is this the right behavior? https://bugs.webkit.org/show_bug.cgi?id=164876
    if (state->argumentCount() != signature->arguments.size())
        return JSValue::encode(throwException(state, scope, createNotEnoughArgumentsError(state, defaultSourceAppender)));

    // FIXME is this boxing correct? https://bugs.webkit.org/show_bug.cgi?id=164876
    Vector<JSValue> boxedArgs;
    for (unsigned argIndex = 0; argIndex < state->argumentCount(); ++argIndex) {
        JSValue arg = state->uncheckedArgument(argIndex);
        switch (signature->arguments[argIndex]) {
        case Wasm::Void:
        case Wasm::I64:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case Wasm::I32:
            arg = JSValue::decode(arg.toInt32(state));
            break;
        case Wasm::F32:
            arg = JSValue::decode(bitwise_cast<uint32_t>(arg.toFloat(state)));
            break;
        case Wasm::F64:
            arg = JSValue::decode(bitwise_cast<uint64_t>(arg.toNumber(state)));
            break;
        }
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        boxedArgs.append(arg);
    }

    JSValue firstArgument;
    int argCount = 1;
    JSValue* remainingArgs = nullptr;
    if (boxedArgs.size()) {
        remainingArgs = boxedArgs.data();
        firstArgument = *remainingArgs;
        remainingArgs++;
        argCount = boxedArgs.size();
    }

    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(nullptr, nullptr, firstArgument, argCount, remainingArgs);
    
    EncodedJSValue rawResult = vmEntryToWasm(jsEntryPoint->code().executableAddress(), &vm, &protoCallFrame);
    // FIXME is this correct? https://bugs.webkit.org/show_bug.cgi?id=164876
    switch (signature->returnType) {
    case Wasm::Void:
        return JSValue::encode(jsUndefined());
    case Wasm::I64:
        RELEASE_ASSERT_NOT_REACHED();
    case Wasm::I32:
        return JSValue::encode(JSValue(static_cast<int32_t>(rawResult)));
    case Wasm::F32:
        return JSValue::encode(JSValue(bitwise_cast<float>(static_cast<int32_t>(rawResult))));
    case Wasm::F64:
        return JSValue::encode(JSValue(bitwise_cast<double>(rawResult)));
    }

    RELEASE_ASSERT_NOT_REACHED();
}

WebAssemblyFunction* WebAssemblyFunction::create(VM& vm, JSGlobalObject* globalObject, int length, const String& name, CallableWebAssemblyFunction&& callable)
{
    NativeExecutable* executable = vm.getHostFunction(callWebAssemblyFunction, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    WebAssemblyFunctionCell* functionCell = WebAssemblyFunctionCell::create(vm, WTFMove(callable));
    Structure* structure = globalObject->webAssemblyFunctionStructure();
    WebAssemblyFunction* function = new (NotNull, allocateCell<WebAssemblyFunction>(vm.heap)) WebAssemblyFunction(vm, globalObject, structure);
    function->finishCreation(vm, executable, length, name, functionCell);
    return function;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
