//
// Created by fld on 08.12.23.
//

#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <set>
#include <cstring>

#define STRIP_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define log_err        std::cerr << STRIP_FILENAME << ":" << __LINE__ << " (" << __FUNCTION__ << ")"
#define closeFd(x)    { close(x); }

/**
 * @brief splitStringByWhitespace will split the given string by whitespace
 *
 * @param input the string to split
 *
 * @return a vector of strings
 */
std::vector<std::string> splitStringByWhitespace(const std::string& input);

/**
 * @brief Will split the given string with the given delimiter, will return a vector of the elements without the splitter
 *
 * @param input The string to split
 * @param delimiter The delimiter to split the string at
 *
 * @return The tokens as found in the string, without the delimiter
 */
std::vector<std::string> splitStringWithDelimiter(const std::string& input, const std::string& delimiter, bool keepEmpty = false);

/**
 * @brief extractWordsAfterSequence This will extract the words from an input string that come after a specific sequence
 *
 * The function will modify the input variable, by removing the sequence and the extracted word from it.
 *
 * @param input
 * @param sequence
 *
 * @return the words that were extracted after a specific sequence
 */
std::vector<std::string> extractWordsAfterSequence(std::string& input, const std::string& sequence);


/**
 * @brief will check if the input looks like a strings which is built up of commands separated by a pipe.
 *
 * It does this by checking for the presence of the pipe "|" synbol outside of quotes
 *
 * @param input the string to check for the presence of the pipe symbol
 *
 * @return true/false
 */
bool unquotedCharacter(const std::string& input, char character = '|', const std::set<char>& quoteCharacters = {'\'', '"'});

#endif //UTILS_H

