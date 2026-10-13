#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "accuracy.h"
#include "catboost_classifier.h"
#include "dataset.h"

using namespace fashion_mnist;

namespace {

const std::string kSample = std::string(TEST_DATA_DIR) + "/sample.csv";
const std::string kRawExpected = std::string(TEST_DATA_DIR) + "/sample_raw_expected.txt";

std::vector<std::vector<double>> loadExpectedRaw() {
    std::ifstream in(kRawExpected);
    std::vector<std::vector<double>> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::vector<double> row;
        double v;
        while (ss >> v) row.push_back(v);
        rows.push_back(row);
    }
    return rows;
}

}  // namespace

TEST(CatBoostClassifier, LoadsModel) {
    const CatBoostClassifier model(MODEL_PATH);
    EXPECT_EQ(model.featureCount(), kImageSize);
    EXPECT_EQ(model.dimensionCount(), static_cast<std::size_t>(kClassCount));
}

TEST(CatBoostClassifier, ThrowsOnMissingModel) {
    EXPECT_THROW(CatBoostClassifier("no_such_model.cbm"), std::runtime_error);
}

TEST(CatBoostClassifier, ThrowsOnWrongFeatureCount) {
    const CatBoostClassifier model(MODEL_PATH);
    EXPECT_THROW(model.predict(std::vector<float>(10)), std::invalid_argument);
}

// Сверка с эталоном, полученным в Python: model.predict(X, prediction_type='RawFormulaVal')
TEST(CatBoostClassifier, RawPredictionsMatchPython) {
    const CatBoostClassifier model(MODEL_PATH);
    const auto expected = loadExpectedRaw();

    std::ifstream csv(kSample);
    CsvDatasetReader reader(csv);
    std::size_t i = 0;

    Sample sample_test;
    while (auto sample = reader.next(sample_test)) {
        ASSERT_LT(i, expected.size());
        const auto raw = model.predictRaw(sample->pixels);
        ASSERT_EQ(raw.size(), expected[i].size());
        for (std::size_t k = 0; k < raw.size(); ++k) {
            EXPECT_NEAR(raw[k], expected[i][k], 1e-6) << "row " << i << " class " << k;
        }
        ++i;
    }
    EXPECT_EQ(i, expected.size());
}

TEST(CatBoostClassifier, PredictsSampleClasses) {
    const CatBoostClassifier model(MODEL_PATH);
    std::ifstream csv(kSample);
    const auto r = evaluate(model, csv);
    EXPECT_EQ(r.total, 5u);
    EXPECT_EQ(r.correct, 5u);  // на этих 5 строках модель (и Python) не ошибаются
}
