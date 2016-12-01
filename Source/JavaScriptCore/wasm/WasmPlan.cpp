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
#include "WasmPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "WasmB3IRGenerator.h"
#include "WasmCallingConvention.h"
#include "WasmMemory.h"
#include "WasmModuleParser.h"
#include "WasmValidate.h"
#include <wtf/DataLog.h>
#include <wtf/text/StringBuilder.h>

namespace JSC { namespace Wasm {

static const bool verbose = false;
    
Plan::Plan(VM* vm, Vector<uint8_t> source)
    : Plan(vm, source.data(), source.size())
{
}

Plan::Plan(VM* vm, const uint8_t* source, size_t sourceLength)
    : m_vm(vm)
    , m_source(source)
    , m_sourceLength(sourceLength)
{
}

void Plan::run()
{
    if (verbose)
        dataLogLn("Starting plan.");
    {
        ModuleParser moduleParser(m_vm, m_source, m_sourceLength);
        if (!moduleParser.parse()) {
            if (verbose)
                dataLogLn("Parsing module failed: ", moduleParser.errorMessage());
            m_errorMessage = moduleParser.errorMessage();
            return;
        }
        m_moduleInformation = WTFMove(moduleParser.moduleInformation());
    }
    if (verbose)
        dataLogLn("Parsed module.");

    size_t numFunctions = m_moduleInformation->numImportFunctions + m_moduleInformation->functions.size();
    if (!m_compiledFunctions.tryReserveCapacity(numFunctions)) {
        StringBuilder builder;
        builder.appendLiteral("Failed allocating enough space for ");
        builder.appendNumber(numFunctions);
        builder.appendLiteral(" compiled functions");
        m_errorMessage = builder.toString();
        return;
    }

    for (const Import& import : m_moduleInformation->imports) {
        switch (import.kind) {
        case External::Function:
            if (verbose)
                dataLogLn("Processing import function ", import.module, " ", import.field);
            // Imports come before WebAssembly functions in the function index space.
            m_compiledFunctions.uncheckedAppend(compileImportStubs(*m_vm, import.functionSignature));
            break;
        case External::Table:
            // FIXME implement Table https://bugs.webkit.org/show_bug.cgi?id=164135
            RELEASE_ASSERT_NOT_REACHED();
        case External::Memory:
            // FIXME implement Memory https://bugs.webkit.org/show_bug.cgi?id=164134
            RELEASE_ASSERT_NOT_REACHED();
        case External::Global:
            // FIXME implement Global https://bugs.webkit.org/show_bug.cgi?id=164133
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    ASSERT(m_compiledFunctions.size() == m_moduleInformation->numImportFunctions);

    // WebAssembly functions come after imports in the function index space.
    for (const FunctionInformation& info : m_moduleInformation->functions) {
        if (verbose)
            dataLogLn("Processing function starting at: ", info.start, " and ending at: ", info.end);
        const uint8_t* functionStart = m_source + info.start;
        size_t functionLength = info.end - info.start;
        ASSERT(functionLength <= m_sourceLength);

        String error = validateFunction(functionStart, functionLength, info.signature, m_moduleInformation->functions);
        if (!error.isNull()) {
            if (verbose) {
                for (unsigned i = 0; i < functionLength; ++i)
                    dataLog(RawPointer(reinterpret_cast<void*>(functionStart[i])), ", ");
                dataLogLn();
            }
            m_errorMessage = error;
            return;
        }

        m_compiledFunctions.uncheckedAppend(parseAndCompile(*m_vm, functionStart, functionLength, m_moduleInformation->memory.get(), info.signature, m_moduleInformation->functions));
    }
    ASSERT(m_compiledFunctions.size() == numFunctions);

    // Patch the call sites for each WebAssembly function.
    for (std::unique_ptr<FunctionCompilation>& functionPtr : m_compiledFunctions) {
        FunctionCompilation* function = functionPtr.get();
        for (auto& call : function->unlinkedWasmToWasmCalls)
            MacroAssembler::repatchCall(call.callLocation, CodeLocationLabel(m_compiledFunctions[call.functionIndex]->code->code()));
    }

    m_failed = false;
}

Plan::~Plan() { }

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
