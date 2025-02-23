/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmAES_KW.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoKeyAES.h"
#include "ExceptionCode.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

// FIXME: We should change data to Vector<uint8_t> type once WebKitSubtleCrypto is deprecated.
// https://bugs.webkit.org/show_bug.cgi?id=164939
static ExceptionOr<Vector<uint8_t>> wrapKeyAES_KW(const Vector<uint8_t>& key, const uint8_t* data, size_t dataLength)
{
    Vector<uint8_t> result(CCSymmetricWrappedSize(kCCWRAPAES, dataLength));
    size_t resultSize = result.size();
    if (CCSymmetricKeyWrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, key.data(), key.size(), data, dataLength, result.data(), &resultSize))
        return Exception { OperationError };

    result.shrink(resultSize);
    return WTFMove(result);
}

void CryptoAlgorithmAES_KW::platformWrapKey(Ref<CryptoKey>&& key, Vector<uint8_t>&& data, VectorCallback&& callback, ExceptionCallback&& exceptionCallback)
{
    auto& aesKey = downcast<CryptoKeyAES>(key.get());
    auto result = wrapKeyAES_KW(aesKey.key(), data.data(), data.size());
    if (result.hasException()) {
        exceptionCallback(result.releaseException().code());
        return;
    }
    callback(result.releaseReturnValue());
}

ExceptionOr<void> CryptoAlgorithmAES_KW::platformEncrypt(const CryptoKeyAES& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    if (data.second % 8) {
        // RFC 3394 uses 64-bit blocks as input.
        // <rdar://problem/15949992> CommonCrypto doesn't detect incorrect data length, silently producing a bad cyphertext.
        failureCallback();
        return { };
    }

    auto result = wrapKeyAES_KW(key.key(), data.first, data.second);
    if (result.hasException()) {
        failureCallback();
        return { };
    }
    callback(result.releaseReturnValue());
    return { };
}

ExceptionOr<void> CryptoAlgorithmAES_KW::platformDecrypt(const CryptoKeyAES& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    Vector<uint8_t> result(CCSymmetricUnwrappedSize(kCCWRAPAES, data.second));
    size_t resultSize = result.size();

    if (resultSize % 8) {
        failureCallback();
        return { };
    }

    int status = CCSymmetricKeyUnwrap(kCCWRAPAES, CCrfc3394_iv, CCrfc3394_ivLen, key.key().data(), key.key().size(), data.first, data.second, result.data(), &resultSize);
    if (status) {
        failureCallback();
        return { };
    }
    result.shrink(resultSize);
    callback(result);
    return { };
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
