#include <type_traits>
#include <cstddef>
#include <new>
#include <iostream>
#include <memory>
#include <utility>
#include <iterator>
#include <map>

template <std::size_t BlocksCount>
class MemoryPool
{
public:
    MemoryPool()
        : m_buffer(static_cast<void*>(::operator new(BlocksCount * m_block_size))),
          m_used(0)
    {}

    ~MemoryPool()
    {
        ::operator delete(m_buffer);
    }

    void* allocate_bytes(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        std::size_t current = reinterpret_cast<std::size_t>(m_buffer) + m_used;
        std::size_t aligned = (current + alignment - 1) & ~(alignment - 1);
        std::size_t offset = aligned - reinterpret_cast<std::size_t>(m_buffer);

        if (offset + bytes > BlocksCount * m_block_size)
        {
            std::cout << "1\n";
            throw std::bad_alloc();
        }
        m_used = offset + bytes;
        return static_cast<char*>(m_buffer) + offset;
    }

private:
    static constexpr std::size_t m_block_size = 1;
    void* m_buffer = nullptr;
    std::size_t m_used = 0;
};

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

    CustomAllocator() noexcept
        : m_pool(std::make_shared<MemoryPool<BlocksCount>>())
    {}

    template <class U>
    CustomAllocator(const CustomAllocator<U, BlocksCount>& other) noexcept
        : m_pool(other.m_pool)
    {}

    CustomAllocator(const CustomAllocator&) noexcept = default;
    CustomAllocator& operator=(const CustomAllocator&) noexcept = default;
    CustomAllocator(CustomAllocator&&) noexcept = default;
    CustomAllocator& operator=(CustomAllocator&&) noexcept = default;

    T* allocate(std::size_t n)
    {
        if (n == 0)
            return nullptr;
        if (n > BlocksCount)
        {
            std::cout << "2\n";
            throw std::bad_alloc();
        }
        void* p = m_pool->allocate_bytes(n * sizeof(T), alignof(T));
        return static_cast<T*>(p);
    }

    void deallocate(T*, std::size_t) noexcept
    {
    }

    template <class U, std::size_t C>
    friend class CustomAllocator;

private:
    std::shared_ptr<MemoryPool<BlocksCount>> m_pool;
};

template <typename T, std::size_t C, typename U>
bool operator==(const CustomAllocator<T, C>& a, const CustomAllocator<U, C>& b) noexcept
{
    return a.m_pool == b.m_pool;
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
          m_capacity(capacity)
    {}

    ~DynamicArray()
    {
        clear();
        if (m_data)
            traits::deallocate(m_alloc, m_data, m_capacity);
    }

    template <class... Args>
    void emplace_back(Args&&... args)
    {
        if (m_size == m_capacity)
        {
            std::cout << "3\n";
            throw std::bad_alloc();
        }
        traits::construct(m_alloc, m_data + m_size, std::forward<Args>(args)...);
        ++m_size;
    }

    void clear() noexcept
    {
        for (std::size_t i = 0; i < m_size; ++i)
            traits::destroy(m_alloc, m_data + i);
        m_size = 0;
    }

    T* begin() noexcept { return m_data; }
    T* end() noexcept { return m_data + m_size; }
    std::size_t size() const noexcept { return m_size; }

private:
    Alloc m_alloc;
    T* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
};

int factorial(int n)
{
    int res = 1;
    for (int i = 2; i <= n; ++i)
        res *= i;
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