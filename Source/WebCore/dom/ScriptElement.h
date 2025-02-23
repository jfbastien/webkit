/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "LoadableScript.h"
#include "LoadableScriptClient.h"
#include "Timer.h"
#include "URL.h"
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedScript;
class ContainerNode;
class Element;
class PendingScript;
class ScriptElement;
class ScriptSourceCode;
class CachedModuleScript;

class ScriptElement {
public:
    virtual ~ScriptElement() { }

    Element& element() { return m_element; }
    const Element& element() const { return m_element; }

    enum LegacyTypeSupport { DisallowLegacyTypeInTypeAttribute, AllowLegacyTypeInTypeAttribute };
    bool prepareScript(const TextPosition& scriptStartPosition = TextPosition::minimumPosition(), LegacyTypeSupport = DisallowLegacyTypeInTypeAttribute);

    String scriptCharset() const { return m_characterEncoding; }
    WEBCORE_EXPORT String scriptContent() const;
    void executeClassicScript(const ScriptSourceCode&);
    void executeModuleScript(CachedModuleScript&);

    void executePendingScript(PendingScript&);

    // XML parser calls these
    virtual void dispatchLoadEvent() = 0;
    void dispatchErrorEvent();

    bool haveFiredLoadEvent() const { return m_haveFiredLoad; }
    bool willBeParserExecuted() const { return m_willBeParserExecuted; }
    bool readyToBeParserExecuted() const { return m_readyToBeParserExecuted; }
    bool willExecuteWhenDocumentFinishedParsing() const { return m_willExecuteWhenDocumentFinishedParsing; }
    bool willExecuteInOrder() const { return m_willExecuteInOrder; }
    LoadableScript* loadableScript() { return m_loadableScript.get(); }

    CachedResourceHandle<CachedScript> requestScriptWithCacheForModuleScript(const URL&);

    // https://html.spec.whatwg.org/multipage/scripting.html#concept-script-type
    enum class ScriptType { Classic, Module };
    ScriptType scriptType() const { return m_isModuleScript ? ScriptType::Module : ScriptType::Classic; }

protected:
    ScriptElement(Element&, bool createdByParser, bool isEvaluated);

    void setHaveFiredLoadEvent(bool haveFiredLoad) { m_haveFiredLoad = haveFiredLoad; }
    bool isParserInserted() const { return m_parserInserted; }
    bool alreadyStarted() const { return m_alreadyStarted; }
    bool forceAsync() const { return m_forceAsync; }

    // Helper functions used by our parent classes.
    bool shouldCallFinishedInsertingSubtree(ContainerNode&);
    void finishedInsertingSubtree();
    void childrenChanged();
    void handleSourceAttribute(const String& sourceURL);
    void handleAsyncAttribute();

private:
    void executeScriptAndDispatchEvent(LoadableScript&);

    std::optional<ScriptType> determineScriptType(LegacyTypeSupport) const;
    bool ignoresLoadRequest() const;
    bool isScriptForEventSupported() const;

    CachedResourceHandle<CachedScript> requestScriptWithCache(const URL&, const String& nonceAttribute, const String& crossoriginAttribute);

    bool requestClassicScript(const String& sourceURL);
    bool requestModuleScript(const TextPosition& scriptStartPosition);

    virtual String sourceAttributeValue() const = 0;
    virtual String charsetAttributeValue() const = 0;
    virtual String typeAttributeValue() const = 0;
    virtual String languageAttributeValue() const = 0;
    virtual String forAttributeValue() const = 0;
    virtual String eventAttributeValue() const = 0;
    virtual bool asyncAttributeValue() const = 0;
    virtual bool deferAttributeValue() const = 0;
    virtual bool hasSourceAttribute() const = 0;

    Element& m_element;
    WTF::OrdinalNumber m_startLineNumber;
    bool m_parserInserted : 1;
    bool m_isExternalScript : 1;
    bool m_alreadyStarted : 1;
    bool m_haveFiredLoad : 1;
    bool m_willBeParserExecuted : 1; // Same as "The parser will handle executing the script."
    bool m_readyToBeParserExecuted : 1;
    bool m_willExecuteWhenDocumentFinishedParsing : 1;
    bool m_forceAsync : 1;
    bool m_willExecuteInOrder : 1;
    bool m_isModuleScript : 1;
    String m_characterEncoding;
    String m_fallbackCharacterEncoding;
    RefPtr<LoadableScript> m_loadableScript;
};

// FIXME: replace with downcast<ScriptElement>.
ScriptElement* toScriptElementIfPossible(Element*);

}
