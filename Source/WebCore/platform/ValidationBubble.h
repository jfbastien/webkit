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

#pragma once

#include "IntRect.h"
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

#if PLATFORM(MAC)
OBJC_CLASS NSPopover;
#elif PLATFORM(IOS)
OBJC_CLASS UIViewController;
#endif

#if PLATFORM(MAC)
OBJC_CLASS NSView;
using PlatformView = NSView;
#elif PLATFORM(IOS)
OBJC_CLASS UIView;
using PlatformView = UIView;
#else
using PlatformView = void;
#endif

namespace WebCore {

class ValidationBubble {
public:
    WEBCORE_EXPORT ValidationBubble(PlatformView*, const String& message);
    WEBCORE_EXPORT ~ValidationBubble();

    const String& message() const { return m_message; }

#if PLATFORM(IOS)
    WEBCORE_EXPORT void setAnchorRect(const IntRect& anchorRect, UIViewController* presentingViewController);
    WEBCORE_EXPORT void show();
#else
    WEBCORE_EXPORT void showRelativeTo(const IntRect& anchorRect);
#endif

private:
    PlatformView* m_view;
    String m_message;
#if PLATFORM(MAC)
    RetainPtr<NSPopover> m_popover;
#elif PLATFORM(IOS)
    RetainPtr<UIViewController> m_popoverController;
    UIViewController *m_presentingViewController;
#endif
};

}
