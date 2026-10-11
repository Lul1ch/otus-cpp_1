#pragma once

#include <vector>

namespace fashion_mnist {

// Общий интерфейс классификатора: позволяет тестировать подсчёт accuracy
// без CatBoost и при желании добавить другие модели.
class IClassifier {
public:
    virtual ~IClassifier() = default;
    virtual int predict(const std::vector<float>& features) const = 0;
};

}  // namespace fashion_mnist
