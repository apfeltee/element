#include "element.h"

#include <stdarg.h>

using namespace std::string_literals;

namespace element
{
    void Logger::pushError(const std::string& errorMessage)
    {
        m_messages.emplace_back(errorMessage);
    }

    void Logger::pushError(int line, const std::string& errorMessage)
    {
        m_messages.emplace_back("line "s + std::to_string(line) + ": " + errorMessage);
    }

    void Logger::pushError(const Location& coords, const std::string& errorMessage)
    {
        m_messages.emplace_back("line "s + std::to_string(coords.line) + " column "s + std::to_string(coords.column)
                                    + "\n\t"s + errorMessage);
    }

    bool Logger::hasMessages() const
    {
        return !m_messages.empty();
    }

    std::string Logger::getCombined() const
    {
        std::string combined;

        for(auto it = m_messages.rbegin(); it != m_messages.rend(); ++it)
            combined += *it + "\n";

        return combined;
    }

    void Logger::clearMessages()
    {
        m_messages.clear();
    }

}// namespace element
