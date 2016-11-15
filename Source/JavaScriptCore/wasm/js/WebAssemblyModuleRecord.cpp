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
#include "WebAssemblyModuleRecord.h"

#if ENABLE(WEBASSEMBLY)

#include "Error.h"
#include "JSCInlines.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "JSWebAssemblyModule.h"
#include "WasmFormat.h"
#include "WebAssemblyFunction.h"

namespace JSC {

const ClassInfo WebAssemblyModuleRecord::s_info = { "WebAssemblyModuleRecord", &Base::s_info, 0, CREATE_METHOD_TABLE(WebAssemblyModuleRecord) };

Structure* WebAssemblyModuleRecord::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

WebAssemblyModuleRecord* WebAssemblyModuleRecord::create(ExecState* exec, VM& vm, Structure* structure, JSWebAssemblyModule* module, const Identifier& moduleKey, const SourceCode& sourceCode, const VariableEnvironment& declaredVariables, const VariableEnvironment& lexicalVariables)
{
    WebAssemblyModuleRecord* instance = new (NotNull, allocateCell<WebAssemblyModuleRecord>(vm.heap)) WebAssemblyModuleRecord(vm, structure, moduleKey, sourceCode, declaredVariables, lexicalVariables);
    instance->finishCreation(exec, vm, module);
    return instance;
}

WebAssemblyModuleRecord::WebAssemblyModuleRecord(VM& vm, Structure* structure, const Identifier& moduleKey, const SourceCode& sourceCode, const VariableEnvironment& declaredVariables, const VariableEnvironment& lexicalVariables)
    : Base(vm, structure, moduleKey, sourceCode, declaredVariables, lexicalVariables)
{
}

void WebAssemblyModuleRecord::destroy(JSCell* cell)
{
    WebAssemblyModuleRecord* thisObject = jsCast<WebAssemblyModuleRecord*>(cell);
    thisObject->WebAssemblyModuleRecord::~WebAssemblyModuleRecord();
}

void WebAssemblyModuleRecord::finishCreation(ExecState* exec, VM& vm, JSWebAssemblyModule* module)
{
    Base::finishCreation(exec, vm);
    ASSERT(inherits(info()));
    m_module.set(vm, this, module);
}

void WebAssemblyModuleRecord::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    WebAssemblyModuleRecord* thisObject = jsCast<WebAssemblyModuleRecord*>(cell);
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_module);
}

void WebAssemblyModuleRecord::link(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(scope);
    auto* globalObject = state->lexicalGlobalObject();

    const Wasm::ModuleInformation& moduleInformation = m_module->moduleInformation();
    const Wasm::CompiledFunctions& compiledFunctions = m_module->compiledFunctions();
    SymbolTable* exportSymbolTable = m_module->exportSymbolTable();

    // FIXME wire up the imports.

    // Let exports be a list of (string, JS value) pairs that is mapped from each external value e in instance.exports as follows:
    JSModuleEnvironment* moduleEnvironment = JSModuleEnvironment::create(vm, globalObject, nullptr, exportSymbolTable, JSValue(), this);
    unsigned offset = 0;
    for (const auto& e : moduleInformation.exports) {
        JSValue exportedValue;
        PutPropertySlot slot(this);
        slot.setNewProperty(this, offset++);
        switch (e.kind) {
        case Wasm::External::Function: {
            // 1. If e is a closure c:
            //   i. If there is an Exported Function Exotic Object func in funcs whose func.[[Closure]] equals c, then return func.
            //   ii. (Note: At most one wrapper is created for any closure, so func is unique, even if there are multiple occurrances in the list. Moreover, if the item was an import that is already an Exported Function Exotic Object, then the original function object will be found. For imports that are regular JS functions, a new wrapper will be created.)
            //   iii. Otherwise:
            //     a. Let func be an Exported Function Exotic Object created from c.
            //     b. Append func to funcs.
            //     c. Return func.
            const Wasm::FunctionCompilation* compiledFunction = compiledFunctions.at(e.functionIndex).get();
            const B3::Compilation* jsEntryPoint = compiledFunction->jsEntryPoint.get();
            const Wasm::Signature* signature = moduleInformation.functions.at(e.functionIndex).signature;
            WebAssemblyFunction* function = WebAssemblyFunction::create(vm, globalObject, signature->arguments.size(), e.field.string(), CallableWebAssemblyFunction(jsEntryPoint, signature));
            exportedValue = function;
        }
        break;
        case Wasm::External::Table: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164135
        }
        break;
        case Wasm::External::Memory: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164134
        }
        break;
        case Wasm::External::Global: {
            // FIXME https://bugs.webkit.org/show_bug.cgi?id=164133
            // In the MVP, only immutable global variables can be exported.
        }
        break;
        }

        bool shouldThrowReadOnlyError = false;
        bool ignoreReadOnlyErrors = true;
        bool putResult = false;
        symbolTablePutTouchWatchpointSet(moduleEnvironment, state, e.field, exportedValue, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
        RELEASE_ASSERT(putResult);
    }
    
    m_moduleEnvironment.set(vm, this, moduleEnvironment);
}

JSValue WebAssemblyModuleRecord::evaluate(ExecState* state)
{
    // FIXME this should call the module's `start` function, if any.
    // https://github.com/WebAssembly/design/blob/master/Modules.md#module-start-function
    // The start function must not take any arguments or return anything
    UNUSED_PARAM(state);
    return jsUndefined();
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
