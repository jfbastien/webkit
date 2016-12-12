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

#include "WasmFormat.h"
#include "WasmOps.h"
#include "WasmParser.h"
#include <wtf/Vector.h>

namespace JSC { namespace Wasm {

class ModuleParser : public Parser {
public:

    ModuleParser(VM* vm, const uint8_t* sourceBuffer, size_t sourceLength)
        : Parser(sourceBuffer, sourceLength)
        , m_vm(vm)
    {
    }
    ModuleParser(VM* vm, const Vector<uint8_t>& sourceBuffer)
        : ModuleParser(vm, sourceBuffer.data(), sourceBuffer.size())
    {
    }

    Result WARN_UNUSED_RETURN parse();

    std::unique_ptr<ModuleInformation>& moduleInformation()
    {
        RELEASE_ASSERT(!failed());
        return m_module;
    }

    FunctionIndexSpace& functionIndexSpace()
    {
        RELEASE_ASSERT(!failed());
        return m_functionIndexSpace;
    }

    Vector<FunctionLocationInBinary>& functionLocationInBinary()
    {
        RELEASE_ASSERT(!failed());
        return m_functionLocationInBinary;
    }

private:
#define WASM_SECTION_DECLARE_PARSER(NAME, ID, DESCRIPTION) Result WARN_UNUSED_RETURN parse ## NAME();
    FOR_EACH_WASM_SECTION(WASM_SECTION_DECLARE_PARSER)
#undef WASM_SECTION_DECLARE_PARSER

    Result WARN_UNUSED_RETURN parseMemoryHelper(bool isImport);

    VM* m_vm;
    std::unique_ptr<ModuleInformation> m_module;
    FunctionIndexSpace m_functionIndexSpace;
    Vector<FunctionLocationInBinary> m_functionLocationInBinary;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
