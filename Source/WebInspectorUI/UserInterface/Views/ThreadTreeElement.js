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

WebInspector.ThreadTreeElement = class ThreadTreeElement extends WebInspector.GeneralTreeElement
{
    constructor(target)
    {
        super("thread", target.displayName);

        this._target = target;

        this._idleTreeElement = new WebInspector.IdleTreeElement;
    }

    // Public

    get target() { return this._target; }

    refresh()
    {
        this.removeChildren();

        let targetData = WebInspector.debuggerManager.dataForTarget(this._target);
        let callFrames = targetData.callFrames;

        if (targetData.pausing || !callFrames.length) {
            this.appendChild(this._idleTreeElement);
            this.expand();
            return;
        }

        let activeCallFrame = WebInspector.debuggerManager.activeCallFrame;
        let activeCallFrameTreeElement = null;

        for (let callFrame of callFrames) {
            let callFrameTreeElement = new WebInspector.CallFrameTreeElement(callFrame);
            if (callFrame === activeCallFrame)
                activeCallFrameTreeElement = callFrameTreeElement;
            this.appendChild(callFrameTreeElement);
        }

        if (activeCallFrameTreeElement) {
            activeCallFrameTreeElement.select(true, true);
            activeCallFrameTreeElement.isActiveCallFrame = true;
        }

        this.expand();
    }

    // Protected (GeneralTreeElement)

    onattach()
    {
        super.onattach();

        this.refresh();
        this.expand();
    }

    oncontextmenu(event)
    {
        let targetData = WebInspector.debuggerManager.dataForTarget(this._target);

        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);
        if (DebuggerAgent.continueUntilNextRunLoop) {
            contextMenu.appendItem(WebInspector.UIString("Resume Thread"), () => {
                WebInspector.debuggerManager.continueUntilNextRunLoop(this._target);
            }, !targetData.paused);
        }
    }
};
