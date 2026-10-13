#pragma once

#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace fashion_mnist {

constexpr std::size_t kImageSize = 28 * 28;
constexpr int kClassCount = 10;

struct Sample {
    int label = -1;
    std::vector<float> pixels;  // 784 значения 0..255, без нормализации
};

// Разбирает одну строку данных CSV: "label,p1,...,p784".
// Записывает результат в sample и возвращает его копию.
// Бросает std::runtime_error при неверном формате.
Sample parseCsvLine(const std::string& line, Sample& sample);

// Построчное чтение тестового набора без загрузки всего файла в память.
class CsvDatasetReader {
public:
    explicit CsvDatasetReader(std::istream& input);

    // Заполняет out и возвращает следующий объект,
    // либо std::nullopt в конце файла.
    // Пустые строки пропускаются.
    // Первая непустая строка может быть заголовком
    // "label,pixel1,...,pixel784".
    std::optional<Sample> next(Sample& out);

    std::size_t lineNumber() const { return line_number_; }

private:
    std::istream& input_;
    std::size_t line_number_ = 0;
    bool first_nonempty_line_ = true;
};

}  // namespace fashion_mnist