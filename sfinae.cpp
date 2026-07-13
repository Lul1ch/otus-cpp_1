#include <iostream>
#include <type_traits>
#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <tuple>
#include <utility>

template <typename T>
struct is_int_width_8_to_64
    : std::integral_constant<bool,
        std::is_integral<T>::value &&
        std::is_signed<T>::value &&
        (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8)> {};

template <typename T>
void printDecimalAsIP(T argument)
{
    int size = (sizeof(T) - 1) * 8;
    while (size >= 0)
    {
        if (size > 0)
            std::cout << ((argument >> size) & 255) << '.';
        else
            std::cout << (argument & 255);

        size -= 8;
    }
}

template <typename T,
          typename std::enable_if<
              std::is_integral<T>::value &&
              std::is_signed<T>::value &&
              sizeof(T) >= sizeof(std::int8_t) &&
              sizeof(T) <= sizeof(std::int64_t),
              int>::type = 0>
void printIP(T argument)
{
    printDecimalAsIP(argument);
    std::cout << '\n';
}

template <typename T>
auto printIP(T argument) -> decltype(argument.c_str())
{
    std::cout << argument << '\n';
    return argument.c_str();
}

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {};

template <typename T>
struct is_list : std::false_type {};

template <typename T, typename Alloc>
struct is_list<std::list<T, Alloc>> : std::true_type {};

template <typename Container>
void printContainerAsIP(const Container& c)
{
    bool first = true;
    for (const auto& x : c)
    {
        if (!first)
            std::cout << '.';
        std::cout << x;
        first = false;
    }
    std::cout << '\n';
}

template <typename T,
          typename std::enable_if<is_vector<T>::value || is_list<T>::value, int>::type = 0>
void printIP(const T& container)
{
    printContainerAsIP(container);
}

template <typename Tuple>
struct tuple_all_same;

template <>
struct tuple_all_same<std::tuple<>> : std::true_type {};

template <typename T>
struct tuple_all_same<std::tuple<T>> : std::true_type {};

template <typename T, typename U, typename... Rest>
struct tuple_all_same<std::tuple<T, U, Rest...>>
    : std::integral_constant<bool,
        std::is_same<T, U>::value &&
        tuple_all_same<std::tuple<U, Rest...>>::value> {};

template <typename Tuple, std::size_t... Is>
void printTupleAsIP(const Tuple& tuple, std::index_sequence<Is...>)
{
    bool first = true;
    int dummy[] = {0, ((std::cout << (first ? "" : ".") << std::get<Is>(tuple), first = false), 0)...};
    (void)dummy;
    std::cout << '\n';
}

template <typename... Ts,
          typename std::enable_if<tuple_all_same<std::tuple<Ts...>>::value, int>::type = 0>
void printIP(const std::tuple<Ts...>& tuple)
{
    printTupleAsIP(tuple, std::index_sequence_for<Ts...>{});
}

int main()
{
    printIP(int8_t{-1});
    printIP(int16_t{0});
    printIP(int32_t{2130706433});
    printIP(int64_t{8875824491850138409});

    printIP(std::string{"Hello, World!"});

    printIP(std::vector<int>{100, 200, 300, 400});
    printIP(std::list<short>{400, 300, 200, 100});

    printIP(std::make_tuple(123, 456, 789, 0));

    return 0;
}