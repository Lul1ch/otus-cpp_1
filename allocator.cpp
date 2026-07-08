#include <type_traits>
#include <cstddef>
#include <new>
#include <iostream>
#include <memory>
#include <utility>
#include <iterator>
#include <map>

template <typename T, std::size_t BlocksCount>
class CustomAllocator 
{
public:
    using value_type = T;

    template <typename U>
    struct rebind 
    {
        using other = CustomAllocator<U, BlocksCount>;
    };

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    CustomAllocator() noexcept = default;

    template <class U>
    CustomAllocator(const CustomAllocator<U, BlocksCount>&) noexcept {}

    CustomAllocator(const CustomAllocator&) noexcept = default;
    CustomAllocator& operator=(const CustomAllocator&) noexcept = default;

    CustomAllocator(CustomAllocator&&) noexcept = default;
    CustomAllocator& operator=(CustomAllocator&&) noexcept = default;

    ~CustomAllocator() = default;

    T* allocate(std::size_t n) 
    {
        if (n == 0) {
            return nullptr;
        }
        if (n > BlocksCount) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept 
    {
        ::operator delete(p);
    }

    template <class U, std::size_t C>
    friend class CustomAllocator;
};

template <typename T, std::size_t C, typename U>
bool operator==(const CustomAllocator<T, C>&, const CustomAllocator<U, C>&) noexcept 
{
    return true;
}

template <typename T, std::size_t C, typename U>
bool operator!=(const CustomAllocator<T, C>& a, const CustomAllocator<U, C>& b) noexcept 
{
    return !(a == b);
}

template <class T, class Alloc = std::allocator<T>>
class DynamicArray 
{
public:
    using traits = std::allocator_traits<Alloc>;

    explicit DynamicArray(std::size_t capacity, Alloc alloc = Alloc{})
        : m_alloc(std::move(alloc)),
          m_data(traits::allocate(m_alloc, capacity)),
          m_capacity(capacity) {}

    ~DynamicArray() 
    {
        clear();
        if (m_data) {
            traits::deallocate(m_alloc, m_data, m_capacity);
        }
    }

    template <class... Args>
    void emplace_back(Args&&... args) 
    {
        if (m_size == m_capacity) {
            throw std::bad_alloc();
        }
        traits::construct(m_alloc, m_data + m_size, std::forward<Args>(args)...);
        ++m_size;
    }

    void clear() noexcept 
    {
        for (std::size_t i = 0; i < m_size; ++i) 
        {
            traits::destroy(m_alloc, m_data + i);
        }
        m_size = 0;
    }

    T* begin() noexcept { return m_data; }
    T* end() noexcept { return m_data + m_size; }
    std::size_t size() { return m_size; }

private:
    Alloc m_alloc;
    T* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
};

int factorial(int n) {
    int res = 1;
    for (int i = 2; i <= n; ++i) 
    {
        res *= i;
    }
    return res;
}

int main() 
{
    using MapValue = std::pair<const int, int>;
    using MapAlloc = CustomAllocator<MapValue, 10>;

    std::map<int, int> map1;
    std::map<int, int, std::less<int>, MapAlloc> map2((std::less<int>()), MapAlloc{});

    DynamicArray<int> arr1(10);
    DynamicArray<int, CustomAllocator<int, 10>> arr2(10);

    for (int i = 0; i < 10; ++i) 
    {
        map1.emplace(i, factorial(i));
        map2.emplace(i, factorial(i));
        arr1.emplace_back(i);
        arr2.emplace_back(i);
    }

    std::cout << "Map and standard allocator\n";
    for (const auto& kv : map1) 
    {
        std::cout << kv.first << " -> " << kv.second << "\n";
    }

	std::cout << "Map and custom allocator\n";
    for (const auto& kv : map2) 
    {
        std::cout << kv.first << " -> " << kv.second << "\n";
    }

    std::cout << "Custom array and standard allocator\n";
    for (auto it = arr1.begin(); it != arr1.end(); ++it) 
    {
        std::cout << *it << ' ';
    }
    std::cout << "\n";

    std::cout << "Custom array and custom allocator\n";
    for (auto it = arr2.begin(); it != arr2.end(); ++it) 
    {
        std::cout << *it << ' ';
    }
    std::cout << "\n";
}