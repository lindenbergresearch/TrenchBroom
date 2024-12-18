#pragma once

#include "string"

namespace TrenchBroom {
/**
 * @brief Replacement function of 'stringf' from v0.6.x
 * @param zcFormat Format string
 * @param ... Formats args
 * @return Rendered string
 *
 * This is used as replacement for the dropped 'stringf'
 * function from string.cpp used in Rack 0.6.x.
 * It just covers the functionality but uses a quite different
 * code found at:
 *
 * https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
 *
 */
inline std::string stringf(const char *const zcFormat, ...);

typedef std::pair<std::string, std::string> TStringPair;


/**
 * Simple Key-Value Table supporting different datatypes and layout-options.
 */
class StatsTable {
    // the title - leave empty to disable
    std::string m_title = "";
    // data buffer
    std::vector<TStringPair> m_lines;
    // total width of the label on the left side, 0 means no layout
    size_t m_labelWidth = 25;
    // the number of digits after the assignment char
    size_t m_rightPadding = 1;
    // assignment char
    char m_assignmentChar = ':';
    // end of line char
    char m_lineSeparatorChar = '\n';

  public:
    enum TimeUnit {
      SECONDS,
      MILLIS = 1000,
      MICROS = 1000 * MILLIS,
      NANOS = 1000 * MICROS
    };


    void addPair(std::string name, std::string value, bool pushFront = false);

    void addString(const std::string &name, const std::string &value);

    void addString(const std::string &name, const QString &value);

    void addInt(const std::string &name, const int value);

    void addLong(const std::string &name, const long value);

    void addBool(const std::string &name, const bool value);

    void addFloat(const std::string &name, const float value, const int n = 2, const std::string &suffix = "");

    void addDouble(const std::string &name, const double value, const int n = 2, const std::string &suffix = "");

    void addTime(const std::string &name, const double value, const TimeUnit tu = SECONDS);

    void addMem(const std::string &name, const size_t value);

    void addSeparator();

    std::string render(bool reverse = false) const;

    void reset();

    size_t labelWidth() const;

    void setLabelWidth(size_t mLeftSize);

    size_t rightPadding() const;

    void setRightPadding(size_t mRightSize);

    const std::string &title() const;

    void setTitle(const std::string &mTitle);

    char assignmentChar() const;

    void setAssignmentChar(char assignmentChar);

    char lineSeparatorChar() const;

    void setLineSeparatorChar(char lineSeparatorChar);
};


class BoxStats {
  private:
    std::vector<double> m_data{0};

    size_t m_pos;
    const size_t m_length;
    const size_t m_mask;
  public:
    BoxStats(size_t size = 7);

    void add(double n);


    double average();

};


struct Stats {
  double max, min;
  size_t j, resetMinMax = 44;
  BoxStats stat{7};

  explicit Stats();

  void push(double value);

  double average();

  void softReset();

  void reset();
};

/**
 * @brief Replaces all characters in a string with the specified character.
 *
 * @param str The string in which characters will be replaced.
 * @param ch The character to replace each character in the string with.
 * @return std::string The modified string with all characters replaced.
 */
std::string replaceAllChars(std::string str, char ch);

} // namespace TrenchBroom
