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

#include "config.h"

#if ENABLE(WEB_RTC)

#include "RTCDTMFSender.h"

#include "MediaStreamTrack.h"
#include "RTCDTMFSenderHandler.h"
#include "RTCDTMFToneChangeEvent.h"
#include "RTCPeerConnectionHandler.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

static const long minToneDurationMs = 40;
static const long defaultToneDurationMs = 100;
static const long maxToneDurationMs = 6000;
static const long minInterToneGapMs = 30;
static const long defaultInterToneGapMs = 70;

ExceptionOr<Ref<RTCDTMFSender>> RTCDTMFSender::create(ScriptExecutionContext* context, RTCPeerConnectionHandler* peerConnectionHandler, RefPtr<MediaStreamTrack>&& track)
{
    auto handler = peerConnectionHandler->createDTMFSender(&track->source());
    if (!handler)
        return Exception { NOT_SUPPORTED_ERR };

    auto sender = adoptRef(*new RTCDTMFSender(*context, WTFMove(track), WTFMove(handler)));
    sender->suspendIfNeeded();
    return WTFMove(sender);
}

RTCDTMFSender::RTCDTMFSender(ScriptExecutionContext& context, RefPtr<MediaStreamTrack>&& track, std::unique_ptr<RTCDTMFSenderHandler> handler)
    : ActiveDOMObject(&context)
    , m_track(WTFMove(track))
    , m_duration(defaultToneDurationMs)
    , m_interToneGap(defaultInterToneGapMs)
    , m_handler(WTFMove(handler))
    , m_stopped(false)
    , m_scheduledEventTimer(*this, &RTCDTMFSender::scheduledEventTimerFired)
{
    m_handler->setClient(this);
}

RTCDTMFSender::~RTCDTMFSender()
{
}

bool RTCDTMFSender::canInsertDTMF() const
{
    return m_handler->canInsertDTMF();
}

MediaStreamTrack* RTCDTMFSender::track() const
{
    return m_track.get();
}

String RTCDTMFSender::toneBuffer() const
{
    return m_handler->currentToneBuffer();
}

ExceptionOr<void> RTCDTMFSender::insertDTMF(const String& tones, std::optional<int> duration, std::optional<int> interToneGap)
{
    if (!canInsertDTMF())
        return Exception { NOT_SUPPORTED_ERR };

    if (duration && (duration.value() > maxToneDurationMs || duration.value() < minToneDurationMs))
        return Exception { SYNTAX_ERR };

    if (interToneGap && interToneGap.value() < minInterToneGapMs)
        return Exception { SYNTAX_ERR };

    m_duration = duration.value_or(defaultToneDurationMs);
    m_interToneGap = interToneGap.value_or(defaultInterToneGapMs);

    if (!m_handler->insertDTMF(tones, m_duration, m_interToneGap))
        return Exception { SYNTAX_ERR };

    return { };
}

void RTCDTMFSender::didPlayTone(const String& tone)
{
    scheduleDispatchEvent(RTCDTMFToneChangeEvent::create(tone));
}

void RTCDTMFSender::stop()
{
    m_stopped = true;
    m_handler->setClient(nullptr);
}

const char* RTCDTMFSender::activeDOMObjectName() const
{
    return "RTCDTMFSender";
}

bool RTCDTMFSender::canSuspendForDocumentSuspension() const
{
    // FIXME: We should try and do better here.
    return false;
}

void RTCDTMFSender::scheduleDispatchEvent(Ref<Event>&& event)
{
    m_scheduledEvents.append(WTFMove(event));

    if (!m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0);
}

void RTCDTMFSender::scheduledEventTimerFired()
{
    if (m_stopped)
        return;

    Vector<Ref<Event>> events;
    events.swap(m_scheduledEvents);

    for (auto& event : events)
        dispatchEvent(event);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
