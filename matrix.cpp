#include <cstdint>
#include <cstddef>
#include <iostream>
#include <map>
#include <utility>

constexpr int DEFAULT_VALUE = 0;

template <typename T, typename Index = std::int64_t>
class SparseMatrix
{
private:
    using Position = std::pair<Index, Index>;
    using Storage = std::map<Position, T>;

    T default_value_;
    Storage data_;

public:
    struct Cell
    {
        Index row;
        Index column;
        T value;
    };

private:
    class CellProxy
    {
    private:
        SparseMatrix& matrix_;
        Position position_;

    public:
        CellProxy(SparseMatrix& matrix, Index row, Index column)
            : matrix_(matrix)
            , position_(row, column)
        {
        }

        // Чтение: matrix[row][column]
        operator T() const
        {
            const auto it = matrix_.data_.find(position_);

            if (it == matrix_.data_.end())
            {
                return matrix_.default_value_;
            }

            return it->second;
        }

        // Запись значения
        CellProxy& operator=(const T& value)
        {
            if (value == matrix_.default_value_)
            {
                matrix_.data_.erase(position_);
            }
            else
            {
                matrix_.data_[position_] = value;
            }

            return *this;
        }

        // Например: matrix[1][2] = matrix[3][4]
        CellProxy& operator=(const CellProxy& other)
        {
            return operator=(static_cast<T>(other));
        }
    };

    class RowProxy
    {
    private:
        SparseMatrix& matrix_;
        Index row_;

    public:
        RowProxy(SparseMatrix& matrix, Index row)
            : matrix_(matrix)
            , row_(row)
        {
        }

        CellProxy operator[](Index column)
        {
            return CellProxy(matrix_, row_, column);
        }
    };

public:
    class ConstRowProxy
    {
    private:
        const SparseMatrix& matrix_;
        Index row_;

    public:
        ConstRowProxy(const SparseMatrix& matrix, Index row)
            : matrix_(matrix)
            , row_(row)
        {
        }

        T operator[](Index column) const
        {
            return matrix_.at(row_, column);
        }
    };

public:
    explicit SparseMatrix(const T& default_value = T{})
        : default_value_(default_value)
    {
    }

    // Доступ для записи
    RowProxy operator[](Index row)
    {
        return RowProxy(*this, row);
    }

    // Доступ для чтения константной матрицы
    ConstRowProxy operator[](Index row) const
    {
        return ConstRowProxy(*this, row);
    }

    T at(Index row, Index column) const
    {
        const auto it = data_.find({row, column});

        if (it == data_.end())
        {
            return default_value_;
        }

        return it->second;
    }

    std::size_t size() const noexcept
    {
        return data_.size();
    }

    const T& default_value() const noexcept
    {
        return default_value_;
    }

    class Iterator
    {
    private:
        using MapIterator = typename Storage::const_iterator;

        MapIterator current_;

    public:
        explicit Iterator(MapIterator current)
            : current_(current)
        {
        }

        Cell operator*() const
        {
            return Cell{
                current_->first.first,
                current_->first.second,
                current_->second
            };
        }

        Iterator& operator++()
        {
            ++current_;
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
            return current_ != other.current_;
        }

        bool operator==(const Iterator& other) const
        {
            return current_ == other.current_;
        }
    };

    Iterator begin() const
    {
        return Iterator(data_.begin());
    }

    Iterator end() const
    {
        return Iterator(data_.end());
    }
};

int main()
{
    SparseMatrix<int> matrix(DEFAULT_VALUE);

    for (int i = 0; i < 10; i++)
    {
        matrix[i][i] = i;
        matrix[9 - i][i] = i;
    }

    for (int i = 1; i < 9; i++)
    {
        for (int j = 1; j < 9; j++)
        {
            std::cout << matrix[i][j] << " ";
        }
        std::cout << "\n";
    }

    std::cout << "Matrix size is " << matrix.size() << "\n";

    ((matrix[5][5] = 314) = 0) = 217;

    for (const auto cell : matrix)
    {
        std::cout << "matrix["
                  << cell.row
                  << "]["
                  << cell.column
                  << "] = "
                  << cell.value
                  << '\n';
    }

    return 0;
}