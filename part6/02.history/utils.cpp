//
// Created by fld on 08.12.23.
//

#include "utils.h"

#include <sstream>

std::vector<std::string> splitStringByWhitespace(const std::string& input)
{
    std::vector<std::string> result;
    std::istringstream iss(input);
    std::string token;

    while (iss >> token)
    {
        result.push_back(token);
    }

    return result;
}

std::vector<std::string> splitStringWithDelimiter(const std::string& input, const std::string& delimiter, bool keepEmpty)
{
    std::vector<std::string> result;
    if(input.empty())
    {
        return result;
    }
    size_t start = 0;
    size_t end = input.find(delimiter);

    while (end != std::string::npos)
    {
        std::string token = input.substr(start, end - start);
        if (keepEmpty || !token.empty())
            result.push_back(token);
        start = end + delimiter.length();
        end = input.find(delimiter, start);
    }

    const std::string lastToken = input.substr(start);
    if (keepEmpty || !lastToken.empty())
        result.push_back(lastToken);

    return result;
}

std::vector<std::string> extractWordsAfterSequence(std::string& input, const std::string& sequence)
{
    std::vector<std::string> result;
    size_t pos = 0;
    while ((pos = input.find(sequence)) != std::string::npos)
    {
        size_t end = pos + sequence.length();
        while (end < input.length() && std::iswspace( input[end] )) end++;
        size_t wordStart = end;
        while (wordStart < input.length() && !std::iswspace(input[wordStart]) && input.substr(wordStart, sequence.length()) != sequence)
        {
            wordStart++;
        }
        std::string word = input.substr(end, wordStart - end);
        result.push_back(word);
        input.erase(pos, wordStart - pos);
    }
    return result;
}

bool unquotedCharacter(const std::string& input, char character, const std::set<char>& quoteCharacters)
{
    bool insideQuotes = false;
    for (const char c : input) {
        if (quoteCharacters.contains(c))
        {
            insideQuotes = !insideQuotes;
        }
        else
        if (c == character && !insideQuotes)
        {
            return true;
        }
    }
    return false;
}

