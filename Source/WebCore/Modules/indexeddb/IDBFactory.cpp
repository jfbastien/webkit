/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBFactory.h"

#if ENABLE(INDEXED_DATABASE)

#include "Document.h"
#include "ExceptionCode.h"
#include "IDBBindingUtilities.h"
#include "IDBConnectionProxy.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBKey.h"
#include "IDBOpenDBRequest.h"
#include "Logging.h"
#include "Page.h"
#include "SchemeRegistry.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

using namespace JSC;

namespace WebCore {

static bool shouldThrowSecurityException(ScriptExecutionContext& context)
{
    ASSERT(is<Document>(context) || context.isWorkerGlobalScope());
    if (is<Document>(context)) {
        Document& document = downcast<Document>(context);
        if (!document.frame())
            return true;
        if (!document.page())
            return true;
    }

    if (!context.securityOrigin()->canAccessDatabase(context.topOrigin()))
        return true;

    return false;
}

Ref<IDBFactory> IDBFactory::create(IDBClient::IDBConnectionProxy& connectionProxy)
{
    return adoptRef(*new IDBFactory(connectionProxy));
}

IDBFactory::IDBFactory(IDBClient::IDBConnectionProxy& connectionProxy)
    : m_connectionProxy(connectionProxy)
{
}

IDBFactory::~IDBFactory()
{
}

ExceptionOr<Ref<IDBOpenDBRequest>> IDBFactory::open(ScriptExecutionContext& context, const String& name, std::optional<uint64_t> version)
{
    LOG(IndexedDB, "IDBFactory::open");
    
    if (version && !version.value())
        return Exception { TypeError, ASCIILiteral("IDBFactory.open() called with a version of 0") };

    return openInternal(context, name, version.value_or(0));
}

ExceptionOr<Ref<IDBOpenDBRequest>> IDBFactory::openInternal(ScriptExecutionContext& context, const String& name, unsigned long long version)
{
    if (name.isNull())
        return Exception { TypeError, ASCIILiteral("IDBFactory.open() called without a database name") };

    if (shouldThrowSecurityException(context))
        return Exception { SECURITY_ERR, ASCIILiteral("IDBFactory.open() called in an invalid security context") };

    ASSERT(context.securityOrigin());
    ASSERT(context.topOrigin());
    IDBDatabaseIdentifier databaseIdentifier(name, *context.securityOrigin(), *context.topOrigin());
    if (!databaseIdentifier.isValid())
        return Exception { TypeError, ASCIILiteral("IDBFactory.open() called with an invalid security origin") };

    return m_connectionProxy->openDatabase(context, databaseIdentifier, version);
}

ExceptionOr<Ref<IDBOpenDBRequest>> IDBFactory::deleteDatabase(ScriptExecutionContext& context, const String& name)
{
    LOG(IndexedDB, "IDBFactory::deleteDatabase - %s", name.utf8().data());

    if (name.isNull())
        return Exception { TypeError, ASCIILiteral("IDBFactory.deleteDatabase() called without a database name") };

    if (shouldThrowSecurityException(context))
        return Exception { SECURITY_ERR, ASCIILiteral("IDBFactory.deleteDatabase() called in an invalid security context") };

    ASSERT(context.securityOrigin());
    ASSERT(context.topOrigin());
    IDBDatabaseIdentifier databaseIdentifier(name, *context.securityOrigin(), *context.topOrigin());
    if (!databaseIdentifier.isValid())
        return Exception { TypeError, ASCIILiteral("IDBFactory.deleteDatabase() called with an invalid security origin") };

    return m_connectionProxy->deleteDatabase(context, databaseIdentifier);
}

ExceptionOr<short> IDBFactory::cmp(ExecState& execState, JSValue firstValue, JSValue secondValue)
{
    auto first = scriptValueToIDBKey(execState, firstValue);
    auto second = scriptValueToIDBKey(execState, secondValue);

    if (!first->isValid() || !second->isValid())
        return Exception { IDBDatabaseException::DataError, ASCIILiteral("Failed to execute 'cmp' on 'IDBFactory': The parameter is not a valid key.") };

    return first->compare(second.get());
}

void IDBFactory::getAllDatabaseNames(const SecurityOrigin& mainFrameOrigin, const SecurityOrigin& openingOrigin, std::function<void (const Vector<String>&)> callback)
{
    m_connectionProxy->getAllDatabaseNames(mainFrameOrigin, openingOrigin, callback);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
