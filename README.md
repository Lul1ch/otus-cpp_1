# Fashion MNIST: инференс CatBoost на C++

Программа загружает модель CatBoost (`model.cbm`), классифицирует изображения
из CSV-файла и печатает долю правильных ответов (accuracy).

```bash
./build/fashion_mnist test.csv model.cbm
0.883889
```

## Формат входных данных

CSV без заголовка, 785 целых чисел в строке: первое — класс (0–9),
остальные 784 — пиксели 0–255 (изображение 28×28 по строкам).
Пиксели передаются модели без нормализации — так модель обучалась в `FashionMNIST.ipynb`.

## Зависимости

- Linux x86_64 или aarch64, g++ с C++17, CMake ≥ 3.14
- `libcatboostmodel.so` и `c_api.h` — скачиваются скриптом
- GoogleTest — скачивается CMake автоматически при сборке тестов

## Сборка

```bash
./scripts/download_catboost.sh          # библиотека CatBoost -> third_party/catboost
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Сборка в Docker-образе `sdukshis/cppml` (библиотека уже установлена в `/usr/local`):

```bash
cmake -S . -B build -DCATBOOST_ROOT=/usr/local
cmake --build build -j
```

Без тестов: `-DBUILD_TESTING=OFF`.

## Запуск

```bash
./build/fashion_mnist test.csv model.cbm
```

## Тесты

```bash
cd build && ctest --output-on-failure
```

- `tests/test_dataset.cpp` — разбор CSV, обработка ошибок, подсчёт accuracy на классификаторе-заглушке;
- `tests/test_model.cpp` — загрузка модели и сверка логитов C++ с эталоном из Python
  (`tests/data/sample_raw_expected.txt`, получен через
  `model.predict(X, prediction_type='RawFormulaVal')`).

## Структура

```
CMakeLists.txt
model.cbm
scripts/download_catboost.sh
src/
  main.cpp                   разбор аргументов, вывод accuracy
  dataset.h/.cpp             построчное чтение и проверка CSV
  classifier.h               интерфейс IClassifier
  catboost_classifier.h/.cpp RAII-обёртка над C API CatBoost
  accuracy.h/.cpp            подсчёт accuracy
tests/
  test_dataset.cpp
  test_model.cpp
  data/sample.csv            первые 5 строк test.csv
  data/sample_raw_expected.txt
```

## Выбор модели

CatBoost (заявленная точность 0.87) выбран как компромисс: точнее логистической
регрессии, при этом не требует ручной реализации модели — достаточно вызвать
`CalcModelPredictionSingle`. Функция возвращает 10 логитов, класс выбирается по максимуму
(softmax монотонен, поэтому вероятности считать не нужно).
