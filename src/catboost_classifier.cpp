#include "catboost_classifier.h"

#include <c_api.h>

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <stdexcept>

namespace fashion_mnist {

void CatBoostDeleter::operator()(void* handle) const noexcept {
    if (handle) {
        ModelCalcerDelete(handle);
    }
}

CatBoostClassifier::CatBoostClassifier(const std::string& model_path) {
    // Проверяем файл заранее: иначе CatBoost печатает
    // в ошибке весь стек вызовов.
    if (!std::filesystem::exists(model_path)) {
        throw std::runtime_error("cannot open model file: " + model_path);
    }

    handle_.reset(ModelCalcerCreate());

    if (!handle_) {
        throw std::runtime_error("ModelCalcerCreate failed");
    }

    if (!LoadFullModelFromFile(handle_.get(), model_path.c_str())) {
        const std::string error = GetErrorString();

        throw std::runtime_error("cannot load model '" + model_path + "': " + error);
    }

    feature_count_ = GetFloatFeaturesCount(handle_.get());
    dimension_count_ = GetDimensionsCount(handle_.get());

    if (dimension_count_ < 2) {
        throw std::runtime_error(
            "model is not a multiclass classifier");
    }
}

CatBoostClassifier::~CatBoostClassifier() = default;

std::vector<double> CatBoostClassifier::predictRaw(const std::vector<float>& features) const {

    if (features.size() != feature_count_) {
        throw std::invalid_argument(
            "expected " + std::to_string(feature_count_) +
            " features, got " + std::to_string(features.size()));
    }

    std::vector<double> result(dimension_count_);

    if (!CalcModelPredictionSingle(
            handle_.get(),
            features.data(),
            features.size(),
            nullptr,
            0,
            result.data(),
            result.size())) {
        throw std::runtime_error(
            std::string("CalcModelPredictionSingle failed: ") +
            GetErrorString());
    }

    return result;
}

int CatBoostClassifier::predict(
    const std::vector<float>& features) const {

    // По умолчанию возвращаются RawFormulaVal; softmax монотонен,
    // поэтому argmax логитов совпадает с argmax вероятностей.
    const auto raw = predictRaw(features);

    return static_cast<int>(
        std::distance(
            raw.begin(),
            std::max_element(raw.begin(), raw.end())));
}

}  // namespace fashion_mnist