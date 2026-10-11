#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "classifier.h"

namespace fashion_mnist {

// RAII-обёртка над C API CatBoost.
class CatBoostClassifier final : public IClassifier {
public:
    explicit CatBoostClassifier(const std::string& model_path);
    ~CatBoostClassifier() override;

    CatBoostClassifier(const CatBoostClassifier&) = delete;
    CatBoostClassifier& operator=(const CatBoostClassifier&) = delete;

    // Сырые оценки (логиты) по всем классам.
    std::vector<double> predictRaw(const std::vector<float>& features) const;

    // Номер класса с максимальной оценкой.
    int predict(const std::vector<float>& features) const override;

    std::size_t featureCount() const { return feature_count_; }
    std::size_t dimensionCount() const { return dimension_count_; }

private:
    void* handle_ = nullptr;  // ModelCalcerHandle* из c_api.h (typedef void)
    std::size_t feature_count_ = 0;
    std::size_t dimension_count_ = 0;
};

}  // namespace fashion_mnist
