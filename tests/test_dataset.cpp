#include <gtest/gtest.h>

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "accuracy.h"
#include "dataset.h"

using namespace fashion_mnist;

namespace {

std::string makeLine(
    int label,
    int pixel_value = 0,
    std::size_t pixels = kImageSize) {

    std::string line = std::to_string(label);

    for (std::size_t i = 0; i < pixels; ++i) {
        line += ',' + std::to_string(pixel_value);
    }

    return line;
}

// Классификатор-заглушка:
// возвращает значение первого пикселя как класс.
class FirstPixelClassifier : public IClassifier {
public:
    int predict(const std::vector<float>& features) const override {
        return static_cast<int>(features.at(0));
    }
};

}  // namespace

TEST(ParseCsvLine, ParsesLabelAndPixels) {
    std::string line = "3";

    for (std::size_t i = 0; i < kImageSize; ++i) {
        line += ',' + std::to_string(i % 256);
    }

    Sample sample{};

    ASSERT_NO_THROW(parseCsvLine(line, sample));

    EXPECT_EQ(sample.label, 3);
    ASSERT_EQ(sample.pixels.size(), kImageSize);
    EXPECT_FLOAT_EQ(sample.pixels[0], 0.0f);
    EXPECT_FLOAT_EQ(sample.pixels[255], 255.0f);
    EXPECT_FLOAT_EQ(
        sample.pixels[783],
        static_cast<float>(783 % 256));
}

TEST(ParseCsvLine, PixelsAreNotNormalized) {
    Sample sample{};

    ASSERT_NO_THROW(parseCsvLine(makeLine(1, 200), sample));

    ASSERT_EQ(sample.pixels.size(), kImageSize);
    EXPECT_FLOAT_EQ(sample.pixels[100], 200.0f);
}

TEST(ParseCsvLine, AcceptsWindowsLineEnding) {
    Sample sample{};

    ASSERT_NO_THROW(parseCsvLine(makeLine(1) + "\r", sample));

    EXPECT_EQ(sample.label, 1);
    EXPECT_EQ(sample.pixels.size(), kImageSize);
}

TEST(ParseCsvLine, RejectsWrongColumnCount) {
    Sample too_few{};
    Sample too_many{};

    EXPECT_THROW(
        parseCsvLine(makeLine(1, 0, kImageSize - 1), too_few),
        std::runtime_error);

    EXPECT_THROW(
        parseCsvLine(makeLine(1, 0, kImageSize + 1), too_many),
        std::runtime_error);
}

TEST(ParseCsvLine, RejectsInvalidValues) {
    Sample invalid_label{};
    Sample invalid_pixel{};
    Sample nonnumeric_label{};
    Sample header{};

    // Метка вне диапазона 0..9.
    EXPECT_THROW(
        parseCsvLine(makeLine(10), invalid_label),
        std::runtime_error);

    // Значение пикселя больше 255.
    EXPECT_THROW(
        parseCsvLine(makeLine(1, 256), invalid_pixel),
        std::runtime_error);

    // Нечисловая метка.
    EXPECT_THROW(
        parseCsvLine(
            "x" + makeLine(1).substr(1),
            nonnumeric_label),
        std::runtime_error);

    // Заголовок вместо строки данных.
    EXPECT_THROW(
        parseCsvLine("label,pixel1", header),
        std::runtime_error);
}

TEST(ParseCsvLine, ReplacesPreviousSample) {
    Sample sample{};

    ASSERT_NO_THROW(parseCsvLine(makeLine(1, 100), sample));

    EXPECT_EQ(sample.label, 1);
    ASSERT_EQ(sample.pixels.size(), kImageSize);
    EXPECT_FLOAT_EQ(sample.pixels.front(), 100.0f);
    EXPECT_FLOAT_EQ(sample.pixels.back(), 100.0f);

    ASSERT_NO_THROW(parseCsvLine(makeLine(3, 200), sample));

    EXPECT_EQ(sample.label, 3);
    ASSERT_EQ(sample.pixels.size(), kImageSize);
    EXPECT_FLOAT_EQ(sample.pixels.front(), 200.0f);
    EXPECT_FLOAT_EQ(sample.pixels.back(), 200.0f);
}

TEST(CsvDatasetReader, ReadsAllRowsAndSkipsEmptyLines) {
    std::stringstream in(
        makeLine(1) + "\n\n" +
        makeLine(2) + "\n");

    CsvDatasetReader reader(in);
    Sample sample{};

    ASSERT_TRUE(reader.next(sample).has_value());
    EXPECT_EQ(sample.label, 1);
    ASSERT_EQ(sample.pixels.size(), kImageSize);

    ASSERT_TRUE(reader.next(sample).has_value());
    EXPECT_EQ(sample.label, 2);
    ASSERT_EQ(sample.pixels.size(), kImageSize);

    EXPECT_FALSE(reader.next(sample).has_value());
}

TEST(Evaluate, ComputesAccuracy) {
    // У 3 из 4 объектов метка совпадает с первым пикселем.
    std::stringstream in(
        makeLine(1, 1) + "\n" +
        makeLine(2, 2) + "\n" +
        makeLine(3, 3) + "\n" +
        makeLine(4, 5) + "\n");

    const auto result = evaluate(FirstPixelClassifier{}, in);

    EXPECT_EQ(result.total, 4u);
    EXPECT_EQ(result.correct, 3u);
    EXPECT_DOUBLE_EQ(result.accuracy(), 0.75);
}

TEST(Evaluate, EmptyInputGivesZeroTotal) {
    std::stringstream in;

    EXPECT_EQ(
        evaluate(FirstPixelClassifier{}, in).total,
        0u);
}

TEST(Evaluate, ReportsLineNumberOnError) {
    std::stringstream in(makeLine(1) + "\nbroken\n");

    try {
        evaluate(FirstPixelClassifier{}, in);
        FAIL() << "exception expected";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(
            std::string(error.what()).find("line 2"),
            std::string::npos);
    }
}