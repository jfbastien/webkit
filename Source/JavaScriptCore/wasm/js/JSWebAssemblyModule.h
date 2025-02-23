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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "JSDestructibleObject.h"
#include "JSObject.h"
#include "WasmFormat.h"

namespace JSC {

class SymbolTable;

class JSWebAssemblyModule : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    static JSWebAssemblyModule* create(VM&, Structure*, std::unique_ptr<Wasm::ModuleInformation>&, Wasm::CompiledFunctions&, SymbolTable*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    const Wasm::ModuleInformation& moduleInformation() const { return *m_moduleInformation.get(); }
    const Wasm::CompiledFunctions& compiledFunctions() const { return m_compiledFunctions; }
    SymbolTable* exportSymbolTable() const { return m_exportSymbolTable.get(); }

protected:
    JSWebAssemblyModule(VM&, Structure*, std::unique_ptr<Wasm::ModuleInformation>&, Wasm::CompiledFunctions&);
    void finishCreation(VM&, SymbolTable*);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);
private:
    std::unique_ptr<Wasm::ModuleInformation> m_moduleInformation;
    Wasm::CompiledFunctions m_compiledFunctions;
    WriteBarrier<SymbolTable> m_exportSymbolTable;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
