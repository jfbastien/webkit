/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "RTCDTMFSenderHandlerClient.h"
#include "ScriptWrappable.h"
#include "Timer.h"

namespace WebCore {

class MediaStreamTrack;
class RTCPeerConnectionHandler;
class RTCDTMFSenderHandler;

class RTCDTMFSender final : public RefCounted<RTCDTMFSender>, public EventTargetWithInlineData, private RTCDTMFSenderHandlerClient, public ActiveDOMObject {
public:
    static ExceptionOr<Ref<RTCDTMFSender>> create(ScriptExecutionContext*, RTCPeerConnectionHandler*, RefPtr<MediaStreamTrack>&&);
    virtual ~RTCDTMFSender();

    bool canInsertDTMF() const;
    MediaStreamTrack* track() const;
    String toneBuffer() const;
    long duration() const { return m_duration; }
    long interToneGap() const { return m_interToneGap; }

    ExceptionOr<void> insertDTMF(const String& tones, std::optional<int> duration, std::optional<int> interToneGap);

    using RefCounted::ref;
    using RefCounted::deref;

private:
    RTCDTMFSender(ScriptExecutionContext&, RefPtr<MediaStreamTrack>&&, std::unique_ptr<RTCDTMFSenderHandler>);

    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    EventTargetInterface eventTargetInterface() const final { return RTCDTMFSenderEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    void didPlayTone(const String&) final;

    void scheduleDispatchEvent(Ref<Event>&&);
    void scheduledEventTimerFired();

    RefPtr<MediaStreamTrack> m_track;
    long m_duration;
    long m_interToneGap;

    std::unique_ptr<RTCDTMFSenderHandler> m_handler;

    bool m_stopped;

    Timer m_scheduledEventTimer;
    Vector<Ref<Event>> m_scheduledEvents;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
