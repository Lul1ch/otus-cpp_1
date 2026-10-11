#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>
#include <string>

#include "accuracy.h"
#include "dataset.h"

using namespace fashion_mnist;

namespace {

std::string makeLine(int label, int pixel_value = 0, std::size_t pixels = kImageSize) {
    std::string line = std::to_string(label);
    for (std::size_t i = 0; i < pixels; ++i) {
        line += ',' + std::to_string(pixel_value);
    }
    return line;
}

// Классификатор-заглушка: возвращает значение первого пикселя как класс.
class FirstPixelClassifier : public IClassifier {
public:
    int predict(const std::vector<float>& features) const override {
        return static_cast<int>(features.at(0));
    }
};

}  // namespace

TEST(ParseCsvLine, ParsesLabelAndPixels) {
    std::string line = "3";
    for (std::size_t i = 0; i < kImageSize; ++i) line += ',' + std::to_string(i % 256);

    const Sample s = parseCsvLine(line);
    EXPECT_EQ(s.label, 3);
    ASSERT_EQ(s.pixels.size(), kImageSize);
    EXPECT_FLOAT_EQ(s.pixels[0], 0.0f);
    EXPECT_FLOAT_EQ(s.pixels[255], 255.0f);
    EXPECT_FLOAT_EQ(s.pixels[783], static_cast<float>(783 % 256));
}

TEST(ParseCsvLine, PixelsAreNotNormalized) {
    const Sample s = parseCsvLine(makeLine(1, 200));
    EXPECT_FLOAT_EQ(s.pixels[100], 200.0f);
}

TEST(ParseCsvLine, AcceptsWindowsLineEnding) {
    EXPECT_NO_THROW(parseCsvLine(makeLine(1) + "\r"));
}

TEST(ParseCsvLine, RejectsWrongColumnCount) {
    EXPECT_THROW(parseCsvLine(makeLine(1, 0, kImageSize - 1)), std::runtime_error);
    EXPECT_THROW(parseCsvLine(makeLine(1, 0, kImageSize + 1)), std::runtime_error);
}

TEST(ParseCsvLine, RejectsInvalidValues) {
    EXPECT_THROW(parseCsvLine(makeLine(10)), std::runtime_error);   // метка вне 0..9
    EXPECT_THROW(parseCsvLine(makeLine(1, 256)), std::runtime_error);  // пиксель > 255
    EXPECT_THROW(parseCsvLine("x" + makeLine(1).substr(1)), std::runtime_error);
    EXPECT_THROW(parseCsvLine("label,pixel1"), std::runtime_error);  // заголовок
}

TEST(CsvDatasetReader, ReadsAllRowsAndSkipsEmptyLines) {
    std::stringstream in(makeLine(1) + "\n\n" + makeLine(2) + "\n");
    CsvDatasetReader reader(in);
    EXPECT_EQ(reader.next()->label, 1);
    EXPECT_EQ(reader.next()->label, 2);
    EXPECT_FALSE(reader.next().has_value());
}

TEST(Evaluate, ComputesAccuracy) {
    // 3 из 4 объектов: метка совпадает с первым пикселем.
    std::stringstream in(makeLine(1, 1) + "\n" + makeLine(2, 2) + "\n" +
                         makeLine(3, 3) + "\n" + makeLine(4, 5) + "\n");
    const auto r = evaluate(FirstPixelClassifier{}, in);
    EXPECT_EQ(r.total, 4u);
    EXPECT_EQ(r.correct, 3u);
    EXPECT_DOUBLE_EQ(r.accuracy(), 0.75);
}

TEST(Evaluate, EmptyInputGivesZeroTotal) {
    std::stringstream in;
    EXPECT_EQ(evaluate(FirstPixelClassifier{}, in).total, 0u);
}

TEST(Evaluate, ReportsLineNumberOnError) {
    std::stringstream in(makeLine(1) + "\nbroken\n");
    try {
        evaluate(FirstPixelClassifier{}, in);
        FAIL() << "exception expected";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("line 2"), std::string::npos);
    }
}
