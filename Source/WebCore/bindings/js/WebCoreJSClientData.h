/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "DOMWrapperWorld.h"
#include "WebCoreBuiltinNames.h"
#include "WebCoreJSBuiltins.h"
#include "WebCoreTypedArrayController.h"
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class JSVMClientData : public JSC::VM::ClientData {
    WTF_MAKE_NONCOPYABLE(JSVMClientData); WTF_MAKE_FAST_ALLOCATED;
    friend class VMWorldIterator;
    friend void initNormalWorldClientData(JSC::VM*);

public:
    explicit JSVMClientData(JSC::VM& vm)
        : m_builtinFunctions(vm)
        , m_builtinNames(&vm)
    {
    }

    virtual ~JSVMClientData()
    {
        ASSERT(m_worldSet.contains(m_normalWorld.get()));
        ASSERT(m_worldSet.size() == 1);
        ASSERT(m_normalWorld->hasOneRef());
        m_normalWorld = nullptr;
        ASSERT(m_worldSet.isEmpty());
    }

    DOMWrapperWorld& normalWorld() { return *m_normalWorld; }

    void getAllWorlds(Vector<Ref<DOMWrapperWorld>>& worlds)
    {
        ASSERT(worlds.isEmpty());

        worlds.reserveInitialCapacity(m_worldSet.size());
        for (auto it = m_worldSet.begin(), end = m_worldSet.end(); it != end; ++it)
            worlds.uncheckedAppend(*(*it));
    }

    void rememberWorld(DOMWrapperWorld& world)
    {
        ASSERT(!m_worldSet.contains(&world));
        m_worldSet.add(&world);
    }

    void forgetWorld(DOMWrapperWorld& world)
    {
        ASSERT(m_worldSet.contains(&world));
        m_worldSet.remove(&world);
    }

    WebCoreBuiltinNames& builtinNames() { return m_builtinNames; }
    JSBuiltinFunctions& builtinFunctions() { return m_builtinFunctions; }

private:
    HashSet<DOMWrapperWorld*> m_worldSet;
    RefPtr<DOMWrapperWorld> m_normalWorld;

    JSBuiltinFunctions m_builtinFunctions;
    WebCoreBuiltinNames m_builtinNames;
};

inline void initNormalWorldClientData(JSC::VM* vm)
{
    JSVMClientData* clientData = new JSVMClientData(*vm);
    vm->clientData = clientData; // ~VM deletes this pointer.
    clientData->m_normalWorld = DOMWrapperWorld::create(*vm, true);
    vm->m_typedArrayController = adoptRef(new WebCoreTypedArrayController());
}

} // namespace WebCore
