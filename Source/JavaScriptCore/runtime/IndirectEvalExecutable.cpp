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
#include "IndirectEvalExecutable.h"

#include "Error.h"
#include "HeapInlines.h"
#include "JSCJSValueInlines.h"

namespace JSC {

IndirectEvalExecutable* IndirectEvalExecutable::create(ExecState* exec, const SourceCode& source, bool isInStrictContext, DerivedContextType derivedContextType, bool isArrowFunctionContext, EvalContextType evalContextType)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    if (!globalObject->evalEnabled()) {
        throwException(exec, scope, createEvalError(exec, globalObject->evalDisabledErrorMessage()));
        return 0;
    }

    auto* executable = new (NotNull, allocateCell<IndirectEvalExecutable>(*exec->heap())) IndirectEvalExecutable(exec, source, isInStrictContext, derivedContextType, isArrowFunctionContext, evalContextType);
    executable->finishCreation(vm);

    UnlinkedEvalCodeBlock* unlinkedEvalCode = globalObject->createGlobalEvalCodeBlock(exec, executable);
    ASSERT(!!scope.exception() == !unlinkedEvalCode);
    if (!unlinkedEvalCode)
        return 0;

    executable->m_unlinkedEvalCodeBlock.set(vm, executable, unlinkedEvalCode);

    return executable;
}

IndirectEvalExecutable::IndirectEvalExecutable(ExecState* exec, const SourceCode& source, bool inStrictContext, DerivedContextType derivedContextType, bool isArrowFunctionContext, EvalContextType evalContextType)
    : EvalExecutable(exec, source, inStrictContext, derivedContextType, isArrowFunctionContext, evalContextType)
{
}

} // namespace JSC
