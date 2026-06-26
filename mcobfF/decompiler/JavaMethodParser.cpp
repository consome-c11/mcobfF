#include "JavaMethodParser.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace mcobfF
{
    bool JavaMethodParser::isIdentChar(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
    }

    int JavaMethodParser::getLineNumber(const std::string& source, size_t pos)
    {
        int line = 0;
        for (size_t i = 0; i < pos && i < source.size(); i++)
        {
            if (source[i] == '\n') line++;
        }
        return line;
    }

    size_t JavaMethodParser::skipBackwardWhitespaceAndComments(const std::string& source, size_t pos)
    {
        while (pos > 0)
        {
            char c = source[pos - 1];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
                pos--;
                continue;
            }
            break;
        }
        return pos;
    }

    std::string JavaMethodParser::getPreviousToken(const std::string& source, size_t pos)
    {
        pos = skipBackwardWhitespaceAndComments(source, pos);
        if (pos == 0) return "";

        size_t end = pos;
        char lastChar = source[pos - 1];
        if (lastChar == '{' || lastChar == '}' || lastChar == '(' || lastChar == ')' ||
            lastChar == ';' || lastChar == ',' || lastChar == '.' || lastChar == '@' ||
            lastChar == '<' || lastChar == '>' || lastChar == '[' || lastChar == ']')
        {
            return std::string(1, lastChar);
        }

        size_t start = pos;
        while (start > 0 && isIdentChar(source[start - 1]))
        {
            start--;
        }

        if (start == end)
        {
            return std::string(1, source[pos - 1]);
        }

        return source.substr(start, end - start);
    }

    // ──────────────────────────────────────────────────────────────────────
    // Find matching closing brace, respecting strings/comments
    // ──────────────────────────────────────────────────────────────────────
    size_t JavaMethodParser::findMatchingBrace(const std::string& source, size_t openBracePos)
    {
        int depth = 0;
        CharContext ctx = CharContext::Normal;

        for (size_t i = openBracePos; i < source.size(); i++)
        {
            char c = source[i];
            char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

            switch (ctx)
            {
            case CharContext::Normal:
                if (c == '/' && next == '/')
                {
                    ctx = CharContext::InLineComment;
                    i++;
                }
                else if (c == '/' && next == '*')
                {
                    ctx = CharContext::InBlockComment;
                    i++;
                }
                else if (c == '"')
                {
                    ctx = CharContext::InString;
                }
                else if (c == '\'')
                {
                    ctx = CharContext::InChar;
                }
                else if (c == '{')
                {
                    depth++;
                }
                else if (c == '}')
                {
                    depth--;
                    if (depth == 0) return i;
                }
                break;

            case CharContext::InLineComment:
                if (c == '\n') ctx = CharContext::Normal;
                break;

            case CharContext::InBlockComment:
                if (c == '*' && next == '/')
                {
                    ctx = CharContext::Normal;
                    i++;
                }
                break;

            case CharContext::InString:
                if (c == '\\')
                {
                    i++;
                }
                else if (c == '"')
                {
                    ctx = CharContext::Normal;
                }
                break;

            case CharContext::InChar:
                if (c == '\\')
                {
                    i++;
                }
                else if (c == '\'')
                {
                    ctx = CharContext::Normal;
                }
                break;
            }
        }

        return std::string::npos;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Extend backward to include annotations and Javadoc.
    // Uses a line-based approach for reliability.
    // ──────────────────────────────────────────────────────────────────────
    size_t JavaMethodParser::extendBackwardForAnnotations(const std::string& source, size_t methodDeclStart)
    {
        size_t lineStart = methodDeclStart;
        while (lineStart > 0 && source[lineStart - 1] != '\n') lineStart--;

        size_t earliestStart = lineStart;
        bool inMultilineAnnotation = false;
        int annotationParenDepth = 0;

        while (true)
        {
            size_t prevLineEnd = lineStart;
            if (prevLineEnd > 0 && source[prevLineEnd - 1] == '\n') prevLineEnd--;
            else if (prevLineEnd == 0) break;
            if (prevLineEnd == 0) break;

            size_t prevLineStart = prevLineEnd;
            while (prevLineStart > 0 && source[prevLineStart - 1] != '\n') prevLineStart--;

            std::string lineText = source.substr(prevLineStart, prevLineEnd - prevLineStart);

            size_t first = lineText.find_first_not_of(" \t\r\n");
            size_t last = lineText.find_last_not_of(" \t\r\n");
            std::string trimmed;
            if (first != std::string::npos)
            {
                trimmed = lineText.substr(first, last - first + 1);
            }

            if (trimmed.empty())
            {
                if (!inMultilineAnnotation) break;
                lineStart = prevLineStart;
                continue;
            }

            if (inMultilineAnnotation)
            {
                for (char c : trimmed)
                {
                    if (c == '(') annotationParenDepth++;
                    else if (c == ')') annotationParenDepth--;
                }
                earliestStart = prevLineStart;
                lineStart = prevLineStart;
                if (annotationParenDepth <= 0)
                {
                    inMultilineAnnotation = false;
                    annotationParenDepth = 0;
                }
                continue;
            }

            if (trimmed[0] == '@')
            {
                earliestStart = prevLineStart;
                lineStart = prevLineStart;

                annotationParenDepth = 0;
                for (char c : trimmed)
                {
                    if (c == '(') annotationParenDepth++;
                    else if (c == ')') annotationParenDepth--;
                }
                if (annotationParenDepth > 0)
                {
                    inMultilineAnnotation = true;
                }
                continue;
            }

            if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '*')
            {
                earliestStart = prevLineStart;
                break;
            }

            if (trimmed[0] == '*')
            {
                earliestStart = prevLineStart;
                lineStart = prevLineStart;
                continue;
            }

            if (trimmed.size() >= 2 &&
                trimmed[trimmed.size() - 2] == '*' && trimmed[trimmed.size() - 1] == '/')
            {
                earliestStart = prevLineStart;
                lineStart = prevLineStart;

                while (true)
                {
                    size_t prevEnd2 = lineStart;
                    if (prevEnd2 > 0 && source[prevEnd2 - 1] == '\n') prevEnd2--;
                    if (prevEnd2 == 0) break;

                    size_t prevStart2 = prevEnd2;
                    while (prevStart2 > 0 && source[prevStart2 - 1] != '\n') prevStart2--;

                    std::string lt2 = source.substr(prevStart2, prevEnd2 - prevStart2);
                    size_t f2 = lt2.find_first_not_of(" \t\r\n");
                    std::string tr2;
                    if (f2 != std::string::npos)
                    {
                        size_t l2 = lt2.find_last_not_of(" \t\r\n");
                        tr2 = lt2.substr(f2, l2 - f2 + 1);
                    }

                    earliestStart = prevStart2;
                    lineStart = prevStart2;

                    if (tr2.find("/*") != std::string::npos)
                    {
                        goto done_annotations;
                    }
                    if (tr2.empty()) break;
                }
                break;
            }

            break;
        }
    done_annotations:

        return earliestStart;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Count parameters in a JVM descriptor: "(II)V" -> 2, "(Ljava/lang/String;)V" -> 1
    // ──────────────────────────────────────────────────────────────────────
    int JavaMethodParser::parseDescriptorParamCount(const std::string& desc)
    {
        if (desc.empty() || desc[0] != '(') return -1;

        int count = 0;
        size_t i = 1;
        while (i < desc.size() && desc[i] != ')')
        {
            char c = desc[i];
            if (c == 'L')
            {
                while (i < desc.size() && desc[i] != ';') i++;
                count++;
            }
            else if (c == '[')
            {
                i++;
                continue;
            }
            else
            {
                count++;
            }
            i++;
        }
        return count;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Count parameters in source code parameter list
    // ──────────────────────────────────────────────────────────────────────
    int JavaMethodParser::countSourceParams(const std::string& source, size_t openParen, size_t closeParen)
    {
        if (closeParen <= openParen + 1) return 0;

        int count = 0;
        int depth = 0;
        int parenDepth = 0;
        CharContext ctx = CharContext::Normal;
        bool hasContent = false;

        for (size_t i = openParen + 1; i < closeParen; i++)
        {
            char c = source[i];
            char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

            switch (ctx)
            {
            case CharContext::Normal:
                if (c == '/' && next == '/')
                {
                    ctx = CharContext::InLineComment;
                    i++;
                }
                else if (c == '/' && next == '*')
                {
                    ctx = CharContext::InBlockComment;
                    i++;
                }
                else if (c == '"')
                {
                    ctx = CharContext::InString;
                }
                else if (c == '\'')
                {
                    ctx = CharContext::InChar;
                }
                else if (c == '<')
                {
                    depth++;
                }
                else if (c == '>')
                {
                    if (depth > 0) depth--;
                }
                else if (c == '(')
                {
                    parenDepth++;
                }
                else if (c == ')')
                {
                    if (parenDepth > 0) parenDepth--;
                }
                else if (c == ',' && depth == 0 && parenDepth == 0)
                {
                    count++;
                }
                else if (!std::isspace(static_cast<unsigned char>(c)))
                {
                    hasContent = true;
                }
                break;

            case CharContext::InLineComment:
                if (c == '\n') ctx = CharContext::Normal;
                break;

            case CharContext::InBlockComment:
                if (c == '*' && next == '/')
                {
                    ctx = CharContext::Normal;
                    i++;
                }
                break;

            case CharContext::InString:
                if (c == '\\') i++;
                else if (c == '"') ctx = CharContext::Normal;
                break;

            case CharContext::InChar:
                if (c == '\\') i++;
                else if (c == '\'') ctx = CharContext::Normal;
                break;
            }
        }

        if (hasContent) count++;
        return count;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Check if identifier is a method declaration (not a call or reference)
    // ──────────────────────────────────────────────────────────────────────
    bool JavaMethodParser::isMethodDeclaration(const std::string& source, size_t nameStart, size_t nameEnd)
    {
        if (nameEnd >= source.size()) return false;

        size_t afterName = nameEnd;
        while (afterName < source.size() &&
            std::isspace(static_cast<unsigned char>(source[afterName])))
        {
            afterName++;
        }
        if (afterName >= source.size() || source[afterName] != '(') return false;

        size_t pos = nameStart;
        while (pos > 0 && (source[pos - 1] == ' ' || source[pos - 1] == '\t' ||
            source[pos - 1] == '\r' || source[pos - 1] == '\n'))
        {
            pos--;
        }

        if (pos > 0)
        {
            char before = source[pos - 1];
            if (before == '.') return false;
            if (before == ',' || before == '(' || before == '=') return false;
        }

        {
            size_t scanPos = pos;
            std::string prevTok = getPreviousToken(source, nameStart);

            bool hasReturnType = false;

            if (!prevTok.empty())
            {
                if (prevTok == ">" || prevTok == "]")
                {
                    hasReturnType = true;
                }
                else if (prevTok == "new" || prevTok == "return" || prevTok == "throw")
                {
                    hasReturnType = false;
                }
                else if (std::isalpha(static_cast<unsigned char>(prevTok[0])) || prevTok[0] == '_')
                {
                    hasReturnType = true;
                }
            }

            if (!hasReturnType) return false;
        }

        size_t openParen = afterName;
        int depth = 0;
        size_t closeParen = std::string::npos;
        CharContext parenCtx = CharContext::Normal;

        for (size_t i = openParen; i < source.size(); i++)
        {
            char c = source[i];
            char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

            switch (parenCtx)
            {
            case CharContext::Normal:
                if (c == '/' && next == '/')
                {
                    parenCtx = CharContext::InLineComment;
                    i++;
                }
                else if (c == '/' && next == '*')
                {
                    parenCtx = CharContext::InBlockComment;
                    i++;
                }
                else if (c == '"')
                {
                    parenCtx = CharContext::InString;
                }
                else if (c == '\'')
                {
                    parenCtx = CharContext::InChar;
                }
                else if (c == '(')
                {
                    depth++;
                }
                else if (c == ')')
                {
                    depth--;
                    if (depth == 0)
                    {
                        closeParen = i;
                        goto found_cp;
                    }
                }
                break;
            case CharContext::InLineComment:
                if (c == '\n') parenCtx = CharContext::Normal;
                break;
            case CharContext::InBlockComment:
                if (c == '*' && next == '/')
                {
                    parenCtx = CharContext::Normal;
                    i++;
                }
                break;
            case CharContext::InString:
                if (c == '\\') i++;
                else if (c == '"') parenCtx = CharContext::Normal;
                break;
            case CharContext::InChar:
                if (c == '\\') i++;
                else if (c == '\'') parenCtx = CharContext::Normal;
                break;
            }
        }
    found_cp:

        if (closeParen == std::string::npos) return false;

        size_t after = closeParen + 1;
        CharContext afterCtx = CharContext::Normal;

        for (size_t i = after; i < source.size(); i++)
        {
            char c = source[i];
            char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

            switch (afterCtx)
            {
            case CharContext::Normal:
                if (c == '/' && next == '/')
                {
                    afterCtx = CharContext::InLineComment;
                    i++;
                }
                else if (c == '/' && next == '*')
                {
                    afterCtx = CharContext::InBlockComment;
                    i++;
                }
                else if (c == '"')
                {
                    afterCtx = CharContext::InString;
                }
                else if (c == '\'')
                {
                    afterCtx = CharContext::InChar;
                }
                else if (std::isspace(static_cast<unsigned char>(c)))
                {
                    // skip
                }
                else
                {
                    if (c == '{' || c == ';') return true;

                    if (c == 't' && i + 6 <= source.size() && source.substr(i, 6) == "throws" &&
                        (i + 6 >= source.size() || !isIdentChar(source[i + 6])))
                    {
                        return true;
                    }

                    if (c == 'd' && i + 7 <= source.size() && source.substr(i, 7) == "default" &&
                        (i + 7 >= source.size() || !isIdentChar(source[i + 7])))
                    {
                        return true;
                    }

                    return false;
                }
                break;
            case CharContext::InLineComment:
                if (c == '\n') afterCtx = CharContext::Normal;
                break;
            case CharContext::InBlockComment:
                if (c == '*' && next == '/')
                {
                    afterCtx = CharContext::Normal;
                    i++;
                }
                break;
            case CharContext::InString:
                if (c == '\\') i++;
                else if (c == '"') afterCtx = CharContext::Normal;
                break;
            case CharContext::InChar:
                if (c == '\\') i++;
                else if (c == '\'') afterCtx = CharContext::Normal;
                break;
            }
        }

        return false;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Find the opening '{' of the method body after the parameter list
    // ──────────────────────────────────────────────────────────────────────
    size_t JavaMethodParser::findBodyOpenBrace(const std::string& source, size_t closeParenPos)
    {
        CharContext ctx = CharContext::Normal;

        for (size_t i = closeParenPos + 1; i < source.size(); i++)
        {
            char c = source[i];
            char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

            switch (ctx)
            {
            case CharContext::Normal:
                if (c == '/' && next == '/')
                {
                    ctx = CharContext::InLineComment;
                    i++;
                }
                else if (c == '/' && next == '*')
                {
                    ctx = CharContext::InBlockComment;
                    i++;
                }
                else if (c == '"')
                {
                    ctx = CharContext::InString;
                }
                else if (c == '\'')
                {
                    ctx = CharContext::InChar;
                }
                else if (c == '{')
                {
                    return i; // Found body start
                }
                else if (c == ';')
                {
                    return std::string::npos;
                }
                else if (c == 't' && i + 5 < source.size())
                {
                    if (source.substr(i, 6) == "throws" && !isIdentChar(source[i + 6]))
                    {
                        i += 5;
                    }
                }
                break;
            case CharContext::InLineComment:
                if (c == '\n') ctx = CharContext::Normal;
                break;
            case CharContext::InBlockComment:
                if (c == '*' && next == '/')
                {
                    ctx = CharContext::Normal;
                    i++;
                }
                break;
            case CharContext::InString:
                if (c == '\\') i++;
                else if (c == '"') ctx = CharContext::Normal;
                break;
            case CharContext::InChar:
                if (c == '\\') i++;
                else if (c == '\'') ctx = CharContext::Normal;
                break;
            }
        }

        return std::string::npos;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Find all methods in the source
    // ──────────────────────────────────────────────────────────────────────
    std::vector<MethodSpan> JavaMethodParser::findAllMethods(
        const std::string& source,
        const std::string& className)
    {
        std::vector<MethodSpan> methods;

        std::string simpleName;
        if (!className.empty())
        {
            simpleName = getSimpleClassName(className);
        }

        CharContext ctx = CharContext::Normal;

        for (size_t i = 0; i < source.size(); i++)
        {
            char c = source[i];
            char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

            switch (ctx)
            {
            case CharContext::Normal:
                if (c == '/' && next == '/')
                {
                    ctx = CharContext::InLineComment;
                    i++;
                }
                else if (c == '/' && next == '*')
                {
                    ctx = CharContext::InBlockComment;
                    i++;
                }
                else if (c == '"')
                {
                    ctx = CharContext::InString;
                }
                else if (c == '\'')
                {
                    ctx = CharContext::InChar;
                }
                else if (isIdentChar(c))
                {
                    size_t nameStart = i;
                    while (i < source.size() && isIdentChar(source[i])) i++;
                    size_t nameEnd = i;
                    i--;

                    std::string ident = source.substr(nameStart, nameEnd - nameStart);

                    static const std::vector<std::string> keywords = {
                        "if", "for", "while", "switch", "catch", "synchronized",
                        "new", "return", "class", "interface", "enum", "extends",
                        "implements", "import", "package", "try", "else", "do",
                        "case", "throw", "assert", "super", "this", "instanceof",
                        "void", "int", "long", "double", "float", "boolean",
                        "byte", "char", "short", "public", "private", "protected",
                        "static", "final", "abstract", "native", "strictfp", "transient",
                        "volatile", "default", "record", "sealed", "non-sealed",
                        "permits", "var", "yield"
                    };
                    bool isKeyword = false;
                    for (const auto& kw : keywords)
                    {
                        if (ident == kw)
                        {
                            isKeyword = true;
                            break;
                        }
                    }
                    if (isKeyword) continue;

                    size_t afterIdent = nameEnd;
                    while (afterIdent < source.size() &&
                        std::isspace(static_cast<unsigned char>(source[afterIdent])))
                    {
                        afterIdent++;
                    }

                    if (afterIdent < source.size() && source[afterIdent] == '(')
                    {
                        if (isMethodDeclaration(source, nameStart, nameEnd))
                        {
                            size_t openParen = afterIdent;
                            int depth = 0;
                            size_t closeParen = std::string::npos;
                            CharContext parenCtx = CharContext::Normal;

                            for (size_t j = openParen; j < source.size(); j++)
                            {
                                char pc = source[j];
                                char pn = (j + 1 < source.size()) ? source[j + 1] : '\0';

                                switch (parenCtx)
                                {
                                case CharContext::Normal:
                                    if (pc == '/' && pn == '/')
                                    {
                                        parenCtx = CharContext::InLineComment;
                                        j++;
                                    }
                                    else if (pc == '/' && pn == '*')
                                    {
                                        parenCtx = CharContext::InBlockComment;
                                        j++;
                                    }
                                    else if (pc == '"') parenCtx = CharContext::InString;
                                    else if (pc == '\'') parenCtx = CharContext::InChar;
                                    else if (pc == '(') depth++;
                                    else if (pc == ')')
                                    {
                                        depth--;
                                        if (depth == 0)
                                        {
                                            closeParen = j;
                                            goto found_cp_all;
                                        }
                                    }
                                    break;
                                case CharContext::InLineComment:
                                    if (pc == '\n') parenCtx = CharContext::Normal;
                                    break;
                                case CharContext::InBlockComment:
                                    if (pc == '*' && pn == '/')
                                    {
                                        parenCtx = CharContext::Normal;
                                        j++;
                                    }
                                    break;
                                case CharContext::InString:
                                    if (pc == '\\') j++;
                                    else if (pc == '"') parenCtx = CharContext::Normal;
                                    break;
                                case CharContext::InChar:
                                    if (pc == '\\') j++;
                                    else if (pc == '\'') parenCtx = CharContext::Normal;
                                    break;
                                }
                            }
                        found_cp_all:

                            if (closeParen != std::string::npos)
                            {
                                size_t bodyBrace = findBodyOpenBrace(source, closeParen);

                                MethodSpan span;
                                span.name = ident;

                                if (!simpleName.empty() && ident == simpleName)
                                {
                                    span.name = "<init>";
                                }

                                if (bodyBrace != std::string::npos)
                                {
                                    size_t closingBrace = findMatchingBrace(source, bodyBrace);
                                    if (closingBrace != std::string::npos)
                                    {
                                        size_t annotStart = extendBackwardForAnnotations(source, nameStart);
                                        span.startPos = annotStart;
                                        span.endPos = closingBrace + 1;
                                        span.startLine = getLineNumber(source, annotStart);
                                        span.endLine = getLineNumber(source, closingBrace);
                                        methods.push_back(span);
                                        i = closingBrace;
                                    }
                                }
                                else
                                {
                                    size_t semiPos = std::string::npos;
                                    CharContext semiCtx = CharContext::Normal;
                                    for (size_t j = closeParen + 1; j < source.size(); j++)
                                    {
                                        char sc = source[j];
                                        char sn = (j + 1 < source.size()) ? source[j + 1] : '\0';
                                        switch (semiCtx)
                                        {
                                        case CharContext::Normal:
                                            if (sc == '/' && sn == '/')
                                            {
                                                semiCtx = CharContext::InLineComment;
                                                j++;
                                            }
                                            else if (sc == '/' && sn == '*')
                                            {
                                                semiCtx = CharContext::InBlockComment;
                                                j++;
                                            }
                                            else if (sc == '"') semiCtx = CharContext::InString;
                                            else if (sc == '\'') semiCtx = CharContext::InChar;
                                            else if (sc == ';')
                                            {
                                                semiPos = j;
                                                goto found_semi_all;
                                            }
                                            break;
                                        case CharContext::InLineComment:
                                            if (sc == '\n') semiCtx = CharContext::Normal;
                                            break;
                                        case CharContext::InBlockComment:
                                            if (sc == '*' && sn == '/')
                                            {
                                                semiCtx = CharContext::Normal;
                                                j++;
                                            }
                                            break;
                                        case CharContext::InString:
                                            if (sc == '\\') j++;
                                            else if (sc == '"') semiCtx = CharContext::Normal;
                                            break;
                                        case CharContext::InChar:
                                            if (sc == '\\') j++;
                                            else if (sc == '\'') semiCtx = CharContext::Normal;
                                            break;
                                        }
                                    }
                                found_semi_all:
                                    if (semiPos != std::string::npos)
                                    {
                                        size_t annotStart = extendBackwardForAnnotations(source, nameStart);
                                        span.startPos = annotStart;
                                        span.endPos = semiPos + 1;
                                        span.startLine = getLineNumber(source, annotStart);
                                        span.endLine = getLineNumber(source, semiPos);
                                        methods.push_back(span);
                                        i = semiPos;
                                    }
                                }
                            }
                        }
                    }
                }
                break;

            case CharContext::InLineComment:
                if (c == '\n') ctx = CharContext::Normal;
                break;

            case CharContext::InBlockComment:
                if (c == '*' && next == '/')
                {
                    ctx = CharContext::Normal;
                    i++;
                }
                break;

            case CharContext::InString:
                if (c == '\\') i++;
                else if (c == '"') ctx = CharContext::Normal;
                break;

            case CharContext::InChar:
                if (c == '\\') i++;
                else if (c == '\'') ctx = CharContext::Normal;
                break;
            }
        }

        auto clinitSource = extractStaticInitializer(source);
        if (clinitSource)
        {
            MethodSpan span;
            span.name = "<clinit>";
            size_t startOfBlock = source.find("static", 0);
            if (startOfBlock != std::string::npos)
            {
                span.startPos = startOfBlock;
                size_t bracePos = source.find('{', startOfBlock);
                if (bracePos != std::string::npos)
                {
                    size_t closeBrace = findMatchingBrace(source, bracePos);
                    span.endPos = (closeBrace != std::string::npos) ? closeBrace + 1 : source.size();
                }
                else
                {
                    span.endPos = source.size();
                }
                span.startLine = getLineNumber(source, span.startPos);
                span.endLine = getLineNumber(source, span.endPos);
            }
            methods.push_back(span);
        }

        return methods;
    }

    std::string JavaMethodParser::getSimpleClassName(const std::string& className)
    {
        std::string name = className;
        size_t lastSlash = name.rfind('/');
        if (lastSlash != std::string::npos)
        {
            name = name.substr(lastSlash + 1);
        }
        size_t lastDot = name.rfind('.');
        if (lastDot != std::string::npos)
        {
            name = name.substr(lastDot + 1);
        }
        size_t lastDollar = name.rfind('$');
        if (lastDollar != std::string::npos)
        {
            name = name.substr(lastDollar + 1);
        }
        return name;
    }

    std::optional<std::string> JavaMethodParser::extractConstructor(
        const std::string& source,
        const std::string& simpleName,
        const std::string& jvmDescriptor)
    {
        if (source.empty() || simpleName.empty()) return std::nullopt;

        int expectedParamCount = -1;
        if (!jvmDescriptor.empty())
        {
            expectedParamCount = parseDescriptorParamCount(jvmDescriptor);
        }

        static const std::vector<std::string> ctorModifiers = {
            "public", "private", "protected", "static", "final",
            "abstract", "native", "synchronized", "strictfp"
        };

        size_t searchPos = 0;
        while (searchPos < source.size())
        {
            size_t namePos = source.find(simpleName, searchPos);
            if (namePos == std::string::npos) break;

            bool wordStart = (namePos == 0 || !isIdentChar(source[namePos - 1]));
            size_t nameEnd = namePos + simpleName.size();
            bool wordEnd = (nameEnd >= source.size() || !isIdentChar(source[nameEnd]));

            if (!wordStart || !wordEnd)
            {
                searchPos = nameEnd;
                continue;
            }

            size_t afterName = nameEnd;
            while (afterName < source.size() &&
                std::isspace(static_cast<unsigned char>(source[afterName])))
            {
                afterName++;
            }
            if (afterName >= source.size() || source[afterName] != '(')
            {
                searchPos = nameEnd;
                continue;
            }

            std::string prevTok = getPreviousToken(source, namePos);

            if (prevTok == "record") {
                searchPos = nameEnd;
                continue;
            }

            if (prevTok == "new")
            {
                searchPos = nameEnd;
                continue;
            }

            bool isModifier = false;
            for (const auto& mod : ctorModifiers)
            {
                if (prevTok == mod)
                {
                    isModifier = true;
                    break;
                }
            }

            if (!isModifier && !prevTok.empty())
            {
                if (std::isalpha(static_cast<unsigned char>(prevTok[0])) || prevTok[0] == '_')
                {
                    searchPos = nameEnd;
                    continue;
                }
            }

            size_t openParen = afterName;
            int depth = 0;
            size_t closeParen = std::string::npos;
            CharContext parenCtx = CharContext::Normal;

            for (size_t j = openParen; j < source.size(); j++)
            {
                char pc = source[j];
                char pn = (j + 1 < source.size()) ? source[j + 1] : '\0';

                switch (parenCtx)
                {
                case CharContext::Normal:
                    if (pc == '/' && pn == '/')
                    {
                        parenCtx = CharContext::InLineComment;
                        j++;
                    }
                    else if (pc == '/' && pn == '*')
                    {
                        parenCtx = CharContext::InBlockComment;
                        j++;
                    }
                    else if (pc == '"') parenCtx = CharContext::InString;
                    else if (pc == '\'') parenCtx = CharContext::InChar;
                    else if (pc == '(') depth++;
                    else if (pc == ')')
                    {
                        depth--;
                        if (depth == 0)
                        {
                            closeParen = j;
                            goto found_cp_ctor;
                        }
                    }
                    break;
                case CharContext::InLineComment:
                    if (pc == '\n') parenCtx = CharContext::Normal;
                    break;
                case CharContext::InBlockComment:
                    if (pc == '*' && pn == '/')
                    {
                        parenCtx = CharContext::Normal;
                        j++;
                    }
                    break;
                case CharContext::InString:
                    if (pc == '\\') j++;
                    else if (pc == '"') parenCtx = CharContext::Normal;
                    break;
                case CharContext::InChar:
                    if (pc == '\\') j++;
                    else if (pc == '\'') parenCtx = CharContext::Normal;
                    break;
                }
            }
        found_cp_ctor:

            if (closeParen != std::string::npos)
            {
                if (expectedParamCount >= 0)
                {
                    int paramCount = countSourceParams(source, openParen, closeParen);
                    if (paramCount != expectedParamCount)
                    {
                        searchPos = nameEnd;
                        continue;
                    }
                }

                size_t bodyBrace = findBodyOpenBrace(source, closeParen);

                if (bodyBrace != std::string::npos)
                {
                    size_t closingBrace = findMatchingBrace(source, bodyBrace);
                    if (closingBrace != std::string::npos)
                    {
                        size_t annotStart = extendBackwardForAnnotations(source, namePos);
                        return source.substr(annotStart, closingBrace + 1 - annotStart);
                    }
                }
                else
                {
                    size_t semiPos = std::string::npos;
                    CharContext semiCtx = CharContext::Normal;
                    for (size_t j = closeParen + 1; j < source.size(); j++)
                    {
                        char sc = source[j];
                        char sn = (j + 1 < source.size()) ? source[j + 1] : '\0';
                        switch (semiCtx)
                        {
                        case CharContext::Normal:
                            if (sc == '/' && sn == '/')
                            {
                                semiCtx = CharContext::InLineComment;
                                j++;
                            }
                            else if (sc == '/' && sn == '*')
                            {
                                semiCtx = CharContext::InBlockComment;
                                j++;
                            }
                            else if (sc == '"') semiCtx = CharContext::InString;
                            else if (sc == '\'') semiCtx = CharContext::InChar;
                            else if (sc == ';')
                            {
                                semiPos = j;
                                goto found_semi_ctor;
                            }
                            break;
                        case CharContext::InLineComment:
                            if (sc == '\n') semiCtx = CharContext::Normal;
                            break;
                        case CharContext::InBlockComment:
                            if (sc == '*' && sn == '/')
                            {
                                semiCtx = CharContext::Normal;
                                j++;
                            }
                            break;
                        case CharContext::InString:
                            if (sc == '\\') j++;
                            else if (sc == '"') semiCtx = CharContext::Normal;
                            break;
                        case CharContext::InChar:
                            if (sc == '\\') j++;
                            else if (sc == '\'') semiCtx = CharContext::Normal;
                            break;
                        }
                    }
                found_semi_ctor:
                    if (semiPos != std::string::npos)
                    {
                        size_t annotStart = extendBackwardForAnnotations(source, namePos);
                        return source.substr(annotStart, semiPos + 1 - annotStart);
                    }
                }
            }

            searchPos = nameEnd;
        }

        return std::nullopt;
    }

    std::optional<std::string> JavaMethodParser::extractStaticInitializer(const std::string& source)
    {
        CharContext ctx = CharContext::Normal;

        for (size_t i = 0; i < source.size(); i++)
        {
            char c = source[i];
            char next = (i + 1 < source.size()) ? source[i + 1] : '\0';

            switch (ctx)
            {
            case CharContext::Normal:
                if (c == '/' && next == '/')
                {
                    ctx = CharContext::InLineComment;
                    i++;
                }
                else if (c == '/' && next == '*')
                {
                    ctx = CharContext::InBlockComment;
                    i++;
                }
                else if (c == '"')
                {
                    ctx = CharContext::InString;
                }
                else if (c == '\'')
                {
                    ctx = CharContext::InChar;
                }
                else if (c == 's' && i + 6 <= source.size() && source.substr(i, 6) == "static")
                {
                    bool wordStart = (i == 0 || !isIdentChar(source[i - 1]));
                    size_t staticEnd = i + 6;
                    bool wordEnd = (staticEnd >= source.size() || !isIdentChar(source[staticEnd]));

                    if (wordStart && wordEnd)
                    {
                        size_t j = staticEnd;
                        CharContext afterCtx = CharContext::Normal;

                        while (j < source.size())
                        {
                            char ac = source[j];
                            char an = (j + 1 < source.size()) ? source[j + 1] : '\0';

                            switch (afterCtx)
                            {
                            case CharContext::Normal:
                                if (ac == '/' && an == '/')
                                {
                                    afterCtx = CharContext::InLineComment;
                                    j++;
                                }
                                else if (ac == '/' && an == '*')
                                {
                                    afterCtx = CharContext::InBlockComment;
                                    j++;
                                }
                                else if (std::isspace(static_cast<unsigned char>(ac)))
                                {
                                    // skip whitespace
                                }
                                else if (ac == '{')
                                {
                                    size_t closeBrace = findMatchingBrace(source, j);
                                    if (closeBrace != std::string::npos)
                                    {
                                        size_t annotStart = extendBackwardForAnnotations(source, i);
                                        return source.substr(annotStart, closeBrace + 1 - annotStart);
                                    }
                                    goto next_static;
                                }
                                else
                                {
                                    goto next_static;
                                }
                                break;
                            case CharContext::InLineComment:
                                if (ac == '\n') afterCtx = CharContext::Normal;
                                break;
                            case CharContext::InBlockComment:
                                if (ac == '*' && an == '/')
                                {
                                    afterCtx = CharContext::Normal;
                                    j++;
                                }
                                break;
                            }
                            j++;
                        }
                    next_static:
                        i = staticEnd - 1;
                    }
                }
                break;

            case CharContext::InLineComment:
                if (c == '\n') ctx = CharContext::Normal;
                break;

            case CharContext::InBlockComment:
                if (c == '*' && next == '/')
                {
                    ctx = CharContext::Normal;
                    i++;
                }
                break;

            case CharContext::InString:
                if (c == '\\') i++;
                else if (c == '"') ctx = CharContext::Normal;
                break;

            case CharContext::InChar:
                if (c == '\\') i++;
                else if (c == '\'') ctx = CharContext::Normal;
                break;
            }
        }

        return std::nullopt;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Extract method source by name, with optional descriptor disambiguation
    // ──────────────────────────────────────────────────────────────────────
    std::optional<std::string> JavaMethodParser::extractMethod(
        const std::string& source,
        const std::string& methodName,
        const std::string& jvmDescriptor,
        const std::string& className)
    {
        if (source.empty() || methodName.empty()) return std::nullopt;

        if (methodName == "<init>")
        {
            if (className.empty()) return std::nullopt;
            std::string simpleName = getSimpleClassName(className);
            return extractConstructor(source, simpleName, jvmDescriptor);
        }

        if (methodName == "<clinit>")
        {
            return extractStaticInitializer(source);
        }

        int expectedParamCount = -1;
        if (!jvmDescriptor.empty())
        {
            expectedParamCount = parseDescriptorParamCount(jvmDescriptor);
        }

        size_t searchPos = 0;
        while (searchPos < source.size())
        {
            size_t namePos = source.find(methodName, searchPos);
            if (namePos == std::string::npos) break;

            bool wordStart = (namePos == 0 || !isIdentChar(source[namePos - 1]));
            size_t nameEnd = namePos + methodName.size();
            bool wordEnd = (nameEnd >= source.size() || !isIdentChar(source[nameEnd]));

            if (wordStart && wordEnd && isMethodDeclaration(source, namePos, nameEnd))
            {
                size_t afterName = nameEnd;
                while (afterName < source.size() &&
                    std::isspace(static_cast<unsigned char>(source[afterName])))
                {
                    afterName++;
                }

                if (afterName < source.size() && source[afterName] == '(')
                {
                    size_t openParen = afterName;

                    int depth = 0;
                    size_t closeParen = std::string::npos;
                    CharContext parenCtx = CharContext::Normal;

                    for (size_t j = openParen; j < source.size(); j++)
                    {
                        char pc = source[j];
                        char pn = (j + 1 < source.size()) ? source[j + 1] : '\0';

                        switch (parenCtx)
                        {
                        case CharContext::Normal:
                            if (pc == '/' && pn == '/')
                            {
                                parenCtx = CharContext::InLineComment;
                                j++;
                            }
                            else if (pc == '/' && pn == '*')
                            {
                                parenCtx = CharContext::InBlockComment;
                                j++;
                            }
                            else if (pc == '"') parenCtx = CharContext::InString;
                            else if (pc == '\'') parenCtx = CharContext::InChar;
                            else if (pc == '(') depth++;
                            else if (pc == ')')
                            {
                                depth--;
                                if (depth == 0)
                                {
                                    closeParen = j;
                                    goto found_cp_ext;
                                }
                            }
                            break;
                        case CharContext::InLineComment:
                            if (pc == '\n') parenCtx = CharContext::Normal;
                            break;
                        case CharContext::InBlockComment:
                            if (pc == '*' && pn == '/')
                            {
                                parenCtx = CharContext::Normal;
                                j++;
                            }
                            break;
                        case CharContext::InString:
                            if (pc == '\\') j++;
                            else if (pc == '"') parenCtx = CharContext::Normal;
                            break;
                        case CharContext::InChar:
                            if (pc == '\\') j++;
                            else if (pc == '\'') parenCtx = CharContext::Normal;
                            break;
                        }
                    }
                found_cp_ext:

                    if (closeParen != std::string::npos)
                    {
                        if (expectedParamCount >= 0)
                        {
                            int paramCount = countSourceParams(source, openParen, closeParen);
                            if (paramCount != expectedParamCount)
                            {
                                searchPos = nameEnd;
                                continue;
                            }
                        }

                        size_t bodyBrace = findBodyOpenBrace(source, closeParen);

                        if (bodyBrace != std::string::npos)
                        {
                            size_t closingBrace = findMatchingBrace(source, bodyBrace);
                            if (closingBrace != std::string::npos)
                            {
                                size_t annotStart = extendBackwardForAnnotations(source, namePos);
                                return source.substr(annotStart, closingBrace + 1 - annotStart);
                            }
                        }
                        else
                        {
                            size_t semiPos = std::string::npos;
                            CharContext semiCtx = CharContext::Normal;
                            for (size_t j = closeParen + 1; j < source.size(); j++)
                            {
                                char sc = source[j];
                                char sn = (j + 1 < source.size()) ? source[j + 1] : '\0';
                                switch (semiCtx)
                                {
                                case CharContext::Normal:
                                    if (sc == '/' && sn == '/')
                                    {
                                        semiCtx = CharContext::InLineComment;
                                        j++;
                                    }
                                    else if (sc == '/' && sn == '*')
                                    {
                                        semiCtx = CharContext::InBlockComment;
                                        j++;
                                    }
                                    else if (sc == '"') semiCtx = CharContext::InString;
                                    else if (sc == '\'') semiCtx = CharContext::InChar;
                                    else if (sc == ';')
                                    {
                                        semiPos = j;
                                        goto found_semi_ext;
                                    }
                                    break;
                                case CharContext::InLineComment:
                                    if (sc == '\n') semiCtx = CharContext::Normal;
                                    break;
                                case CharContext::InBlockComment:
                                    if (sc == '*' && sn == '/')
                                    {
                                        semiCtx = CharContext::Normal;
                                        j++;
                                    }
                                    break;
                                case CharContext::InString:
                                    if (sc == '\\') j++;
                                    else if (sc == '"') semiCtx = CharContext::Normal;
                                    break;
                                case CharContext::InChar:
                                    if (sc == '\\') j++;
                                    else if (sc == '\'') semiCtx = CharContext::Normal;
                                    break;
                                }
                            }
                        found_semi_ext:
                            if (semiPos != std::string::npos)
                            {
                                size_t annotStart = extendBackwardForAnnotations(source, namePos);
                                return source.substr(annotStart, semiPos + 1 - annotStart);
                            }
                        }
                    }
                }
            }

            searchPos = nameEnd;
        }

        return std::nullopt;
    }
}
