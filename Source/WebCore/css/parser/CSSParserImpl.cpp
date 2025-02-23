// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSParserImpl.h"

#include "CSSAtRuleID.h"
#include "CSSCustomPropertyValue.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSParserObserver.h"
#include "CSSParserObserverWrapper.h"
#include "CSSParserValues.h" // FIXME-NEWPARSER We need to move CSSParserSelector to its own file.
#include "CSSPropertyParser.h"
#include "CSSSelectorParser.h"
#include "CSSStyleSheet.h"
#include "CSSSupportsParser.h"
#include "CSSTokenizer.h"
#include "CSSVariableParser.h"
#include "Document.h"
#include "Element.h"
#include "MediaQueryParser.h"
#include "StyleProperties.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"

#include <bitset>
#include <memory>

namespace WebCore {

CSSParserImpl::CSSParserImpl(const CSSParserContext& context, StyleSheetContents* styleSheet)
    : m_context(context)
    , m_styleSheet(styleSheet)
    , m_observerWrapper(nullptr)
{
}

bool CSSParserImpl::parseValue(MutableStyleProperties* declaration, CSSPropertyID propertyID, const String& string, bool important, const CSSParserContext& context)
{
    CSSParserImpl parser(context);
    StyleRule::Type ruleType = StyleRule::Style;
#if ENABLE(CSS_DEVICE_ADAPTATION)
    if (declaration->cssParserMode() == CSSViewportRuleMode)
        ruleType = StyleRule::Viewport;
#endif
    CSSTokenizer::Scope scope(string);
    parser.consumeDeclarationValue(scope.tokenRange(), propertyID, important, ruleType);
    if (parser.m_parsedProperties.isEmpty())
        return false;
    return declaration->addParsedProperties(parser.m_parsedProperties);
}

static inline void filterProperties(bool important, const ParsedPropertyVector& input, ParsedPropertyVector& output, size_t& unusedEntries, std::bitset<numCSSProperties>& seenProperties, HashSet<AtomicString>& seenCustomProperties)
{
    // Add properties in reverse order so that highest priority definitions are reached first. Duplicate definitions can then be ignored when found.
    for (size_t i = input.size(); i--; ) {
        const CSSProperty& property = input[i];
        if (property.isImportant() != important)
            continue;
        const unsigned propertyIDIndex = property.id() - firstCSSProperty;
        
        if (property.id() == CSSPropertyCustom) {
            auto& name = downcast<CSSCustomPropertyValue>(*property.value()).name();
            if (!seenCustomProperties.add(name).isNewEntry)
                continue;
            output[--unusedEntries] = property;
            continue;
        }
        
        // FIXME-NEWPARSER: We won't support @apply yet.
        /*else if (property.id() == CSSPropertyApplyAtRule) {
         // FIXME: Do we need to do anything here?
         } */
        
        if (seenProperties.test(propertyIDIndex))
            continue;
        seenProperties.set(propertyIDIndex);

        output[--unusedEntries] = property;
    }
}

static Ref<ImmutableStyleProperties> createStyleProperties(ParsedPropertyVector& parsedProperties, CSSParserMode mode)
{
    std::bitset<numCSSProperties> seenProperties;
    size_t unusedEntries = parsedProperties.size();
    ParsedPropertyVector results(unusedEntries);
    HashSet<AtomicString> seenCustomProperties;

    filterProperties(true, parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    filterProperties(false, parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);

    Ref<ImmutableStyleProperties> result = ImmutableStyleProperties::create(results.data() + unusedEntries, results.size() - unusedEntries, mode);
    parsedProperties.clear();
    return result;
}

Ref<ImmutableStyleProperties> CSSParserImpl::parseInlineStyleDeclaration(const String& string, Element* element)
{
    CSSParserContext context(element->document());
    context.mode = strictToCSSParserMode(element->isHTMLElement() && !element->document().inQuirksMode());

    CSSParserImpl parser(context);
    CSSTokenizer::Scope scope(string);
    parser.consumeDeclarationList(scope.tokenRange(), StyleRule::Style);
    return createStyleProperties(parser.m_parsedProperties, context.mode);
}

bool CSSParserImpl::parseDeclarationList(MutableStyleProperties* declaration, const String& string, const CSSParserContext& context)
{
    CSSParserImpl parser(context);
    StyleRule::Type ruleType = StyleRule::Style;
#if ENABLE(CSS_DEVICE_ADAPTATION)
    if (declaration->cssParserMode() == CSSViewportRuleMode)
        ruleType = StyleRule::Viewport;
#endif
    CSSTokenizer::Scope scope(string);
    parser.consumeDeclarationList(scope.tokenRange(), ruleType);
    if (parser.m_parsedProperties.isEmpty())
        return false;

    std::bitset<numCSSProperties> seenProperties;
    size_t unusedEntries = parser.m_parsedProperties.size();
    ParsedPropertyVector results(unusedEntries);
    HashSet<AtomicString> seenCustomProperties;
    filterProperties(true, parser.m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    filterProperties(false, parser.m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    if (unusedEntries)
        results.remove(0, unusedEntries);
    return declaration->addParsedProperties(results);
}

RefPtr<StyleRuleBase> CSSParserImpl::parseRule(const String& string, const CSSParserContext& context, StyleSheetContents* styleSheet, AllowedRulesType allowedRules)
{
    CSSParserImpl parser(context, styleSheet);
    CSSTokenizer::Scope scope(string);
    CSSParserTokenRange range = scope.tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return nullptr; // Parse error, empty rule
    RefPtr<StyleRuleBase> rule;
    if (range.peek().type() == AtKeywordToken)
        rule = parser.consumeAtRule(range, allowedRules);
    else
        rule = parser.consumeQualifiedRule(range, allowedRules);
    if (!rule)
        return nullptr; // Parse error, failed to consume rule
    range.consumeWhitespace();
    if (!rule || !range.atEnd())
        return nullptr; // Parse error, trailing garbage
    return rule;
}

void CSSParserImpl::parseStyleSheet(const String& string, const CSSParserContext& context, StyleSheetContents* styleSheet)
{
    CSSTokenizer::Scope scope(string);
    CSSParserImpl parser(context, styleSheet);
    bool firstRuleValid = parser.consumeRuleList(scope.tokenRange(), TopLevelRuleList, [&styleSheet](RefPtr<StyleRuleBase> rule) {
        if (rule->isCharsetRule())
            return;
        styleSheet->parserAppendRule(rule.releaseNonNull());
    });
    styleSheet->setHasSyntacticallyValidCSSHeader(firstRuleValid);
}

CSSSelectorList CSSParserImpl::parsePageSelector(CSSParserTokenRange range, StyleSheetContents* styleSheet)
{
    // We only support a small subset of the css-page spec.
    range.consumeWhitespace();
    AtomicString typeSelector;
    if (range.peek().type() == IdentToken)
        typeSelector = range.consume().value().toAtomicString();

    AtomicString pseudo;
    if (range.peek().type() == ColonToken) {
        range.consume();
        if (range.peek().type() != IdentToken)
            return CSSSelectorList();
        pseudo = range.consume().value().toAtomicString();
    }

    range.consumeWhitespace();
    if (!range.atEnd())
        return CSSSelectorList(); // Parse error; extra tokens in @page selector

    std::unique_ptr<CSSParserSelector> selector;
    if (!typeSelector.isNull() && pseudo.isNull())
        selector = std::unique_ptr<CSSParserSelector>(new CSSParserSelector(QualifiedName(nullAtom, typeSelector, styleSheet->defaultNamespace())));
    else {
        selector = std::unique_ptr<CSSParserSelector>(new CSSParserSelector);
        if (!pseudo.isNull()) {
            selector = std::unique_ptr<CSSParserSelector>(CSSParserSelector::parsePagePseudoSelector(pseudo));
            if (!selector || selector->match() != CSSSelector::PagePseudoClass)
                return CSSSelectorList();
        }
        if (!typeSelector.isNull())
            selector->prependTagSelector(QualifiedName(nullAtom, typeSelector, styleSheet->defaultNamespace()));
    }

    selector->setForPage();
    Vector<std::unique_ptr<CSSParserSelector>> selectorVector;
    selectorVector.append(WTFMove(selector));
    CSSSelectorList selectorList;
    selectorList.adoptSelectorVector(selectorVector);
    return selectorList;
}

std::unique_ptr<Vector<double>> CSSParserImpl::parseKeyframeKeyList(const String& keyList)
{
    return consumeKeyframeKeyList(CSSTokenizer::Scope(keyList).tokenRange());
}

bool CSSParserImpl::supportsDeclaration(CSSParserTokenRange& range)
{
    ASSERT(m_parsedProperties.isEmpty());
    consumeDeclaration(range, StyleRule::Style);
    bool result = !m_parsedProperties.isEmpty();
    m_parsedProperties.clear();
    return result;
}

void CSSParserImpl::parseDeclarationListForInspector(const String& declaration, const CSSParserContext& context, CSSParserObserver& observer)
{
    CSSParserImpl parser(context);
    CSSParserObserverWrapper wrapper(observer);
    parser.m_observerWrapper = &wrapper;
    CSSTokenizer::Scope scope(declaration, wrapper);
    observer.startRuleHeader(StyleRule::Style, 0);
    observer.endRuleHeader(1);
    parser.consumeDeclarationList(scope.tokenRange(), StyleRule::Style);
}

void CSSParserImpl::parseStyleSheetForInspector(const String& string, const CSSParserContext& context, StyleSheetContents* styleSheet, CSSParserObserver& observer)
{
    CSSParserImpl parser(context, styleSheet);
    CSSParserObserverWrapper wrapper(observer);
    parser.m_observerWrapper = &wrapper;
    CSSTokenizer::Scope scope(string, wrapper);
    bool firstRuleValid = parser.consumeRuleList(scope.tokenRange(), TopLevelRuleList, [&styleSheet](RefPtr<StyleRuleBase> rule) {
        if (rule->isCharsetRule())
            return;
        styleSheet->parserAppendRule(rule.releaseNonNull());
    });
    styleSheet->setHasSyntacticallyValidCSSHeader(firstRuleValid);
}

static CSSParserImpl::AllowedRulesType computeNewAllowedRules(CSSParserImpl::AllowedRulesType allowedRules, StyleRuleBase* rule)
{
    if (!rule || allowedRules == CSSParserImpl::KeyframeRules || allowedRules == CSSParserImpl::NoRules)
        return allowedRules;
    ASSERT(allowedRules <= CSSParserImpl::RegularRules);
    if (rule->isCharsetRule() || rule->isImportRule())
        return CSSParserImpl::AllowImportRules;
    if (rule->isNamespaceRule())
        return CSSParserImpl::AllowNamespaceRules;
    return CSSParserImpl::RegularRules;
}

template<typename T>
bool CSSParserImpl::consumeRuleList(CSSParserTokenRange range, RuleListType ruleListType, const T callback)
{
    AllowedRulesType allowedRules = RegularRules;
    switch (ruleListType) {
    case TopLevelRuleList:
        allowedRules = AllowCharsetRules;
        break;
    case RegularRuleList:
        allowedRules = RegularRules;
        break;
    case KeyframesRuleList:
        allowedRules = KeyframeRules;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    bool seenRule = false;
    bool firstRuleValid = false;
    while (!range.atEnd()) {
        RefPtr<StyleRuleBase> rule;
        switch (range.peek().type()) {
        case WhitespaceToken:
            range.consumeWhitespace();
            continue;
        case AtKeywordToken:
            rule = consumeAtRule(range, allowedRules);
            break;
        case CDOToken:
        case CDCToken:
            if (ruleListType == TopLevelRuleList) {
                range.consume();
                continue;
            }
            FALLTHROUGH;
        default:
            rule = consumeQualifiedRule(range, allowedRules);
            break;
        }
        if (!seenRule) {
            seenRule = true;
            firstRuleValid = rule;
        }
        if (rule) {
            allowedRules = computeNewAllowedRules(allowedRules, rule.get());
            callback(rule);
        }
    }

    return firstRuleValid;
}

RefPtr<StyleRuleBase> CSSParserImpl::consumeAtRule(CSSParserTokenRange& range, AllowedRulesType allowedRules)
{
    ASSERT(range.peek().type() == AtKeywordToken);
    const StringView name = range.consumeIncludingWhitespace().value();
    const CSSParserToken* preludeStart = &range.peek();
    while (!range.atEnd() && range.peek().type() != LeftBraceToken && range.peek().type() != SemicolonToken)
        range.consumeComponentValue();

    CSSParserTokenRange prelude = range.makeSubRange(preludeStart, &range.peek());
    CSSAtRuleID id = cssAtRuleID(name);
    
    if (range.atEnd() || range.peek().type() == SemicolonToken) {
        range.consume();
        if (allowedRules == AllowCharsetRules && id == CSSAtRuleCharset)
            return consumeCharsetRule(prelude);
        if (allowedRules <= AllowImportRules && id == CSSAtRuleImport)
            return consumeImportRule(prelude);
        if (allowedRules <= AllowNamespaceRules && id == CSSAtRuleNamespace)
            return consumeNamespaceRule(prelude);
        // FIXME-NEWPARSER: Support "apply"
        /*if (allowedRules == ApplyRules && id == CSSAtRuleApply) {
            consumeApplyRule(prelude);
            return nullptr; // consumeApplyRule just updates m_parsedProperties
        }*/
        return nullptr; // Parse error, unrecognised at-rule without block
    }

    CSSParserTokenRange block = range.consumeBlock();
    if (allowedRules == KeyframeRules)
        return nullptr; // Parse error, no at-rules supported inside @keyframes
    if (allowedRules == NoRules || allowedRules == ApplyRules)
        return nullptr; // Parse error, no at-rules with blocks supported inside declaration lists

    ASSERT(allowedRules <= RegularRules);

    switch (id) {
    case CSSAtRuleMedia:
        return consumeMediaRule(prelude, block);
    case CSSAtRuleSupports:
        return consumeSupportsRule(prelude, block);
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case CSSAtRuleViewport:
        return consumeViewportRule(prelude, block);
#endif
    case CSSAtRuleFontFace:
        return consumeFontFaceRule(prelude, block);
    case CSSAtRuleWebkitKeyframes:
        return consumeKeyframesRule(true, prelude, block);
    case CSSAtRuleKeyframes:
        return consumeKeyframesRule(false, prelude, block);
    case CSSAtRulePage:
        return consumePageRule(prelude, block);
#if ENABLE(CSS_REGIONS)
    case CSSAtRuleWebkitRegion:
        return consumeRegionRule(prelude, block);
#endif
    default:
        return nullptr; // Parse error, unrecognised at-rule with block
    }
}

RefPtr<StyleRuleBase> CSSParserImpl::consumeQualifiedRule(CSSParserTokenRange& range, AllowedRulesType allowedRules)
{
    const CSSParserToken* preludeStart = &range.peek();
    while (!range.atEnd() && range.peek().type() != LeftBraceToken)
        range.consumeComponentValue();

    if (range.atEnd())
        return nullptr; // Parse error, EOF instead of qualified rule block

    CSSParserTokenRange prelude = range.makeSubRange(preludeStart, &range.peek());
    CSSParserTokenRange block = range.consumeBlock();

    if (allowedRules <= RegularRules)
        return consumeStyleRule(prelude, block);
    if (allowedRules == KeyframeRules)
        return consumeKeyframeStyleRule(prelude, block);

    ASSERT_NOT_REACHED();
    return nullptr;
}

// This may still consume tokens if it fails
static AtomicString consumeStringOrURI(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();

    if (token.type() == StringToken || token.type() == UrlToken)
        return range.consumeIncludingWhitespace().value().toAtomicString();

    if (token.type() != FunctionToken || !equalIgnoringASCIICase(token.value(), "url"))
        return AtomicString();

    CSSParserTokenRange contents = range.consumeBlock();
    const CSSParserToken& uri = contents.consumeIncludingWhitespace();
    if (uri.type() == BadStringToken || !contents.atEnd())
        return AtomicString();
    return uri.value().toAtomicString();
}

RefPtr<StyleRuleCharset> CSSParserImpl::consumeCharsetRule(CSSParserTokenRange prelude)
{
    const CSSParserToken& string = prelude.consumeIncludingWhitespace();
    if (string.type() != StringToken || !prelude.atEnd())
        return nullptr; // Parse error, expected a single string
    return StyleRuleCharset::create();
}

RefPtr<StyleRuleImport> CSSParserImpl::consumeImportRule(CSSParserTokenRange prelude)
{
    AtomicString uri(consumeStringOrURI(prelude));
    if (uri.isNull())
        return nullptr; // Parse error, expected string or URI

    if (m_observerWrapper) {
        unsigned endOffset = m_observerWrapper->endOffset(prelude);
        m_observerWrapper->observer().startRuleHeader(StyleRule::Import, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(endOffset);
        m_observerWrapper->observer().startRuleBody(endOffset);
        m_observerWrapper->observer().endRuleBody(endOffset);
    }

    return StyleRuleImport::create(uri, MediaQueryParser::parseMediaQuerySet(prelude).releaseNonNull());
}

RefPtr<StyleRuleNamespace> CSSParserImpl::consumeNamespaceRule(CSSParserTokenRange prelude)
{
    AtomicString namespacePrefix;
    if (prelude.peek().type() == IdentToken)
        namespacePrefix = prelude.consumeIncludingWhitespace().value().toAtomicString();

    AtomicString uri(consumeStringOrURI(prelude));
    if (uri.isNull() || !prelude.atEnd())
        return nullptr; // Parse error, expected string or URI

    return StyleRuleNamespace::create(namespacePrefix, uri);
}

RefPtr<StyleRuleMedia> CSSParserImpl::consumeMediaRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    Vector<RefPtr<StyleRuleBase>> rules;

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRule::Media, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
    }

    consumeRuleList(block, RegularRuleList, [&rules](RefPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });

    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));

    return StyleRuleMedia::create(MediaQueryParser::parseMediaQuerySet(prelude).releaseNonNull(), rules);
}

RefPtr<StyleRuleSupports> CSSParserImpl::consumeSupportsRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSSupportsParser::SupportsResult supported = CSSSupportsParser::supportsCondition(prelude, *this);
    if (supported == CSSSupportsParser::Invalid)
        return nullptr; // Parse error, invalid @supports condition

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRule::Supports, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
    }

    Vector<RefPtr<StyleRuleBase>> rules;
    consumeRuleList(block, RegularRuleList, [&rules](RefPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });

    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));

    return StyleRuleSupports::create(prelude.serialize().stripWhiteSpace(), supported, rules);
}

#if ENABLE(CSS_DEVICE_ADAPTATION)
RefPtr<StyleRuleViewport> CSSParserImpl::consumeViewportRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!prelude.atEnd())
        return nullptr; // Parser error; @viewport prelude should be empty

    if (m_observerWrapper) {
        unsigned endOffset = m_observerWrapper->endOffset(prelude);
        m_observerWrapper->observer().startRuleHeader(StyleRule::Viewport, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(endOffset);
        m_observerWrapper->observer().startRuleBody(endOffset);
        m_observerWrapper->observer().endRuleBody(endOffset);
    }

    consumeDeclarationList(block, StyleRule::Viewport);
    return StyleRuleViewport::create(createStyleProperties(m_parsedProperties, CSSViewportRuleMode));
}
#endif

RefPtr<StyleRuleFontFace> CSSParserImpl::consumeFontFaceRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    if (!prelude.atEnd())
        return nullptr; // Parse error; @font-face prelude should be empty

    if (m_observerWrapper) {
        unsigned endOffset = m_observerWrapper->endOffset(prelude);
        m_observerWrapper->observer().startRuleHeader(StyleRule::FontFace, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(endOffset);
        m_observerWrapper->observer().startRuleBody(endOffset);
        m_observerWrapper->observer().endRuleBody(endOffset);
    }

    consumeDeclarationList(block, StyleRule::FontFace);
    return StyleRuleFontFace::create(createStyleProperties(m_parsedProperties, m_context.mode));
}

RefPtr<StyleRuleKeyframes> CSSParserImpl::consumeKeyframesRule(bool webkitPrefixed, CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSParserTokenRange rangeCopy = prelude; // For inspector callbacks
    const CSSParserToken& nameToken = prelude.consumeIncludingWhitespace();
    if (!prelude.atEnd())
        return nullptr; // Parse error; expected single non-whitespace token in @keyframes header

    String name;
    if (nameToken.type() == IdentToken) {
        name = nameToken.value().toString();
    } else if (nameToken.type() == StringToken && webkitPrefixed)
        name = nameToken.value().toString();
    else
        return nullptr; // Parse error; expected ident token in @keyframes header

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRule::Keyframes, m_observerWrapper->startOffset(rangeCopy));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));
    }

    RefPtr<StyleRuleKeyframes> keyframeRule = StyleRuleKeyframes::create();
    consumeRuleList(block, KeyframesRuleList, [keyframeRule](RefPtr<StyleRuleBase> keyframe) {
        RefPtr<StyleKeyframe> key = static_cast<StyleKeyframe*>(keyframe.get());
        keyframeRule->parserAppendKeyframe(key.releaseNonNull());
    });
    keyframeRule->setName(name);
    // FIXME-NEWPARSER: Find out why this is done. Behavior difference when prefixed?
    // keyframeRule->setVendorPrefixed(webkitPrefixed);
    return keyframeRule;
}

RefPtr<StyleRulePage> CSSParserImpl::consumePageRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSSelectorList selectorList = parsePageSelector(prelude, m_styleSheet.get());
    if (!selectorList.isValid())
        return nullptr; // Parse error, invalid @page selector

    if (m_observerWrapper) {
        unsigned endOffset = m_observerWrapper->endOffset(prelude);
        m_observerWrapper->observer().startRuleHeader(StyleRule::Page, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(endOffset);
    }

    consumeDeclarationList(block, StyleRule::Style);
    
    RefPtr<StyleRulePage> page = StyleRulePage::create(createStyleProperties(m_parsedProperties, m_context.mode));
    page->wrapperAdoptSelectorList(selectorList);
    return page;
}

#if ENABLE(CSS_REGIONS)
RefPtr<StyleRuleRegion> CSSParserImpl::consumeRegionRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSSelectorList selectorList = CSSSelectorParser::parseSelector(prelude, m_context, m_styleSheet.get());
    if (!selectorList.isValid())
        return nullptr; // Parse error, invalid selector list

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRule::Region, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(block));
    }
    
    Vector<RefPtr<StyleRuleBase>> rules;
    consumeRuleList(block, RegularRuleList, [&rules](RefPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });
    
    if (m_observerWrapper)
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(block));
    
    return StyleRuleRegion::create(selectorList, rules);

}
#endif

// FIXME-NEWPARSER: Support "apply"
/*void CSSParserImpl::consumeApplyRule(CSSParserTokenRange prelude)
{
    const CSSParserToken& ident = prelude.consumeIncludingWhitespace();
    if (!prelude.atEnd() || !CSSVariableParser::isValidVariableName(ident))
        return; // Parse error, expected a single custom property name
    m_parsedProperties.append(CSSProperty(
        CSSPropertyApplyAtRule,
        *CSSCustomIdentValue::create(ident.value().toString())));
}
*/
    
RefPtr<StyleKeyframe> CSSParserImpl::consumeKeyframeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    std::unique_ptr<Vector<double>> keyList = consumeKeyframeKeyList(prelude);
    if (!keyList)
        return nullptr;

    if (m_observerWrapper) {
        m_observerWrapper->observer().startRuleHeader(StyleRule::Keyframe, m_observerWrapper->startOffset(prelude));
        m_observerWrapper->observer().endRuleHeader(m_observerWrapper->endOffset(prelude));
    }

    consumeDeclarationList(block, StyleRule::Keyframe);
    return StyleKeyframe::create(WTFMove(keyList), createStyleProperties(m_parsedProperties, m_context.mode));
}

static void observeSelectors(CSSParserObserverWrapper& wrapper, CSSParserTokenRange selectors)
{
    // This is easier than hooking into the CSSSelectorParser
    selectors.consumeWhitespace();
    CSSParserTokenRange originalRange = selectors;
    wrapper.observer().startRuleHeader(StyleRule::Style, wrapper.startOffset(originalRange));

    while (!selectors.atEnd()) {
        const CSSParserToken* selectorStart = &selectors.peek();
        while (!selectors.atEnd() && selectors.peek().type() != CommaToken)
            selectors.consumeComponentValue();
        CSSParserTokenRange selector = selectors.makeSubRange(selectorStart, &selectors.peek());
        selectors.consumeIncludingWhitespace();

        wrapper.observer().observeSelector(wrapper.startOffset(selector), wrapper.endOffset(selector));
    }

    wrapper.observer().endRuleHeader(wrapper.endOffset(originalRange));
}

RefPtr<StyleRule> CSSParserImpl::consumeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSSelectorList selectorList = CSSSelectorParser::parseSelector(prelude, m_context, m_styleSheet.get());
    if (!selectorList.isValid())
        return nullptr; // Parse error, invalid selector list

    if (m_observerWrapper)
        observeSelectors(*m_observerWrapper, prelude);

    consumeDeclarationList(block, StyleRule::Style);

    // FIXME-NEWPARSER: Line number is in the StyleRule constructor (gross), need to figure this out.
    RefPtr<StyleRule> rule = StyleRule::create(0, createStyleProperties(m_parsedProperties, m_context.mode));
    rule->wrapperAdoptSelectorList(selectorList);
    return rule;
}

void CSSParserImpl::consumeDeclarationList(CSSParserTokenRange range, StyleRule::Type ruleType)
{
    ASSERT(m_parsedProperties.isEmpty());

    bool useObserver = m_observerWrapper && (ruleType == StyleRule::Style || ruleType == StyleRule::Keyframe);
    if (useObserver) {
        m_observerWrapper->observer().startRuleBody(m_observerWrapper->previousTokenStartOffset(range));
        m_observerWrapper->skipCommentsBefore(range, true);
    }

    while (!range.atEnd()) {
        switch (range.peek().type()) {
        case WhitespaceToken:
        case SemicolonToken:
            range.consume();
            break;
        case IdentToken: {
            const CSSParserToken* declarationStart = &range.peek();

            if (useObserver)
                m_observerWrapper->yieldCommentsBefore(range);

            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();

            consumeDeclaration(range.makeSubRange(declarationStart, &range.peek()), ruleType);

            if (useObserver)
                m_observerWrapper->skipCommentsBefore(range, false);
            break;
        }
        case AtKeywordToken: {
            // FIXME-NEWPARSER: Support apply
            AllowedRulesType allowedRules = /* ruleType == StyleRule::Style && RuntimeEnabledFeatures::cssApplyAtRulesEnabled() ? ApplyRules :*/ NoRules;
            RefPtr<StyleRuleBase> rule = consumeAtRule(range, allowedRules);
            ASSERT_UNUSED(rule, !rule);
            break;
        }
        default: // Parse error, unexpected token in declaration list
            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();
            break;
        }
    }

    // Yield remaining comments
    if (useObserver) {
        m_observerWrapper->yieldCommentsBefore(range);
        m_observerWrapper->observer().endRuleBody(m_observerWrapper->endOffset(range));
    }
}

void CSSParserImpl::consumeDeclaration(CSSParserTokenRange range, StyleRule::Type ruleType)
{
    CSSParserTokenRange rangeCopy = range; // For inspector callbacks

    ASSERT(range.peek().type() == IdentToken);
    const CSSParserToken& token = range.consumeIncludingWhitespace();
    CSSPropertyID propertyID = token.parseAsCSSPropertyID();
    if (range.consume().type() != ColonToken)
        return; // Parse error

    bool important = false;
    const CSSParserToken* declarationValueEnd = range.end();
    const CSSParserToken* last = range.end() - 1;
    while (last->type() == WhitespaceToken)
        --last;
    if (last->type() == IdentToken && equalIgnoringASCIICase(last->value(), "important")) {
        --last;
        while (last->type() == WhitespaceToken)
            --last;
        if (last->type() == DelimiterToken && last->delimiter() == '!') {
            important = true;
            declarationValueEnd = last;
        }
    }

    size_t propertiesCount = m_parsedProperties.size();
    if (propertyID == CSSPropertyInvalid && CSSVariableParser::isValidVariableName(token)) {
        AtomicString variableName = token.value().toAtomicString();
        consumeVariableValue(range.makeSubRange(&range.peek(), declarationValueEnd), variableName, important);
    }

    if (important && (ruleType == StyleRule::FontFace || ruleType == StyleRule::Keyframe))
        return;

    if (propertyID != CSSPropertyInvalid)
        consumeDeclarationValue(range.makeSubRange(&range.peek(), declarationValueEnd), propertyID, important, ruleType);

    if (m_observerWrapper && (ruleType == StyleRule::Style || ruleType == StyleRule::Keyframe)) {
        m_observerWrapper->observer().observeProperty(
            m_observerWrapper->startOffset(rangeCopy), m_observerWrapper->endOffset(rangeCopy),
            important, m_parsedProperties.size() != propertiesCount);
    }
}

void CSSParserImpl::consumeVariableValue(CSSParserTokenRange range, const AtomicString& variableName, bool important)
{
    if (RefPtr<CSSCustomPropertyValue> value = CSSVariableParser::parseDeclarationValue(variableName, range))
        m_parsedProperties.append(CSSProperty(CSSPropertyCustom, WTFMove(value), important));
}

void CSSParserImpl::consumeDeclarationValue(CSSParserTokenRange range, CSSPropertyID propertyID, bool important, StyleRule::Type ruleType)
{
    CSSPropertyParser::parseValue(propertyID, important, range, m_context, m_parsedProperties, ruleType);
}

std::unique_ptr<Vector<double>> CSSParserImpl::consumeKeyframeKeyList(CSSParserTokenRange range)
{
    std::unique_ptr<Vector<double>> result = std::unique_ptr<Vector<double>>(new Vector<double>);
    while (true) {
        range.consumeWhitespace();
        const CSSParserToken& token = range.consumeIncludingWhitespace();
        if (token.type() == PercentageToken && token.numericValue() >= 0 && token.numericValue() <= 100)
            result->append(token.numericValue() / 100);
        else if (token.type() == IdentToken && equalIgnoringASCIICase(token.value(), "from"))
            result->append(0);
        else if (token.type() == IdentToken && equalIgnoringASCIICase(token.value(), "to"))
            result->append(1);
        else
            return nullptr; // Parser error, invalid value in keyframe selector
        if (range.atEnd())
            return result;
        if (range.consume().type() != CommaToken)
            return nullptr; // Parser error
    }
}

} // namespace WebCore
