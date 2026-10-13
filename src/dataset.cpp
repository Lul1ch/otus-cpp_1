#include "dataset.h"

#include <charconv>
#include <stdexcept>
#include <string_view>

namespace fashion_mnist {
namespace {

int parseInt(const char* begin, const char* end) {
    // Допускаем пробелы и '\r' (файлы из Windows) вокруг числа.
    while (begin < end && (*begin == ' ' || *begin == '\t')) {
        ++begin;
    }

    while (end > begin &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        --end;
    }

    int value = 0;
    const auto [ptr, ec] = std::from_chars(begin, end, value);

    if (ec != std::errc() || ptr != end || begin == end) {
        throw std::runtime_error(
            "invalid integer: '" + std::string(begin, end) + "'");
    }

    return value;
}

bool isCsvHeader(std::string_view line) {
    // Убираем внешние пробелы и Windows-окончание строки.
    const auto first = line.find_first_not_of(" \t\r");

    if (first == std::string_view::npos) {
        return false;
    }

    const auto last = line.find_last_not_of(" \t\r");
    line = line.substr(first, last - first + 1);

    // Заголовок создаётся один раз.
    static const std::string expected_header = [] {
        std::string header = "label";

        for (std::size_t i = 1; i <= kImageSize; ++i) {
            header += ",pixel" + std::to_string(i);
        }

        return header;
    }();

    return line == std::string_view(expected_header);
}

}  // namespace

Sample parseCsvLine(const std::string& line, Sample& sample) {
    // Объект может повторно использоваться для следующей строки.
    sample.label = -1;
    sample.pixels.clear();
    sample.pixels.reserve(kImageSize);

    const char* cur = line.data();
    const char* const end = cur + line.size();
    bool first = true;

    while (true) {
        const char* comma = cur;

        while (comma < end && *comma != ',') {
            ++comma;
        }

        const int value = parseInt(cur, comma);

        if (first) {
            if (value < 0 || value >= kClassCount) {
                throw std::runtime_error(
                    "label out of range: " + std::to_string(value));
            }

            sample.label = value;
            first = false;
        } else {
            if (value < 0 || value > 255) {
                throw std::runtime_error(
                    "pixel out of range: " + std::to_string(value));
            }

            if (sample.pixels.size() == kImageSize) {
                throw std::runtime_error(
                    "too many columns, expected " +
                    std::to_string(kImageSize + 1));
            }

            sample.pixels.push_back(static_cast<float>(value));
        }

        if (comma == end) {
            break;
        }

        cur = comma + 1;
    }

    if (sample.pixels.size() != kImageSize) {
        throw std::runtime_error(
            "expected " + std::to_string(kImageSize + 1) +
            " columns, got " +
            std::to_string(sample.pixels.size() + 1));
    }

    return sample;
}

CsvDatasetReader::CsvDatasetReader(std::istream& input)
    : input_(input) {}

std::optional<Sample> CsvDatasetReader::next(Sample& out) {
    std::string line;

    while (std::getline(input_, line)) {
        ++line_number_;

        if (line.find_first_not_of(" \t\r") == std::string::npos) {
            continue;
        }

        if (first_nonempty_line_) {
            first_nonempty_line_ = false;

            if (isCsvHeader(line)) {
                continue;
            }
        }

        return parseCsvLine(line, out);
    }

    return std::nullopt;
}

}  // namespace fashion_mnist