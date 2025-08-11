#ifndef ENBT_IO_TOOLS
#define ENBT_IO_TOOLS
#include "io.hpp"
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace enbt {
    namespace io_helper {
        //enables storage optimization for simple types in arrays
        template <class T, class = void>
        struct serialization_simple_cast {
        };

        template <class T>
            requires std::is_integral_v<T>
        struct serialization_simple_cast<std::unique_ptr<T>> {
            T write_cast(const std::unique_ptr<T>& value) {
                return *value;
            }

            std::unique_ptr<T> read_cast(T&& value) {
                return std::make_unique<T>(std::move(value));
            }
        };

        template <class T>
            requires std::is_integral_v<T>
        struct serialization_simple_cast<std::shared_ptr<T>> {
            T write_cast(const std::shared_ptr<T>& value) {
                return *value;
            }

            std::shared_ptr<T> read_cast(T&& value) {
                return std::make_shared<T>(std::move(value));
            }
        };

        template <class T>
            requires requires(T val) { serialization_simple_cast<T>::write_cast(val); }
        struct serialization_simple_cast<serialization_simple_cast<T>> {
            auto write_cast(const serialization_simple_cast<T>& value) {
                return value.write_cast(value);
            }

            auto read_cast(T&& value) {
                return value.read_cast(value);
            }
        };

        template <class T>
        concept simple_cast = requires(T val) { serialization_simple_cast<T>::write_cast(val); };

        template <class T, class = void>
        struct serialization_simple_cast_data : public std::false_type {
        };

        template <class T>
            requires simple_cast<T>
        struct serialization_simple_cast_data<T> : public std::true_type {
            using type = decltype(serialization_simple_cast<T>::write_cast(std::declval<T>()));
        };

        template <class T>
        struct serialization {
            static T read(value_read_stream& read_stream) {
                return T(read_stream.read());
            }

            static void read(T& value, value_read_stream& read_stream) {
                value = T(read_stream.read());
            }

            static void write(const T& value, value_write_stream& write_stream) {
                write_stream.write(value);
            }
        };

        template <class T>
        struct serialization<std::unique_ptr<T>> {
            static std::unique_ptr<T> read(value_read_stream& read_stream) {
                return std::make_unique<T>(serialization<T>::read(read_stream));
            }

            static void read(std::unique_ptr<T>& value, value_read_stream& read_stream) {
                value = std::make_unique<T>(serialization<T>::read(read_stream));
            }

            static void write(const std::unique_ptr<T>& value, value_write_stream& write_stream) {
                serialization<T>::write(*value, write_stream);
            }

            static T write_cast(const std::unique_ptr<T>& value) {
                return *value;
            }
        };

        template <class T>
        struct serialization<std::shared_ptr<T>> {
            static std::shared_ptr<T> read(value_read_stream& read_stream) {
                return std::make_shared<T>(serialization<T>::read(read_stream));
            }

            static void read(std::shared_ptr<T>& value, value_read_stream& read_stream) {
                value = std::make_shared<T>(serialization<T>::read(read_stream));
            }

            static void write(const std::shared_ptr<T>& value, value_write_stream& write_stream) {
                serialization<T>::write(*value, write_stream);
            }

            static T write_cast(const std::shared_ptr<T>& value) {
                return *value;
            }
        };

        template <class T1, class T2>
        struct serialization<std::pair<T1, T2>> {
            static std::pair<T1, T2> read(value_read_stream& read_stream) {
                std::pair<T1, T2> res;
                read(res, read_stream);
                return res;
            }

            static void read(std::pair<T1, T2>& value, value_read_stream& read_stream) {
                bool first_set = false;
                bool second_set = false;
                read_stream.blind_iterate(
                    [&](std::string_view name, value_read_stream& self) {
                        if (name == "first") {
                            serialization<T1>::read(value.first, self);
                            first_set = true;
                        } else if (name == "second") {
                            serialization<T2>::read(value.second, self);
                            second_set = true;
                        }
                    },
                    [&](value_read_stream& self) {
                        if (!first_set)
                            serialization<T1>::read(value.first, self);
                        if (!second_set)
                            serialization<T2>::read(value.second, self);
                    }
                );
                if (!first_set || !second_set)
                    throw std::runtime_error("Invalid pair");
            }

            static void write(const std::pair<T1, T2>& value, value_write_stream& write_stream) {
                write_stream.write_compound().write("first", value.first).write("second", value.second);
            }
        };

        template <class T>
        struct serialization<std::pair<std::string, T>> {
            static std::pair<std::string, T> read(value_read_stream& read_stream) {
                std::pair<std::string, T> res;
                read(res, read_stream);
                return res;
            }

            static void read(std::pair<std::string, T>& value, value_read_stream& read_stream) {
                bool first_set = false;
                bool second_set = false;
                size_t length = 0;
                read_stream.blind_iterate(
                    [&](std::uint64_t len) {
                        length = len;
                    },
                    [&](std::string_view name, value_read_stream& self) {
                        if (length == 1) {
                            value.first = name;
                            serialization<T>::read(value.second, self);
                            first_set = true;
                            second_set = true;
                            return;
                        }
                        if (name == "first") {
                            serialization<std::string>::read(value.first, self);
                            first_set = true;
                        } else if (name == "second") {
                            serialization<T>::read(value.second, self);
                            second_set = true;
                        }
                    },
                    [&](value_read_stream& self) {
                        if (!first_set)
                            serialization<std::string>::read(value.first, self);
                        if (!second_set)
                            serialization<T>::read(value.second, self);
                    }
                );
                if (!first_set || !second_set)
                    throw std::runtime_error("Invalid pair");
            }

            static void write(const std::pair<std::string, T>& value, value_write_stream& write_stream) {
                write_stream.write_compound().write(value.first, value.second);
            }
        };

        template <class T>
        struct serialization<std::vector<T>> {
            static std::vector<T> read(value_read_stream& read_stream) {
                std::vector<T> res;
                read(res, read_stream);
                return res;
            }

            static void read(std::vector<T>& value, value_read_stream& read_stream) {
                if constexpr (std::is_integral_v<T>) {
                    value = read_stream.template iterate_into<T>();
                } else if constexpr (serialization_simple_cast_data<T>::value) {
                    auto tmp = read_stream.template iterate_into<typename serialization_simple_cast<T>::type>();
                    value.clear();
                    value.reserve(tmp.size());
                    for (auto it : tmp)
                        value.push_back(serialization_simple_cast<T>::read_cast(it));
                } else {
                    read_stream.iterate(
                        [&](std::uint64_t len) { value.reserve(len); },
                        [&](value_read_stream& self) { value.push_back(serialization<T>::read(self)); }
                    );
                }
            }

            static void write(const std::vector<T>& value, value_write_stream& write_stream) {
                if constexpr (std::is_integral_v<T>)
                    write_stream.write_sarray<T>(value.size()).iterable(value);
                else if constexpr (serialization_simple_cast_data<T>::value)
                    write_stream.write_sarray<typename serialization_simple_cast_data<T>::type>(value.size()).iterable(value, [](const T& value) { return serialization_simple_cast<T>::write_cast(value); });
                else
                    write_stream.write_array(value.size()).iterable(value, [](const T& value, value_write_stream& write_stream) { serialization<T>::write(value, write_stream); });
            }
        };

        template <class T>
        struct serialization<std::unordered_map<std::string, T>> {
            static std::unordered_map<std::string, T> read(value_read_stream& read_stream) {
                std::unordered_map<std::string, T> res;
                read(res, read_stream);
                return res;
            }

            static void read(std::unordered_map<std::string, T>& value, value_read_stream& read_stream) {
                read_stream.iterate(
                    [&](std::uint64_t len) { value.reserve(len); },
                    [&](std::string_view name, value_read_stream& self) { value.emplace(name, serialization<T>::read(self)); }
                );
            }

            static void write(const std::unordered_map<std::string, T>& value, value_write_stream& write_stream) {
                write_stream.write_compound().iterable(value, [](const auto&, const T& value, value_write_stream& write_stream) { serialization<T>::write(value, write_stream); });
            }
        };

        template <class T, std::size_t N>
        struct serialization<std::array<T, N>> {
            static std::array<T, N> read(value_read_stream& read_stream) {
                std::array<T, N> res;
                read(res, read_stream);
                return res;
            }

            static void read(std::array<T, N>& value, value_read_stream& read_stream) {
                if constexpr (std::is_integral_v<T>) {
                    read_stream.iterate_into(value.data(), N);
                } else if constexpr (serialization_simple_cast_data<T>::value) {
                    std::vector<typename serialization_simple_cast_data<T>::type> tmp;
                    tmp.resize(N);
                    read_stream.iterate_into(tmp.data(), N);
                    for (size_t i = 0; i < N; i++)
                        value[i] = serialization_simple_cast<T>::read_cast(tmp[i]);
                } else {
                    size_t i = 0;
                    read_stream.iterate(
                        [](std::uint64_t len) {
                            if (len != N)
                                throw std::runtime_error("Invalid array size");
                        },
                        [&](value_read_stream& self) {
                            serialization<T>::read(value[i], self);
                            i++;
                        }
                    );
                }
            }

            static void write(const std::array<T, N>& value, value_write_stream& write_stream) {
                if constexpr (std::is_integral_v<T>)
                    write_stream.write_sarray<T>(N).iterable(value);
                else if constexpr (serialization_simple_cast_data<T>::value)
                    write_stream.write_sarray<typename serialization_simple_cast_data<T>::type>(N).iterable(value, [](const T& value) { return serialization_simple_cast<T>::write_cast(value); });
                else
                    write_stream.write_array(N).iterable(value, [](const T& value, value_write_stream& write_stream) { serialization<T>::write(value, write_stream); });
            }
        };

        template <class T, size_t N>
        struct serialization<T[N]> {
            static void read(T (&value)[N], value_read_stream& read_stream) {
                if constexpr (std::is_integral_v<T>) {
                    read_stream.iterate_into(value, N);
                } else if constexpr (serialization_simple_cast_data<T>::value) {
                    std::vector<typename serialization_simple_cast_data<T>::type> tmp;
                    tmp.resize(N);
                    read_stream.iterate_into(tmp.data(), N);
                    for (size_t i = 0; i < N; i++)
                        value[i] = serialization_simple_cast<T>::read_cast(tmp[i]);
                } else {
                    size_t i = 0;
                    read_stream.iterate(
                        [](std::uint64_t len) {
                            if (len != N)
                                throw std::runtime_error("Invalid array size");
                        },
                        [&](value_read_stream& self) {
                            serialization<T>::read(value[i], self);
                            i++;
                        }
                    );
                }
            }

            static void write(const T (&value)[N], value_write_stream& write_stream) {
                if constexpr (std::is_integral_v<T>)
                    write_stream.write_sarray_dir(value);
                else if constexpr (serialization_simple_cast_data<T>::value)
                    write_stream.write_sarray<typename serialization_simple_cast_data<T>::type>(N).iterable(value, [](const T& value) { return serialization_simple_cast<T>::write_cast(value); });
                else
                    write_stream.write_array(N).iterable(value, [](const T& value, value_write_stream& write_stream) { serialization<T>::write(value, write_stream); });
            }
        };

        template <class T>
        void serialization_read(T& value, value_read_stream& read_stream) {
            serialization<T>::read(value, read_stream);
        }

        template <class T>
        void serialization_write(const T& value, value_write_stream& write_stream) {
            serialization<T>::write(value, write_stream);
        }
    }
}

#endif /* ENBT_IO_TOOLS */
