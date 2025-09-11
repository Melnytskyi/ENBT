#ifndef ENBT_IO_TOOLS
#define ENBT_IO_TOOLS
#include "io.hpp"
#include <map>
#include <unordered_map>

namespace enbt {
    namespace io_helper {
        //this file provides serialization base for serializing and deserializing for io or for values
        //there no requirement for user support both operations for io and for values, if user uses only io then user should declare io ops


        //enables storage optimization for simple types in arrays
        template <class T, class = void>
        struct serialization_simple_cast {
            //to declare the type simple use:
            //using direct_type = `other type`;
            //the simple type should be same size as the other one and they are should be directly castable

            //to declare manual custom cast for types use, there's no type and size limitations
            //other_type write_cast(const T& value) {}
            //T read_cast(other_type&& value) {}
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
        concept simple_cast = requires(T val) { serialization_simple_cast<T>::write_cast(val); };

        template <class T>
        concept simple_cast_direct = requires(T val) { serialization_simple_cast<T>::direct_type; } && (sizeof(T) == sizeof(typename serialization_simple_cast<T>::direct_type));

        template <class T, class = void>
        struct serialization_simple_cast_data : public std::false_type {};

        template <class T>
            requires simple_cast<T>
        struct serialization_simple_cast_data<T> : public std::true_type {
            using type = decltype(serialization_simple_cast<T>::write_cast(std::declval<T>()));
        };

        template <class T>
            requires simple_cast_direct<T>
        struct serialization_simple_cast_data<T> : public std::true_type {
            using type = serialization_simple_cast<T>::direct_type;
        };

        //use own specialization for custom types and declare read and write ops
        template <class T>
        struct serialization {
            //this is optional
            static T read(value_read_stream& read_stream) {
                return T(read_stream.read());
            }

            static void read(T& value, value_read_stream& read_stream) {
                value = T(read_stream.read());
            }

            static void write(const T& value, value_write_stream& write_stream) {
                write_stream.write(value);
            }

            //this is optional
            static T read(const enbt::value& from) {
                return T(from);
            }

            static void read(T& value, const enbt::value& from) {
                value = T(from);
            }

            static void write(const T& value, enbt::value& to) {
                to = value;
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

            static std::unique_ptr<T> read(const enbt::value& read_stream) {
                return std::make_unique<T>(serialization<T>::read(read_stream));
            }

            static void read(std::unique_ptr<T>& value, const enbt::value& from) {
                value = std::make_unique<T>(serialization<T>::read(from));
            }

            static void write(const std::unique_ptr<T>& value, enbt::value& to) {
                serialization<T>::write(*value, to);
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

            static std::shared_ptr<T> read(const enbt::value& from) {
                return std::make_shared<T>(serialization<T>::read(from));
            }

            static void read(std::shared_ptr<T>& value, const enbt::value& from) {
                value = std::make_shared<T>(serialization<T>::read(from));
            }

            static void write(const std::shared_ptr<T>& value, enbt::value& to) {
                serialization<T>::write(*value, to);
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
                        if (!first_set) {
                            serialization<T1>::read(value.first, self);
                            first_set = true;
                        }
                        if (!second_set) {
                            serialization<T2>::read(value.second, self);
                            second_set = true;
                        }
                    }
                );
                if (!first_set || !second_set)
                    throw std::runtime_error("Invalid pair");
            }

            static void write(const std::pair<T1, T2>& value, value_write_stream& write_stream) {
                write_stream.write_compound().write("first", value.first).write("second", value.second);
            }

            static std::pair<T1, T2> read(const enbt::value& from) {
                std::pair<T1, T2> res;
                read(res, from);
                return res;
            }

            static void read(std::pair<T1, T2>& value, const enbt::value& from) {
                bool first_set = false;
                bool second_set = false;
                if (from.is_compound()) {
                    auto c = from.as_compound();
                    if (auto it = c.find("first"); it != c.end()) {
                        serialization<T1>::read(value.first, it->second);
                        first_set = true;
                    }

                    if (auto it = c.find("second"); it != c.end()) {
                        serialization<T2>::read(value.second, it->second);
                        second_set = true;
                    }
                } else if (from.is_array()) {
                    if (from.size() >= 2) {
                        serialization<T1>::read(value.first, from[0]);
                        serialization<T2>::read(value.second, from[1]);
                        return;
                    }
                } else if (from.is_sarray()) {
                    if (from.size() >= 2) {
                        serialization<T1>::read(value.first, from.get_index(0));
                        serialization<T2>::read(value.second, from.get_index(1));
                        return;
                    }
                }
                if (!first_set || !second_set)
                    throw std::runtime_error("Invalid pair");
            }

            static void write(const std::pair<T1, T2>& value, enbt::value& to) {
                enbt::compound res{
                    {"first", {}},
                    {"second", {}}
                };
                serialization<T1>::write(value.first, res["first"]);
                serialization<T1>::write(value.second, res["second"]);
                to = std::move(res);
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
                        if (!first_set) {
                            serialization<std::string>::read(value.first, self);
                            first_set = true;
                        }
                        if (!second_set) {
                            serialization<T>::read(value.second, self);
                            second_set = true;
                        }
                    }
                );
                if (!first_set || !second_set)
                    throw std::runtime_error("Invalid pair");
            }

            static void write(const std::pair<std::string, T>& value, value_write_stream& write_stream) {
                write_stream.write_compound().write(value.first, [&value](auto& stream) {
                    serialization<T>::read(value.second, stream);
                });
            }

            static std::pair<std::string, T> read(const enbt::value& from) {
                std::pair<std::string, T> res;
                read(res, from);
                return res;
            }

            static void read(std::pair<std::string, T>& value, const enbt::value& from) {
                bool first_set = false;
                bool second_set = false;
                if (from.is_compound()) {
                    auto c = from.as_compound();
                    if (c.size() == 1) {
                        auto it = c.begin();
                        value.first = it->first;
                        serialization<T>::read(value.second, it->second);
                        return;
                    }

                    if (auto it = c.find("first"); it != c.end()) {
                        value.first = it->second.convert_to_str();
                        first_set = true;
                    }

                    if (auto it = c.find("second"); it != c.end()) {
                        serialization<T>::read(value.second, it->second);
                        second_set = true;
                    }
                } else if (from.is_array()) {
                    if (from.size() >= 2) {
                        value.first = from[0].convert_to_str();
                        serialization<T>::read(value.second, from[1]);
                        return;
                    }
                }
                if (!first_set || !second_set)
                    throw std::runtime_error("Invalid pair");
            }

            static void write(const std::pair<std::string, T>& value, enbt::value& to) {
                enbt::compound res;
                serialization<T>::write(value.second, res[value.first]);
                to = std::move(res);
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
                    value.clear();
                    if constexpr (simple_cast_direct<T>) {
                        read_stream.template iterate_into<typename serialization_simple_cast<T>::type>(value);
                    } else {
                        auto tmp = read_stream.template iterate_into<typename serialization_simple_cast<T>::type>();
                        value.reserve(tmp.size());
                        for (auto it : tmp)
                            value.push_back(serialization_simple_cast<T>::read_cast(it));
                    }
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
                else if constexpr (serialization_simple_cast_data<T>::value) {
                    if constexpr (simple_cast_direct<T>)
                        write_stream.write_sarray<typename serialization_simple_cast_data<T>::type>(value.size()).iterable(value, [](const T& value) { return reinterpret_cast<const typename serialization_simple_cast_data<T>::type&>(value); });
                    else
                        write_stream.write_sarray<typename serialization_simple_cast_data<T>::type>(value.size()).iterable(value, [](const T& value) { return serialization_simple_cast<T>::write_cast(value); });
                } else
                    write_stream.write_array(value.size()).iterable(value, [](const T& value, value_write_stream& write_stream) { serialization<T>::write(value, write_stream); });
            }

            static std::vector<T> read(const enbt::value& from) {
                std::vector<T> res;
                read(res, from);
                return res;
            }

            static void read(std::vector<T>& value, const enbt::value& from) {
                value.clear();
                if (from.is_array()) {
                    auto arr = from.as_array();
                    if constexpr (serialization_simple_cast_data<T>::value) {
                        if constexpr (simple_cast_direct<T>) {
                            value.resize(arr.size());
                            size_t i = 0;
                            auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value.data());
                            for (auto& it : arr)
                                dir_cast[i++] = it;
                        } else {
                            value.reserve(arr.size());
                            for (auto& it : arr)
                                value.push_back(serialization_simple_cast<T>::read_cast(it));
                        }
                    } else {
                        size_t i = 0;
                        value.resize(arr.size());
                        for (auto& it : arr)
                            serialization<T>::read(value[i++], it);
                    }
                } else if (from.is_sarray()) {
                    if constexpr (serialization_simple_cast_data<T>::value) {
                        if constexpr (simple_cast_direct<T>) {
                            static auto applicator = []<class Arr>(std::vector<T>& value, Arr arr) {
                                value.resize(arr.size());
                                size_t i = 0;
                                auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value.data());
                                for (auto& it : arr)
                                    dir_cast[i++] = it;
                            };
                            switch (from.get_type_len()) {
                            case enbt::type_len::Tiny: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui8_array());
                                } else
                                    applicator(value, from.as_i8_array());
                                break;
                            }
                            case enbt::type_len::Short: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui16_array());
                                } else
                                    applicator(value, from.as_i16_array());
                                break;
                            }
                            case enbt::type_len::Default: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui32_array());
                                } else
                                    applicator(value, from.as_i32_array());
                                break;
                            }
                            case enbt::type_len::Long: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui64_array());
                                } else
                                    applicator(value, from.as_i64_array());
                                break;
                            }
                            default:
                                break;
                            }
                        } else {
                            static auto applicator = []<class Arr>(std::vector<T>& value, Arr arr) {
                                value.reserve(arr.size());
                                for (auto& it : arr)
                                    value.push_back(serialization_simple_cast<T>::read_cast(it));
                            };
                            switch (from.get_type_len()) {
                            case enbt::type_len::Tiny: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui8_array());
                                } else
                                    applicator(value, from.as_i8_array());
                                break;
                            }
                            case enbt::type_len::Short: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui16_array());
                                } else
                                    applicator(value, from.as_i16_array());
                                break;
                            }
                            case enbt::type_len::Default: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui32_array());
                                } else
                                    applicator(value, from.as_i32_array());
                                break;
                            }
                            case enbt::type_len::Long: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui64_array());
                                } else
                                    applicator(value, from.as_i64_array());
                                break;
                            }
                            default:
                                break;
                            }
                        }
                    } else {
                        static auto applicator = []<class Arr>(std::vector<T>& value, Arr arr) {
                            size_t i = 0;
                            value.resize(arr.size());
                            for (auto& it : arr)
                                serialization<T>::read(value[i++], it);
                        };
                        switch (from.get_type_len()) {
                        case enbt::type_len::Tiny: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui8_array());
                            } else
                                applicator(value, from.as_i8_array());
                            break;
                        }
                        case enbt::type_len::Short: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui16_array());
                            } else
                                applicator(value, from.as_i16_array());
                            break;
                        }
                        case enbt::type_len::Default: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui32_array());
                            } else
                                applicator(value, from.as_i32_array());
                            break;
                        }
                        case enbt::type_len::Long: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui64_array());
                            } else
                                applicator(value, from.as_i64_array());
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }

            static void write(const std::vector<T>& value, enbt::value& to) {
                if constexpr (serialization_simple_cast_data<T>::value) {
                    if constexpr (simple_cast_direct<T>) {
                        if constexpr (std::is_integral_v<typename serialization_simple_cast_data<T>::type>)
                            to = {reinterpret_cast<const typename serialization_simple_cast_data<T>::type*>(value.data()), value.size()};
                        else {
                            size_t i = 0;
                            enbt::fixed_array arr(value.size());
                            for (auto& it : value)
                                serialization<T>::write((const typename serialization_simple_cast_data<T>::type)it, arr[i++]);
                            to = std::move(arr);
                        }
                    } else {
                        size_t siz = value.size();
                        auto arr = [](size_t siz) {
                            if constexpr (std::is_integral_v<typename serialization_simple_cast_data<T>::type>)
                                return enbt::simple_array<T>(siz);
                            else
                                return enbt::fixed_array(siz);
                        }(siz);
                        for (size_t i = 0; i < siz; i++)
                            arr[i] = serialization_simple_cast<T>::write_cast(value[i]);
                        to = std::move(arr);
                    }
                } else
                    to = {to};
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
                value.clear();
                read_stream.iterate(
                    [&](std::uint64_t len) { value.reserve(len); },
                    [&](const std::string& name, value_read_stream& self) { serialization<T>::read(value[name], self); }
                );
            }

            static void write(const std::unordered_map<std::string, T>& value, value_write_stream& write_stream) {
                write_stream.write_compound().iterable(value, [](const auto&, const T& value, value_write_stream& write_stream) { serialization<T>::write(value, write_stream); });
            }

            static std::unordered_map<std::string, T> read(const enbt::value& from) {
                std::unordered_map<std::string, T> res;
                read(res, from);
                return res;
            }

            static void read(std::unordered_map<std::string, T>& value, const enbt::value& from) {
                auto comp = from.as_compound();
                value.clear();
                for (auto& [name, val] : comp)
                    serialization<T>::read(value[name], val);
            }

            static void write(const std::unordered_map<std::string, T>& value, enbt::value& to) {
                enbt::compound c;
                c.reserve(value.size());
                for (auto&& [name, val] : value)
                    serialization<T>::write(val, c[name]);
                to = std::move(c);
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
                    if constexpr (simple_cast_direct<T>) {
                        read_stream.iterate_into(value.data(), N);
                    } else {
                        std::vector<typename serialization_simple_cast_data<T>::type> tmp;
                        tmp.resize(N);
                        read_stream.iterate_into(tmp.data(), N);
                        for (size_t i = 0; i < N; i++)
                            value[i] = serialization_simple_cast<T>::read_cast(tmp[i]);
                    }
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
                else if constexpr (serialization_simple_cast_data<T>::value) {
                    if constexpr (simple_cast_direct<T>)
                        write_stream.write_sarray<typename serialization_simple_cast_data<T>::type>(N).iterable(value, [](const T& value) { return reinterpret_cast<const typename serialization_simple_cast_data<T>::type&>(value); });
                    else
                        write_stream.write_sarray<typename serialization_simple_cast_data<T>::type>(N).iterable(value, [](const T& value) { return serialization_simple_cast<T>::write_cast(value); });

                } else
                    write_stream.write_array(N).iterable(value, [](const T& value, value_write_stream& write_stream) { serialization<T>::write(value, write_stream); });
            }

            static std::array<T, N> read(const enbt::value& from) {
                std::array<T, N> res;
                read(res, from);
                return res;
            }

            static void read(std::array<T, N>& value, const enbt::value& from) {
                if (from.is_array()) {
                    auto arr = from.as_array();
                    size_t max_size = std::max<size_t>(arr.size(), N);
                    size_t i = 0;
                    if constexpr (serialization_simple_cast_data<T>::value) {
                        if constexpr (simple_cast_direct<T>) {
                            auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value.data());
                            for (auto& it : arr) {
                                if (max_size >= i)
                                    break;
                                dir_cast[i++] = it;
                            }
                        } else {
                            for (auto& it : arr) {
                                if (max_size >= i)
                                    break;
                                value[i++] = serialization_simple_cast<T>::read_cast(it);
                            }
                        }
                    } else {
                        for (auto& it : arr) {
                            if (max_size >= i)
                                break;
                            serialization<T>::read(value[i++], it);
                        }
                    }
                } else if (from.is_sarray()) {
                    if constexpr (serialization_simple_cast_data<T>::value) {
                        if constexpr (simple_cast_direct<T>) {
                            static auto applicator = []<class Arr>(std::array<T, N>& value, Arr arr) {
                                size_t max_size = std::max<size_t>(arr.size(), N);
                                size_t i = 0;
                                auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value.data());
                                for (auto& it : arr) {
                                    if (max_size >= i)
                                        break;
                                    dir_cast[i++] = it;
                                }
                            };
                            switch (from.get_type_len()) {
                            case enbt::type_len::Tiny: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui8_array());
                                } else
                                    applicator(value, from.as_i8_array());
                                break;
                            }
                            case enbt::type_len::Short: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui16_array());
                                } else
                                    applicator(value, from.as_i16_array());
                                break;
                            }
                            case enbt::type_len::Default: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui32_array());
                                } else
                                    applicator(value, from.as_i32_array());
                                break;
                            }
                            case enbt::type_len::Long: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui64_array());
                                } else
                                    applicator(value, from.as_i64_array());
                                break;
                            }
                            default:
                                break;
                            }
                        } else {
                            static auto applicator = []<class Arr>(std::array<T, N>& value, Arr arr) {
                                size_t max_size = std::max<size_t>(arr.size(), N);
                                size_t i = 0;
                                auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value.data());
                                for (auto& it : arr) {
                                    if (max_size >= i)
                                        break;
                                    dir_cast[i++] = serialization_simple_cast<T>::read_cast(it);
                                }
                            };
                            switch (from.get_type_len()) {
                            case enbt::type_len::Tiny: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui8_array());
                                } else
                                    applicator(value, from.as_i8_array());
                                break;
                            }
                            case enbt::type_len::Short: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui16_array());
                                } else
                                    applicator(value, from.as_i16_array());
                                break;
                            }
                            case enbt::type_len::Default: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui32_array());
                                } else
                                    applicator(value, from.as_i32_array());
                                break;
                            }
                            case enbt::type_len::Long: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui64_array());
                                } else
                                    applicator(value, from.as_i64_array());
                                break;
                            }
                            default:
                                break;
                            }
                        }
                    } else {
                        static auto applicator = []<class Arr>(std::array<T, N>& value, Arr arr) {
                            size_t max_size = std::max<size_t>(arr.size(), N);
                            size_t i = 0;
                            auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value.data());
                            for (auto& it : arr) {
                                if (max_size >= i)
                                    break;
                                serialization<T>::read(value[i++], it);
                            }
                        };
                        switch (from.get_type_len()) {
                        case enbt::type_len::Tiny: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui8_array());
                            } else
                                applicator(value, from.as_i8_array());
                            break;
                        }
                        case enbt::type_len::Short: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui16_array());
                            } else
                                applicator(value, from.as_i16_array());
                            break;
                        }
                        case enbt::type_len::Default: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui32_array());
                            } else
                                applicator(value, from.as_i32_array());
                            break;
                        }
                        case enbt::type_len::Long: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui64_array());
                            } else
                                applicator(value, from.as_i64_array());
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }

            static void write(const std::array<T, N>& value, enbt::value& to) {
                if constexpr (serialization_simple_cast_data<T>::value) {
                    if constexpr (simple_cast_direct<T>) {
                        if constexpr (std::is_integral_v<typename serialization_simple_cast_data<T>::type>)
                            to = {reinterpret_cast<const typename serialization_simple_cast_data<T>::type*>(value.data()), N};
                        else {
                            size_t i = 0;
                            enbt::fixed_array arr(N);
                            for (auto& it : value)
                                serialization<T>::write((const typename serialization_simple_cast_data<T>::type)it, arr[i++]);
                            to = std::move(arr);
                        }
                    } else {
                        auto arr = []() {
                            if constexpr (std::is_integral_v<typename serialization_simple_cast_data<T>::type>)
                                return enbt::simple_array<T>(N);
                            else
                                return enbt::fixed_array(N);
                        }();
                        for (size_t i = 0; i < N; i++)
                            arr[i] = serialization_simple_cast<T>::write_cast(value[i]);
                        to = std::move(arr);
                    }
                } else {
                    enbt::fixed_array arr(N);
                    for (size_t i = 0; i < N; i++)
                        serialization_simple_cast<T>::write(value[i], arr[i]);
                    to = std::move(arr);
                }
            }
        };

        template <class T, size_t N>
        struct serialization<T[N]> {
            static void read(T (&value)[N], value_read_stream& read_stream) {
                if constexpr (std::is_integral_v<T>) {
                    read_stream.iterate_into(value, N);
                } else if constexpr (serialization_simple_cast_data<T>::value) {
                    if constexpr (simple_cast_direct<T>) {
                        read_stream.iterate_into(value, N);
                    } else {
                        std::vector<typename serialization_simple_cast_data<T>::type> tmp;
                        tmp.resize(N);
                        read_stream.iterate_into(tmp.data(), N);
                        for (size_t i = 0; i < N; i++)
                            value[i] = serialization_simple_cast<T>::read_cast(tmp[i]);
                    }
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
                else if constexpr (serialization_simple_cast_data<T>::value) {
                    if constexpr (simple_cast_direct<T>)
                        write_stream.write_sarray<typename serialization_simple_cast_data<T>::type>(N).iterable(value, [](const T& value) { return reinterpret_cast<const typename serialization_simple_cast_data<T>::type&>(value); });
                    else
                        write_stream.write_sarray<typename serialization_simple_cast_data<T>::type>(N).iterable(value, [](const T& value) { return serialization_simple_cast<T>::write_cast(value); });
                } else
                    write_stream.write_array(N).iterable(value, [](const T& value, value_write_stream& write_stream) { serialization<T>::write(value, write_stream); });
            }

            static void read(T (&value)[N], const enbt::value& from) {
                if (from.is_array()) {
                    auto arr = from.as_array();
                    size_t max_size = std::max<size_t>(arr.size(), N);
                    size_t i = 0;
                    if constexpr (serialization_simple_cast_data<T>::value) {
                        if constexpr (simple_cast_direct<T>) {
                            auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value);
                            for (auto& it : arr) {
                                if (max_size >= i)
                                    break;
                                dir_cast[i++] = it;
                            }
                        } else {
                            for (auto& it : arr) {
                                if (max_size >= i)
                                    break;
                                value[i++] = serialization_simple_cast<T>::read_cast(it);
                            }
                        }
                    } else {
                        for (auto& it : arr) {
                            if (max_size >= i)
                                break;
                            serialization<T>::read(value[i++], it);
                        }
                    }
                } else if (from.is_sarray()) {
                    if constexpr (serialization_simple_cast_data<T>::value) {
                        if constexpr (simple_cast_direct<T>) {
                            static auto applicator = []<class Arr>(T(&value)[N], Arr arr) {
                                size_t max_size = std::max<size_t>(arr.size(), N);
                                size_t i = 0;
                                auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value);
                                for (auto& it : arr) {
                                    if (max_size >= i)
                                        break;
                                    dir_cast[i++] = it;
                                }
                            };
                            switch (from.get_type_len()) {
                            case enbt::type_len::Tiny: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui8_array());
                                } else
                                    applicator(value, from.as_i8_array());
                                break;
                            }
                            case enbt::type_len::Short: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui16_array());
                                } else
                                    applicator(value, from.as_i16_array());
                                break;
                            }
                            case enbt::type_len::Default: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui32_array());
                                } else
                                    applicator(value, from.as_i32_array());
                                break;
                            }
                            case enbt::type_len::Long: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui64_array());
                                } else
                                    applicator(value, from.as_i64_array());
                                break;
                            }
                            default:
                                break;
                            }
                        } else {
                            static auto applicator = []<class Arr>(T(&value)[N], Arr arr) {
                                size_t max_size = std::max<size_t>(arr.size(), N);
                                size_t i = 0;
                                auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value);
                                for (auto& it : arr) {
                                    if (max_size >= i)
                                        break;
                                    dir_cast[i++] = serialization_simple_cast<T>::read_cast(it);
                                }
                            };
                            switch (from.get_type_len()) {
                            case enbt::type_len::Tiny: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui8_array());
                                } else
                                    applicator(value, from.as_i8_array());
                                break;
                            }
                            case enbt::type_len::Short: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui16_array());
                                } else
                                    applicator(value, from.as_i16_array());
                                break;
                            }
                            case enbt::type_len::Default: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui32_array());
                                } else
                                    applicator(value, from.as_i32_array());
                                break;
                            }
                            case enbt::type_len::Long: {
                                if (from.get_type_sign()) {
                                    applicator(value, from.as_ui64_array());
                                } else
                                    applicator(value, from.as_i64_array());
                                break;
                            }
                            default:
                                break;
                            }
                        }
                    } else {
                        static auto applicator = []<class Arr>(const T(&value)[N], Arr arr) {
                            size_t max_size = std::max<size_t>(arr.size(), N);
                            size_t i = 0;
                            auto dir_cast = reinterpret_cast<typename serialization_simple_cast<T>::type*>(value);
                            for (auto& it : arr) {
                                if (max_size >= i)
                                    break;
                                serialization<T>::read(value[i++], it);
                            }
                        };
                        switch (from.get_type_len()) {
                        case enbt::type_len::Tiny: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui8_array());
                            } else
                                applicator(value, from.as_i8_array());
                            break;
                        }
                        case enbt::type_len::Short: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui16_array());
                            } else
                                applicator(value, from.as_i16_array());
                            break;
                        }
                        case enbt::type_len::Default: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui32_array());
                            } else
                                applicator(value, from.as_i32_array());
                            break;
                        }
                        case enbt::type_len::Long: {
                            if (from.get_type_sign()) {
                                applicator(value, from.as_ui64_array());
                            } else
                                applicator(value, from.as_i64_array());
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }

            static void write(const T (&value)[N], enbt::value& to) {
                if constexpr (serialization_simple_cast_data<T>::value) {
                    if constexpr (simple_cast_direct<T>) {
                        if constexpr (std::is_integral_v<typename serialization_simple_cast_data<T>::type>)
                            to = {reinterpret_cast<const typename serialization_simple_cast_data<T>::type*>(value), N};
                        else {
                            size_t i = 0;
                            enbt::fixed_array arr(N);
                            for (auto& it : value)
                                serialization<T>::write((const typename serialization_simple_cast_data<T>::type)it, arr[i++]);
                            to = std::move(arr);
                        }
                    } else {
                        auto arr = []() {
                            if constexpr (std::is_integral_v<typename serialization_simple_cast_data<T>::type>)
                                return enbt::simple_array<T>(N);
                            else
                                return enbt::fixed_array(N);
                        }();
                        for (size_t i = 0; i < N; i++)
                            arr[i] = serialization_simple_cast<T>::write_cast(value[i]);
                        to = std::move(arr);
                    }
                } else {
                    enbt::fixed_array arr(N);
                    for (size_t i = 0; i < N; i++)
                        serialization_simple_cast<T>::write(value[i], arr[i]);
                    to = std::move(arr);
                }
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

        template <class T>
        void serialization_read(T& value, const enbt::value& from) {
            serialization<T>::read(value, from);
        }

        template <class T>
        void serialization_write(const T& value, enbt::value& to) {
            serialization<T>::write(value, to);
        }
    }
}

#endif /* ENBT_IO_TOOLS */
