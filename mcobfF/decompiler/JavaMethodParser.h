#pragma once

#include <string>
#include <optional>
#include <vector>

namespace mcobfF
{
    struct MethodSpan
    {
        std::string name;
        size_t startPos = 0;
        size_t endPos = 0;
        int startLine = 0;
        int endLine = 0;
    };

    class JavaMethodParser
    {
    public:
        /// Extract complete method source (including @Override and other annotations)
        /// by method name. If jvmDescriptor is provided, it is used to disambiguate
        /// overloaded methods by parameter count.
        /// For "<init>", className is required to locate the constructor.
        /// For "<clinit>", the static initializer block is extracted.
        static std::optional<std::string> extractMethod(
            const std::string& source,
            const std::string& methodName,
            const std::string& jvmDescriptor = "",
            const std::string& className = "");

        /// Find all method declarations in the source, including constructors and static initializers.
        /// className is used to identify constructors (named after the class).
        static std::vector<MethodSpan> findAllMethods(
            const std::string& source,
            const std::string& className = "");

    private:
        enum class CharContext
        {
            Normal,
            InString,
            InChar,
            InLineComment,
            InBlockComment
        };

        /// Find the matching closing brace, respecting strings/comments.
        /// openBracePos must point to the '{' character.
        static size_t findMatchingBrace(const std::string& source, size_t openBracePos);

        /// Walk backward from methodDeclStart to include preceding annotations,
        /// Javadoc, and blank lines between them.
        static size_t extendBackwardForAnnotations(const std::string& source, size_t methodDeclStart);

        /// Check if the identifier at [nameStart, nameEnd) followed by '(' looks
        /// like a method declaration rather than a method call or reference.
        static bool isMethodDeclaration(const std::string& source, size_t nameStart, size_t nameEnd);

        /// Count the number of parameters from a JVM descriptor like "(II)V"
        static int parseDescriptorParamCount(const std::string& desc);

        /// Count the number of parameters in source code between '(' and ')',
        /// respecting strings, comments, and nested generics.
        static int countSourceParams(const std::string& source, size_t openParen, size_t closeParen);

        /// Get the line number (0-based) for a byte offset
        static int getLineNumber(const std::string& source, size_t pos);

        /// Find the opening '{' of the method body after the parameter list
        static size_t findBodyOpenBrace(const std::string& source, size_t closeParenPos);

        /// Extract the text of the previous token before a position, skipping whitespace and comments
        static std::string getPreviousToken(const std::string& source, size_t pos);

        /// Check if a character is valid in a Java identifier
        static bool isIdentChar(char c);

        /// Skip backward over whitespace and comments, return new position
        static size_t skipBackwardWhitespaceAndComments(const std::string& source, size_t pos);

        /// Extract simple class name from fully qualified name (e.g. "net/minecraft/World" -> "World")
        /// For inner classes like "Outer$Inner", returns "Inner".
        static std::string getSimpleClassName(const std::string& className);

        /// Extract a constructor by simple class name and descriptor
        static std::optional<std::string> extractConstructor(
            const std::string& source,
            const std::string& simpleName,
            const std::string& jvmDescriptor);

        /// Extract a static initializer block "static { ... }"
        static std::optional<std::string> extractStaticInitializer(const std::string& source);
    };
}