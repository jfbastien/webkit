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
#include <wtf/Vector.h>

namespace JSC {

class JSModuleNamespaceObject;
class JSWebAssemblyModule;

class JSWebAssemblyInstance : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;


    static JSWebAssemblyInstance* create(VM&, Structure*, JSWebAssemblyModule*, JSModuleNamespaceObject*, Vector<WriteBarrier<JSCell>>&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    JSWebAssemblyModule* module()
    {
        ASSERT(m_module);
        return m_module.get();
    }

    JSCell* getImportFunction(unsigned num) const { return m_importFunctions[num].get(); }

protected:
    JSWebAssemblyInstance(VM&, Structure*);
    void finishCreation(VM&, JSWebAssemblyModule*, JSModuleNamespaceObject*, Vector<WriteBarrier<JSCell>>&&);
    static void destroy(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);

private:
    WriteBarrier<JSWebAssemblyModule> m_module;
    WriteBarrier<JSModuleNamespaceObject> m_moduleNamespaceObject;
    Vector<WriteBarrier<JSCell>> m_importFunctions;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
