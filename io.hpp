#ifndef ENBT_IO
#define ENBT_IO
#include "enbt.hpp"
#include <functional>
#include <istream>
#include <sstream>
#include <type_traits>
#include <unordered_set>

namespace enbt {
    namespace io_helper {
        void write_compress_len(std::ostream& write_stream, std::uint64_t len);
        void write_type_id(std::ostream& write_stream, enbt::type_id tid);
        void write_string(std::ostream& write_stream, const value& val);
        void write_string(std::ostream& write_stream, std::string_view val);
        void write_define_len(std::ostream& write_stream, std::uint64_t len, enbt::type_id tid);
        void initialize_version(std::ostream& write_stream);
        void write_compound(std::ostream& write_stream, const value& val);
        void write_array(std::ostream& write_stream, const value& val);
        void write_darray(std::ostream& write_stream, const value& val);
        void write_simple_array(std::ostream& write_stream, const value& val);
        void write_log_item(std::ostream& write_stream, const value& val);
        void write_value(std::ostream& write_stream, const value& val);
        void write_token(std::ostream& write_stream, const value& val);

        inline void write_string(std::ostream& write_stream, const std::string& val) {
            write_string(write_stream, (std::string_view)val);
        }

        enbt::type_id read_type_id(std::istream& read_stream);
        std::size_t read_define_len(std::istream& read_stream, enbt::type_id tid);
        std::uint64_t read_define_len64(std::istream& read_stream, enbt::type_id tid);
        std::uint64_t read_compress_len(std::istream& read_stream);

        std::string read_string(std::istream& read_stream);
        value read_compound(std::istream& read_stream, enbt::type_id tid);
        std::vector<value> read_array(std::istream& read_stream, enbt::type_id tid);
        std::vector<value> read_darray(std::istream& read_stream, enbt::type_id tid);
        value read_sarray(std::istream& read_stream, enbt::type_id tid);
        value read_log_item(std::istream& read_stream);
        value read_value(std::istream& read_stream, enbt::type_id tid);
        value read_token(std::istream& read_stream);

        value read_file(std::istream& read_stream);
        std::vector<value> read_list_file(std::istream& read_stream);

        void check_version(std::istream& read_stream);


        //return zero if cannot, else return type size
        std::uint8_t can_fast_index(enbt::type_id tid);

        void skip_compound(std::istream& read_stream, enbt::type_id tid);
        void skip_array(std::istream& read_stream, enbt::type_id tid);
        void skip_darray(std::istream& read_stream, enbt::type_id tid);
        void skip_sarray(std::istream& read_stream, enbt::type_id tid);
        void skip_string(std::istream& read_stream);
        void skip_log_item(std::istream& read_stream);
        void skip_value(std::istream& read_stream, enbt::type_id tid);
        void skip_token(std::istream& read_stream);


        //move read stream cursor to value in compound, return true if value found
        bool find_value_compound(std::istream& read_stream, enbt::type_id tid, std::string_view key);
        void index_static_array(std::istream& read_stream, std::uint64_t index, std::uint64_t len, enbt::type_id target_id);
        void index_dyn_array(std::istream& read_stream, std::uint64_t index, std::uint64_t len);
        std::optional<enbt::type_id> index_array(std::istream& read_stream, std::uint64_t index, enbt::type_id arr_tid);
        std::optional<enbt::type_id> index_array(std::istream& read_stream, std::uint64_t index);

        struct value_path {
            struct index {
                std::variant<std::string, std::uint64_t> value;
                operator std::string() const;
                operator std::uint64_t() const;
            };

            std::vector<index> path;

            value_path(std::string_view stringized_path); //legacy
            value_path(const std::vector<index>& copy);
            value_path(std::vector<index>&& move);
            value_path(const value_path& copy);
            value_path(value_path&& move) noexcept;

            value_path&& operator[](std::string_view index);
            value_path&& operator[](std::uint64_t index);
        };

        //move read_stream cursor to value,
        //value_path similar: "0/the test/4/54",
        //return the type_id if the value is found and empty optional if not
        //can throw enbt::exception
        std::optional<enbt::type_id> move_to_value_path(std::istream& read_stream, const value_path& value_path);
        std::optional<enbt::type_id> move_to_value_path(std::istream& read_stream, const value_path& value_path, enbt::type_id current_id);
        value get_value_path(std::istream& read_stream, const value_path& value_path);

        //reader class for reading from stream without allocations,
        // all functions except peek_* is final and do not preserve position of read_stream, and must be used once for lifetime of value_read_stream
        // peek_* functions require istream to be seekable
        class value_read_stream {
            std::istream& read_stream;

            enbt::type_id current_type_id;
            bool readed = false;
            int8_t bit_value = -1; //-1 set, 0 and 1 is values

            value_read_stream(std::istream& read_stream, enbt::type_id current_type_id)
                : read_stream(read_stream), current_type_id(current_type_id) {}

            value_read_stream(std::istream& read_stream, enbt::type_id current_type_id, uint8_t bit_value)
                : read_stream(read_stream), current_type_id(current_type_id), bit_value((int8_t)bit_value) {}

            void check_io_state();

        public:
            value_read_stream(std::istream& read_stream);
            ~value_read_stream();
            enbt::value read(); //default read
            value_read_stream& read_into(bool& res);
            value_read_stream& read_into(uint8_t& res);
            value_read_stream& read_into(uint16_t& res);
            value_read_stream& read_into(uint32_t& res);
            value_read_stream& read_into(uint64_t& res);
            value_read_stream& read_into(int8_t& res);
            value_read_stream& read_into(int16_t& res);
            value_read_stream& read_into(int32_t& res);
            value_read_stream& read_into(int64_t& res);
            value_read_stream& read_into(float& res);
            value_read_stream& read_into(double& res);
            value_read_stream& read_into(enbt::raw_uuid& res);
            value_read_stream& read_into(std::string& res);
            value_read_stream& read_as(bool& res);
            value_read_stream& read_as(uint8_t& res);
            value_read_stream& read_as(uint16_t& res);
            value_read_stream& read_as(uint32_t& res);
            value_read_stream& read_as(uint64_t& res);
            value_read_stream& read_as(int8_t& res);
            value_read_stream& read_as(int16_t& res);
            value_read_stream& read_as(int32_t& res);
            value_read_stream& read_as(int64_t& res);
            value_read_stream& read_as(float& res);
            value_read_stream& read_as(double& res);
            value_read_stream& read_as(enbt::raw_uuid& res);
            value_read_stream& read_as(std::string& res);

            void skip();

            enbt::type_id get_type_id() const {
                return current_type_id;
            }

            class peek_stream {
                std::istream& read_stream;
                enbt::type_id current_type_id;
                std::streampos pos;
                int8_t bit_value;

            public:
                peek_stream(std::istream& read_stream, enbt::type_id current_type_id, int8_t bit_value) : read_stream(read_stream), current_type_id(current_type_id), bit_value(bit_value) {
                    pos = read_stream.tellg();
                }

                template <class FN>
                peek_stream& peek(FN&& fn)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    auto old_pos = read_stream.tellg();
                    try {
                        if (old_pos != pos)
                            read_stream.seekg(pos);
                        if (bit_value == -1) {
                            value_read_stream stream(read_stream, current_type_id);
                            fn(stream);
                        } else {
                            value_read_stream stream(read_stream, current_type_id, (uint8_t)bit_value);
                            fn(stream);
                        }
                    } catch (...) {
                        read_stream.seekg(old_pos);
                        throw;
                    }
                    read_stream.seekg(old_pos);
                    return *this;
                }

                template <class FN>
                peek_stream& peek_at(const value_path& value_path, FN&& fn)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    peek_at(value_path, fn, []() {});
                    return *this;
                }

                template <class FN, class FN_nf>
                peek_stream& peek_at(const value_path& value_path, FN&& fn, FN_nf&& not_found)
                    requires(std::is_invocable_v<FN, value_read_stream&> && std::is_invocable_v<FN_nf>)
                {
                    auto old_pos = read_stream.tellg();
                    try {
                        if (old_pos != pos)
                            read_stream.seekg(pos);
                        auto found_id = move_to_value_path(read_stream, value_path, current_type_id);
                        if (found_id) {
                            if (bit_value == -1) {
                                value_read_stream stream(read_stream, *found_id);
                                fn(stream);
                            } else {
                                value_read_stream stream(read_stream, *found_id, (uint8_t)bit_value);
                                fn(stream);
                            }
                        } else
                            not_found();
                    } catch (...) {
                        read_stream.seekg(old_pos);
                        throw;
                    }
                    read_stream.seekg(old_pos);
                    return *this;
                }

                template <class FN>
                peek_stream& peek_at(std::uint64_t index, FN&& callback)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    auto old_pos = read_stream.tellg();
                    try {
                        if (old_pos != pos)
                            read_stream.seekg(pos);
                        auto res = index_array(read_stream, index, current_type_id);
                        if (!res)
                            res = read_type_id(read_stream);
                        value_read_stream stream(read_stream, *res);
                        callback(stream);
                        read_stream.seekg(old_pos);
                    } catch (...) {
                        read_stream.seekg(old_pos);
                        throw;
                    }
                    return *this;
                }

                template <class FN>
                peek_stream& peek_at(const std::string& chr, FN&& callback)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    return peek_at(chr, callback, []() {
                        throw enbt::exception("Key not found");
                    });
                }

                template <class FN, class FN_nf>
                peek_stream& peek_at(const std::string& chr, FN&& callback, FN_nf&& not_found_callback)
                    requires(std::is_invocable_v<FN, value_read_stream&> && std::is_invocable_v<FN_nf>)
                {
                    auto old_pos = read_stream.tellg();
                    try {
                        if (find_value_compound(read_stream, current_type_id, chr)) {
                            value_read_stream stream(read_stream);
                            callback(stream);
                            read_stream.seekg(old_pos);
                        } else
                            not_found_callback();
                    } catch (...) {
                        read_stream.seekg(old_pos);
                        throw;
                    }
                    return *this;
                }
            };

            class darray {
                std::istream& read_stream;
                std::size_t current_item = 0;
                std::size_t items = 0;
                enbt::type_id current_type_id;

            public:
                darray(std::istream& read_stream, enbt::type_id current_type_id);

                ~darray();

                size_t size() const noexcept;
                size_t current_index() const noexcept;
                std::vector<enbt::value> read();
                enbt::value read_one();
                darray& read_one_into(bool& res);
                darray& read_one_into(uint8_t& res);
                darray& read_one_into(uint16_t& res);
                darray& read_one_into(uint32_t& res);
                darray& read_one_into(uint64_t& res);
                darray& read_one_into(int8_t& res);
                darray& read_one_into(int16_t& res);
                darray& read_one_into(int32_t& res);
                darray& read_one_into(int64_t& res);
                darray& read_one_into(float& res);
                darray& read_one_into(double& res);
                darray& read_one_into(enbt::raw_uuid& res);
                darray& read_one_into(std::string& res);
                darray& read_one_as(bool& res);
                darray& read_one_as(uint8_t& res);
                darray& read_one_as(uint16_t& res);
                darray& read_one_as(uint32_t& res);
                darray& read_one_as(uint64_t& res);
                darray& read_one_as(int8_t& res);
                darray& read_one_as(int16_t& res);
                darray& read_one_as(int32_t& res);
                darray& read_one_as(int64_t& res);
                darray& read_one_as(float& res);
                darray& read_one_as(double& res);
                darray& read_one_as(enbt::raw_uuid& res);
                darray& read_one_as(std::string& res);

                template <class FN>
                darray& read_one(FN&& fn)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    if (current_item == items)
                        throw std::out_of_range("Tried to read value out of arrays range.");
                    value_read_stream inner(read_stream);
                    fn(inner);
                    current_item++;
                    return *this;
                }

                template <class FN>
                darray& iterable(FN&& fn)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    while (current_item != items)
                        read_one(fn);
                    return *this;
                }
            };

            class array {
                std::istream& read_stream;
                std::size_t current_item = 0;
                std::size_t items = 0;
                enbt::type_id arr_type_id;
                enbt::type_id current_type_id;
                uint8_t current_byte = 0;

            public:
                array(std::istream& read_stream, enbt::type_id arr_type_id);
                ~array();
                size_t size() const noexcept;
                size_t current_index() const noexcept;
                std::vector<enbt::value> read();
                enbt::value read_one();
                array& read_one_into(bool& res);
                array& read_one_into(uint8_t& res);
                array& read_one_into(uint16_t& res);
                array& read_one_into(uint32_t& res);
                array& read_one_into(uint64_t& res);
                array& read_one_into(int8_t& res);
                array& read_one_into(int16_t& res);
                array& read_one_into(int32_t& res);
                array& read_one_into(int64_t& res);
                array& read_one_into(float& res);
                array& read_one_into(double& res);
                array& read_one_into(enbt::raw_uuid& res);
                array& read_one_into(std::string& res);
                array& read_one_as(bool& res);
                array& read_one_as(uint8_t& res);
                array& read_one_as(uint16_t& res);
                array& read_one_as(uint32_t& res);
                array& read_one_as(uint64_t& res);
                array& read_one_as(int8_t& res);
                array& read_one_as(int16_t& res);
                array& read_one_as(int32_t& res);
                array& read_one_as(int64_t& res);
                array& read_one_as(float& res);
                array& read_one_as(double& res);
                array& read_one_as(enbt::raw_uuid& res);
                array& read_one_as(std::string& res);

                template <class FN>
                array& read_one(FN&& fn)
                    requires std::is_invocable_v<FN, value_read_stream&>
                {
                    if (current_item == items)
                        throw std::out_of_range("Tried to read value out of arrays range.");
                    if (current_item % 8 == 0)
                        read_stream.read((char*)&current_byte, 1);
                    if (current_type_id.type == type::bit) {
                        value_read_stream stream(read_stream, current_type_id, (int8_t)bool(current_byte & uint8_t(current_item % 8)));
                        fn(stream);
                    } else {
                        value_read_stream stream(read_stream, current_type_id);
                        fn(stream);
                    }
                    current_item++;
                    return *this;
                }

                template <class FN>
                array& iterable(FN&& fn)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    while (current_item != items)
                        read_one(fn);
                    return *this;
                }
            };

            template <class T>
            class sarray {
                std::istream& read_stream;
                std::size_t current_item = 0;
                std::size_t items = 0;
                enbt::type_id current_type_id;

                sarray(std::istream& read_stream, enbt::type_id current_type_id)
                    : read_stream(read_stream), current_type_id(current_type_id) {}

            public:
                sarray make_sarray(std::istream& read_stream, enbt::type_id check) {
                    static constexpr enbt::type_id check_s = simple_array<T>::enbt_type;
                    if (check.length != check_s.length || check.type != check_s.type || check.is_signed != check_s.is_signed)
                        throw std::invalid_argument("Type mismatch");
                    sarray res(read_stream, check);
                    res.items = read_compress_len(read_stream);
                    return res;
                }

                ~sarray() {
                    if (std::uncaught_exceptions())
                        return;
                    if (current_item != items)
                        read_stream.seekg((items - current_item) * sizeof(T), std::istream::cur);
                }

                size_t size() const noexcept {
                    return items;
                }

                size_t current_index() const noexcept {
                    return current_item;
                }

                std::vector<T> read() {
                    std::vector<T> res;
                    res.reserve(items - current_item);
                    iterable([&res](T item) {
                        res.push_back(item);
                    });
                    return res;
                }

                T peek_at(std::size_t index) {
                    if (items <= index)
                        throw std::out_of_range("Index points out of the array range");
                    auto old_pos = read_stream.tellg();
                    T val;
                    try {
                        read_stream.seekg(old_pos + std::streampos(index * sizeof(T)) - std::streampos(current_item * sizeof(T)));
                        read_stream.read((char*)&val, sizeof(T));
                    } catch (...) {
                        read_stream.seekg(old_pos);
                        throw;
                    }
                    read_stream.seekg(old_pos);
                    val = enbt::endian_helpers::convert_endian(current_type_id.get_endian(), val);
                    return val;
                }

                T read_one() {
                    if (current_item == items)
                        throw std::out_of_range("Tried to read value out of arrays range.");
                    T val;
                    read_stream.read((char*)&val, sizeof(T));
                    val = enbt::endian_helpers::convert_endian(current_type_id.get_endian(), val);
                    ++current_item;
                    return val;
                }

                template <class FN>
                darray& read_one(FN&& fn)
                    requires(std::is_invocable_v<FN, T>)
                {
                    fn(read_one());
                    return *this;
                }

                template <class FN>
                darray& iterable(FN&& fn)
                    requires(std::is_invocable_v<FN, T>)
                {
                    while (current_item != items)
                        read_one(fn);
                    return *this;
                }
            };

            class compound {
                std::istream& read_stream;
                std::size_t current_item = 0;
                std::size_t items = 0;
                enbt::type_id current_type_id;
                bool enable_collector_strict_order = false;

                std::unordered_map<std::string, std::function<void(value_read_stream&)>> automated_collector;
                std::vector<std::string> collector_strict_order_data;

            public:
                compound(std::istream& read_stream, enbt::type_id current_type_id, bool enable_collector_strict_order);
                ~compound();
                size_t size() const noexcept;
                size_t current_index() const noexcept;
                std::pair<std::string, enbt::value> read();

                template <class FN>
                compound& read(FN&& fn)
                    requires(std::is_invocable_v<FN, std::string&, value_read_stream&>)
                {
                    if (current_item == items)
                        throw std::out_of_range("Tried to read value out of compounds range.");
                    auto str = read_string(read_stream);
                    value_read_stream inner(read_stream);
                    fn(str, inner);
                    current_item++;
                    return *this;
                }

                template <class FN>
                compound& iterable(FN&& fn)
                    requires(std::is_invocable_v<FN, std::string&, value_read_stream&>)
                {
                    while (current_item != items)
                        read(fn);
                    return *this;
                }

                template <class FN>
                compound& collect(const std::string& name, FN&& fn)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    automated_collector[name] = std::forward<FN>(fn);
                    if (enable_collector_strict_order)
                        collector_strict_order_data.push_back(name);
                    return *this;
                }

                compound& collect_into(const std::string& name, bool& res);
                compound& collect_into(const std::string& name, uint8_t& res);
                compound& collect_into(const std::string& name, uint16_t& res);
                compound& collect_into(const std::string& name, uint32_t& res);
                compound& collect_into(const std::string& name, uint64_t& res);
                compound& collect_into(const std::string& name, int8_t& res);
                compound& collect_into(const std::string& name, int16_t& res);
                compound& collect_into(const std::string& name, int32_t& res);
                compound& collect_into(const std::string& name, int64_t& res);
                compound& collect_into(const std::string& name, float& res);
                compound& collect_into(const std::string& name, double& res);
                compound& collect_into(const std::string& name, enbt::raw_uuid& res);
                compound& collect_into(const std::string& name, std::string& res);
                compound& collect_as(const std::string& name, bool& res);
                compound& collect_as(const std::string& name, uint8_t& res);
                compound& collect_as(const std::string& name, uint16_t& res);
                compound& collect_as(const std::string& name, uint32_t& res);
                compound& collect_as(const std::string& name, uint64_t& res);
                compound& collect_as(const std::string& name, int8_t& res);
                compound& collect_as(const std::string& name, int16_t& res);
                compound& collect_as(const std::string& name, int32_t& res);
                compound& collect_as(const std::string& name, int64_t& res);
                compound& collect_as(const std::string& name, float& res);
                compound& collect_as(const std::string& name, double& res);
                compound& collect_as(const std::string& name, enbt::raw_uuid& res);
                compound& collect_as(const std::string& name, std::string& res);

                template <class FN>
                compound& collect_iterate(const std::string& name, FN&& fn)
                    requires(
                        std::is_invocable_v<FN, value_read_stream&>
                        || std::is_invocable_v<FN, std::string_view, value_read_stream&>
                        || std::is_invocable_v<FN, const std::string&, value_read_stream&>
                    )
                {
                    automated_collector[name] = [fn](value_read_stream& stream) {
                        stream.iterate(fn);
                    };
                    if (enable_collector_strict_order)
                        collector_strict_order_data.push_back(name);
                    return *this;
                }

                template <class SIZE_FN, class FN>
                compound& collect_iterate(const std::string& name, SIZE_FN&& fn_size, FN&& fn)
                    requires(
                        (
                            std::is_invocable_v<FN, value_read_stream&>
                            || std::is_invocable_v<FN, std::string_view, value_read_stream&>
                            || std::is_invocable_v<FN, const std::string&, value_read_stream&>
                        )
                        && std::is_invocable_v<SIZE_FN, uint64_t>
                    )
                {
                    automated_collector[name] = [fn, fn_size](value_read_stream& stream) {
                        stream.iterate(fn_size, fn);
                    };
                    if (enable_collector_strict_order)
                        collector_strict_order_data.push_back(name);
                    return *this;
                }

                template <class FN>
                compound& make_collect(FN&& on_uncollected)
                    requires(std::is_invocable_v<FN, std::string&, value_read_stream&>)
                {
                    if (!enable_collector_strict_order)
                        return iterable([this, &on_uncollected](std::string& name, value_read_stream& stream) {
                            if (auto it = automated_collector.find(name); it != automated_collector.end())
                                it->second(stream);
                            else
                                on_uncollected(name, stream);
                        });
                    else
                        return iterable([this, &on_uncollected, order = size_t(0)](std::string& name, value_read_stream& stream) mutable {
                            if (auto& excepted = collector_strict_order_data.at(order++); excepted != name)
                                throw enbt::exception("Invalid order, excepted: " + excepted + ", but got: " + name);
                            automated_collector.at(name)(stream);
                        });
                }

                compound& make_collect();
                compound& force_all_collect();
            };

            darray read_darray();
            array read_array();
            compound read_compound(bool enable_collector_strict_order = false);

            template <class FN>
            void read_optional(FN&& on_value)
                requires(std::is_invocable_v<FN, value_read_stream&>)
            {
                if (current_type_id.type != enbt::type::optional)
                    throw std::invalid_argument("Type mismatch");
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                readed = true;
                if (current_type_id.is_signed) {
                    value_read_stream stream(read_stream);
                    on_value(stream);
                }
            }

            template <class FN, class FN_nv>
            void read_optional(FN&& on_value, FN_nv&& on_no_value)
                requires(std::is_invocable_v<FN, value_read_stream&> && std::is_invocable_v<FN_nv>)
            {
                if (current_type_id.type != enbt::type::optional)
                    throw std::invalid_argument("Type mismatch");
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                readed = true;
                if (current_type_id.is_signed) {
                    value_read_stream stream(read_stream);
                    on_value(stream);
                } else
                    on_no_value();
            }

            template <class T>
            sarray<T> read_sarray() {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                readed = true;
                return sarray<T>::make_sarray(read_stream, current_type_id);
            }

            peek_stream make_peek();

            template <class FN>
            value_read_stream& make_peek(FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&>)
            {
                make_peek().peek(callback);
                return *this;
            }

            template <class FN>
            value_read_stream& peek_at(std::uint64_t index, FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&>)
            {
                make_peek().peek_at(index, callback);
                return *this;
            }

            template <class FN>
            value_read_stream& peek_at(const std::string& chr, FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&>)
            {
                make_peek().peek_at(chr, callback);
                return *this;
            }

            template <class FN>
            value_read_stream& peek_at(const value_path& value_path, FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&>)
            {
                make_peek().peek_at(value_path, callback);
                return *this;
            }

            size_t peek_size();

            template <class FN>
            void iterate(FN&& callback)
                requires(std::is_invocable_v<FN, std::string_view, value_read_stream&> || std::is_invocable_v<FN, const std::string&, value_read_stream&>)
            {
                iterate([](std::uint64_t) {}, std::move(callback));
            }

            template <class FN>
            void iterate(FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&>)
            {
                iterate([](std::uint64_t) {}, std::move(callback));
            }

            template <class FN>
            void peek_iterate(FN&& callback)
                requires(std::is_invocable_v<FN, std::string_view, value_read_stream&> || std::is_invocable_v<FN, const std::string&, value_read_stream&>)
            {
                peek_iterate([](std::uint64_t) {}, std::move(callback));
            }

            template <class FN>
            void peek_iterate(FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&>)
            {
                peek_iterate([](std::uint64_t) {}, std::move(callback));
            }

            template <class T>
            void iterate_into(T* arr, size_t size) {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                if (current_type_id.type == enbt::type::sarray) {
                    if (simple_array<T>::enbt_type.length != current_type_id.length)
                        throw enbt::exception("Type mismatch");
                    std::uint64_t len = read_compress_len(read_stream);
                    if (len != size)
                        throw std::out_of_range("Invalid array size");
                    read_stream.read((char*)arr, size * sizeof(T));
                    readed = true;
                    enbt::endian_helpers::convert_endian_arr(current_type_id.get_endian(), arr, size);
                } else {
                    size_t index = 0;
                    iterate(
                        [&](size_t len) {
                            if (len != size)
                                throw std::runtime_error("Invalid array size");
                        },
                        [&](value_read_stream& self) {
                            arr[index++] = self.read();
                        }
                    );
                }
            }

            template <class T>
            void peek_iterate_into(T* arr, size_t size) {
                auto old_pos = read_stream.tellg();
                try {
                    iterate_into(arr, size);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
                readed = false;
            }

            template <class T>
            void iterate_into(std::vector<T>& arr) {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                if (current_type_id.type == enbt::type::sarray) {
                    if (simple_array<T>::enbt_type.length != current_type_id.length)
                        throw enbt::exception("Type mismatch");
                    std::uint64_t len = read_compress_len(read_stream);
                    arr.resize(len);
                    read_stream.read((char*)arr.data(), len * sizeof(T));
                    readed = true;
                    enbt::endian_helpers::convert_endian_arr(current_type_id.get_endian(), arr);
                } else {
                    iterate(
                        [&](size_t len) {
                            arr.reserve(len);
                        },
                        [&](value_read_stream& self) {
                            arr.emplace_back(self.read());
                        }
                    );
                }
            }

            template <class T>
            void peek_iterate_into(std::vector<T>& arr) {
                auto old_pos = read_stream.tellg();
                try {
                    iterate_into<T>(arr);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }

            template <class T>
            std::vector<T> iterate_into() {
                std::vector<T> res;
                iterate_into(res);
                return res;
            }

            template <class T>
            std::vector<T> peek_iterate_into() {
                std::vector<T> res;
                peek_iterate_into(res);
                return res;
            }

            template <class SIZE_FN, class FN>
            void iterate(SIZE_FN&& size_callback, FN&& callback)
                requires((std::is_invocable_v<FN, std::string_view, value_read_stream&> || std::is_invocable_v<FN, const std::string&, value_read_stream&>) && std::is_invocable_v<SIZE_FN, uint64_t>)
            {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                if (current_type_id.type == enbt::type::compound) {
                    std::uint64_t len = read_define_len64(read_stream, current_type_id);
                    size_callback(len);
                    for (std::uint64_t i = 0; i < len; i++) {
                        auto name = read_string(read_stream);
                        value_read_stream stream(read_stream);
                        callback(name, stream);
                    }
                    readed = true;
                } else
                    throw std::invalid_argument("not compound type");
            }

            template <class SIZE_FN, class FN>
            void peek_iterate(SIZE_FN&& size_callback, FN&& callback)
                requires((std::is_invocable_v<FN, std::string_view, value_read_stream&> || std::is_invocable_v<FN, const std::string&, value_read_stream&>) && std::is_invocable_v<SIZE_FN, uint64_t>)
            {
                auto old_pos = read_stream.tellg();
                try {
                    iterate(size_callback, callback);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }

            template <class SIZE_FN, class FN>
            void iterate(SIZE_FN&& size_callback, FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&> && std::is_invocable_v<SIZE_FN, uint64_t>)
            {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                if (current_type_id.type == enbt::type::array) {
                    std::uint64_t len = read_define_len64(read_stream, current_type_id);
                    size_callback(len);
                    if (len) {
                        auto target_id = read_type_id(read_stream);
                        for (std::uint64_t i = 0; i < len; i++) {
                            value_read_stream stream(read_stream, target_id);
                            callback(stream);
                        }
                    }
                } else if (current_type_id.type == enbt::type::darray) {
                    std::uint64_t len = read_define_len64(read_stream, current_type_id);
                    size_callback(len);
                    for (std::uint64_t i = 0; i < len; i++) {
                        value_read_stream stream(read_stream);
                        callback(stream);
                    }
                } else if (current_type_id.type == enbt::type::sarray) {
                    std::uint64_t len = read_compress_len(read_stream);
                    size_callback(len);
                    for (std::uint64_t i = 0; i < len; i++) {
                        value_read_stream stream(read_stream, enbt::type_id(enbt::type::integer, current_type_id.length, current_type_id.endian, current_type_id.is_signed));
                        callback(stream);
                    }
                } else
                    throw std::invalid_argument("not array type");
                readed = true;
            }

            template <class SIZE_FN, class FN>
            void peek_iterate(SIZE_FN&& size_callback, FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&> && std::is_invocable_v<SIZE_FN, uint64_t>)
            {
                auto old_pos = read_stream.tellg();
                try {
                    iterate(size_callback, callback);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }

            template <class FN>
            void join_log_item(FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&>)
            {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                if (current_type_id.type == enbt::type::log_item) {
                    read_compress_len(read_stream);
                    value_read_stream stream(read_stream);
                    callback(stream);
                    readed = true;
                } else
                    throw std::invalid_argument("not log_item type");
            }

            template <class FN>
            void peek_join_log_item(FN&& callback)
                requires(std::is_invocable_v<FN, value_read_stream&>)
            {
                auto old_pos = read_stream.tellg();
                try {
                    join_log_item(callback);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }

            template <class COMPOUND_FN, class ARRAY_FN>
            void blind_iterate(
                COMPOUND_FN&& compound,
                ARRAY_FN&& array_or_log_item
            )
                requires(std::is_invocable_v<ARRAY_FN, value_read_stream&> && (std::is_invocable_v<COMPOUND_FN, std::string_view, value_read_stream&> || std::is_invocable_v<COMPOUND_FN, const std::string&, value_read_stream&>))
            {
                blind_iterate([](std::uint64_t) {}, std::move(compound), std::move(array_or_log_item));
            }

            template <class SIZE_FN, class COMPOUND_FN, class ARRAY_FN>
            void blind_iterate(
                SIZE_FN&& size_callback, COMPOUND_FN&& compound, ARRAY_FN&& array_or_log_item
            )
                requires(std::is_invocable_v<ARRAY_FN, value_read_stream&> && (std::is_invocable_v<COMPOUND_FN, std::string_view, value_read_stream&> || std::is_invocable_v<COMPOUND_FN, const std::string&, value_read_stream&>) && std::is_invocable_v<SIZE_FN, uint64_t>)
            {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                if (current_type_id.type == enbt::type::compound) {
                    std::uint64_t len = read_define_len64(read_stream, current_type_id);
                    size_callback(len);
                    for (std::uint64_t i = 0; i < len; i++) {
                        auto name = read_string(read_stream);
                        value_read_stream stream(read_stream);
                        compound(name, stream);
                    }
                } else if (current_type_id.type == enbt::type::array) {
                    std::uint64_t len = read_define_len64(read_stream, current_type_id);
                    size_callback(len);
                    if (len) {
                        auto target_id = read_type_id(read_stream);
                        for (std::uint64_t i = 0; i < len; i++) {
                            value_read_stream stream(read_stream, target_id);
                            array_or_log_item(stream);
                        }
                    }
                } else if (current_type_id.type == enbt::type::darray) {
                    std::uint64_t len = read_define_len64(read_stream, current_type_id);
                    size_callback(len);
                    for (std::uint64_t i = 0; i < len; i++) {
                        value_read_stream stream(read_stream);
                        array_or_log_item(stream);
                    }
                } else if (current_type_id.type == enbt::type::sarray) {
                    std::uint64_t len = read_compress_len(read_stream);
                    size_callback(len);
                    for (std::uint64_t i = 0; i < len; i++) {
                        value_read_stream stream(read_stream, enbt::type_id(enbt::type::integer, current_type_id.length, current_type_id.endian, current_type_id.is_signed));
                        array_or_log_item(stream);
                    }
                } else if (current_type_id.type == enbt::type::log_item) {
                    read_compress_len(read_stream);
                    value_read_stream stream(read_stream);
                    size_callback(1);
                    array_or_log_item(stream);
                } else
                    throw std::invalid_argument("non iterable type");
                readed = true;
            }

            template <class COMPOUND_FN, class ARRAY_FN>
            void peek_blind_iterate(COMPOUND_FN&& compound, ARRAY_FN&& array_or_log_item)
                requires(std::is_invocable_v<ARRAY_FN, value_read_stream&> && (std::is_invocable_v<COMPOUND_FN, std::string_view, value_read_stream&> || std::is_invocable_v<COMPOUND_FN, const std::string&, value_read_stream&>))
            {
                auto old_pos = read_stream.tellg();
                try {
                    blind_iterate([](std::uint64_t) {}, std::move(compound), std::move(array_or_log_item));
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }

            template <class SIZE_FN, class COMPOUND_FN, class ARRAY_FN>
            void peek_blind_iterate(SIZE_FN&& size_callback, COMPOUND_FN&& compound, ARRAY_FN&& array_or_log_item)
                requires(std::is_invocable_v<ARRAY_FN, value_read_stream&> && (std::is_invocable_v<COMPOUND_FN, std::string_view, value_read_stream&> || std::is_invocable_v<COMPOUND_FN, const std::string&, value_read_stream&>) && std::is_invocable_v<SIZE_FN, uint64_t>)
            {
                auto old_pos = read_stream.tellg();
                try {
                    blind_iterate(size_callback, std::move(compound), std::move(array_or_log_item));
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }
        };

        //lightweight writer class for writing to stream without allocations
        // this class requires ostream to support peek and seek operations for writing darray and compound
        class value_write_stream {
            std::ostream& write_stream;
            bool need_to_write_type_id;
            enbt::type_id written_type_id;

        public:
            enbt::type_id get_written_type_id() const {
                return written_type_id;
            }

            class darray {
                std::ostream& write_stream;
                std::ostream::pos_type type_size_field_pos;
                std::size_t items = 0;
                bool type_id_written;

            public:
                darray(std::ostream& write_stream, bool write_type_id);
                ~darray();
                darray& write(const enbt::value&);

                inline darray& write(bool res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(uint8_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(uint16_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(uint32_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(uint64_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(int8_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(int16_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(int32_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(int64_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(float res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(double res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(enbt::raw_uuid res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline darray& write(const std::string& res) {
                    return write([&res](auto& s) { s.write(res); });
                }

                inline darray& write(std::string_view res) {
                    return write([res](auto& s) { s.write(res); });
                }

                template <class FN>
                darray& write(FN&& fn)
                    requires(std::is_invocable_v<FN, value_write_stream&>)
                {
                    value_write_stream inner(write_stream);
                    fn(inner);
                    items++;
                    return *this;
                }

                //fn(item, inner)
                template <class Iterable, class FN>
                darray& iterable(const Iterable& iter, FN&& fn)
                    requires(std::is_invocable_v<FN, value_write_stream&>)
                {
                    for (const auto& item : iter)
                        write([&](value_write_stream& inner) {
                            fn(item, inner);
                        });
                    return *this;
                }

                template <class Iterable, class FN>
                darray& iterable(const Iterable& iter) {
                    for (const auto& item : iter)
                        write([&](value_write_stream& inner) {
                            inner.write(item);
                        });
                    return *this;
                }
            };

            class array {
                std::ostream& write_stream;
                std::size_t items_to_write = 0;
                ::enbt::type_id current_type_id;
                std::int8_t bit_i = 0;
                std::uint8_t bit_value = 0;
                bool type_set = false;

            public:
                array(std::ostream& write_stream, size_t size, bool write_type_id);
                ~array();
                array& write(const enbt::value&);

                array& write(bool res) {
                    if (items_to_write == 0)
                        throw std::invalid_argument("array is full");
                    if (!type_set) {
                        current_type_id = enbt::type_id{enbt::type::bit};
                        write_type_id(write_stream, current_type_id);
                        type_set = true;
                    }
                    if (current_type_id != enbt::type_id{enbt::type::bit})
                        throw enbt::exception("array type mismatch");
                    if (bit_i >= 8) {
                        bit_i = 0;
                        write_stream.write((char*)&bit_value, sizeof(bit_value));
                    }
                    bit_value = (((uint8_t)res) & (1 << bit_i));
                    items_to_write--;
                    return *this;
                }

                inline array& write(uint8_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(uint16_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(uint32_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(uint64_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(int8_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(int16_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(int32_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(int64_t res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(float res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(double res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(enbt::raw_uuid res) {
                    return write([res](auto& s) { s.write(res); });
                }

                inline array& write(const std::string& res) {
                    return write([&res](auto& s) { s.write(res); });
                }

                inline array& write(std::string_view res) {
                    return write([res](auto& s) { s.write(res); });
                }

                template <class FN>
                array& write(FN&& fn)
                    requires(std::is_invocable_v<FN, value_write_stream&>)
                {
                    if (items_to_write == 0)
                        throw std::invalid_argument("array is full");
                    value_write_stream inner(write_stream, !type_set);
                    fn(inner);
                    if (!type_set) {
                        current_type_id = inner.get_written_type_id();
                        if (current_type_id.type == type::bit)
                            throw enbt::exception("Bit type for in type::array is compressed, using functional iterable is not allowed.");
                        type_set = true;
                    } else if (inner.get_written_type_id() != current_type_id)
                        throw enbt::exception("array type mismatch");
                    items_to_write--;
                    return *this;
                }

                //fn(item, inner)
                template <class Iterable, class FN>
                array& iterable(const Iterable& iter, FN&& fn) {
                    for (const auto& item : iter)
                        write([&](value_write_stream& inner) {
                            fn(item, inner);
                        });
                    return *this;
                }

                template <class Iterable>
                array& iterable(const Iterable& iter) {
                    for (const auto& item : iter)
                        write(item);
                    return *this;
                }
            };

            template <class T>
            class sarray {
                std::ostream& write_stream;
                std::size_t items_to_write;

            public:
                sarray(std::ostream& write_stream, size_t size, bool write_type_id_)
                    : write_stream(write_stream), items_to_write(size) {
                    enbt::type_id tid = simple_array<T>::enbt_type;
                    if (write_type_id_)
                        write_type_id(write_stream, tid);
                    write_compress_len(write_stream, size);
                }

                sarray& write(const T& value) {
                    if (items_to_write == 0)
                        throw std::invalid_argument("array is full");
                    T val = value;
                    write_stream.write((char*)&val, sizeof(T));
                    items_to_write--;
                    return *this;
                };

                template <class Iterable, class FN>
                sarray& iterable(const Iterable& iter, FN&& cast_fn) {
                    for (const auto& item : iter)
                        write(cast_fn(item));
                    return *this;
                }

                template <class Iterable, class FN>
                sarray& iterable(const Iterable& iter) {
                    for (const auto& item : iter)
                        write(item);
                    return *this;
                }
            };

            class compound {
                std::ostream& write_stream;
                std::ostream::pos_type type_size_field_pos;
                std::size_t items = 0;
                bool type_id_written;

            public:
                compound(std::ostream& write_stream, bool write_type_id);
                ~compound();

                compound& write(std::string_view filed_name, const enbt::value&);

                inline compound& write(std::string_view filed_name, bool res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, uint8_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, uint16_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, uint32_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, uint64_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, int8_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, int16_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, int32_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, int64_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, float res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, double res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, enbt::raw_uuid res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, const std::string& res) {
                    return write(filed_name, [&res](auto& s) { s.write(res); });
                }

                inline compound& write(std::string_view filed_name, std::string_view res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                template <class FN>
                compound& write(std::string_view filed_name, FN&& fn)
                    requires(std::is_invocable_v<FN, value_write_stream&>)
                {
                    write_string(write_stream, filed_name);
                    value_write_stream inner(write_stream);
                    fn(inner);
                    items++;
                    return *this;
                }

                //fn(name, item, inner)
                template <class Iterable, class FN>
                compound& iterable(const Iterable& iter, FN&& fn) {
                    for (const auto& [name, item] : iter)
                        write(name, [&](value_write_stream& inner) {
                            fn(name, item, inner);
                        });
                    return *this;
                }

                template <class Iterable, class FN>
                compound& iterable(const Iterable& iter) {
                    for (const auto& [name, item] : iter)
                        write(name, [&](value_write_stream& inner) {
                            inner.write(item);
                        });
                    return *this;
                }
            };

            class compound_fixed {
                std::ostream& write_stream;
                std::size_t items_to_write;

            public:
                compound_fixed(std::ostream& write_stream, size_t size, bool write_type_id);
                ~compound_fixed();

                compound_fixed& write(std::string_view filed_name, const enbt::value&);

                inline compound_fixed& write(std::string_view filed_name, bool res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, uint8_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, uint16_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, uint32_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, uint64_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, int8_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, int16_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, int32_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, int64_t res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, float res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, double res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, enbt::raw_uuid res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, const std::string& res) {
                    return write(filed_name, [&res](auto& s) { s.write(res); });
                }

                inline compound_fixed& write(std::string_view filed_name, std::string_view res) {
                    return write(filed_name, [res](auto& s) { s.write(res); });
                }

                template <class FN>
                compound_fixed& write(std::string_view filed_name, FN&& fn)
                    requires(std::is_invocable_v<FN, value_write_stream&>)
                {
                    if (items_to_write == 0)
                        throw std::invalid_argument("compound is full");
                    --items_to_write;
                    write_string(write_stream, filed_name);
                    value_write_stream inner(write_stream);
                    fn(inner);
                    return *this;
                }

                //fn(name, item, inner)
                template <class Iterable, class FN>
                compound_fixed& iterable(const Iterable& iter, FN&& fn) {
                    for (const auto& [name, item] : iter)
                        write(name, [&](value_write_stream& inner) {
                            fn(name, item, inner);
                        });
                    return *this;
                }

                template <class Iterable, class FN>
                compound_fixed& iterable(const Iterable& iter) {
                    for (const auto& [name, item] : iter)
                        write(name, [&](value_write_stream& inner) {
                            inner.write(item);
                        });
                    return *this;
                }
            };

            class optional {
                std::ostream& write_stream;
                bool is_written = false;
                bool has_value = false;

            public:
                optional(std::ostream& write_stream, bool need_to_write_type_id);
                ~optional();
                void write(const enbt::value&);

                inline void write(bool res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(uint8_t res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(uint16_t res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(uint32_t res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(uint64_t res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(int8_t res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(int16_t res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(int32_t res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(int64_t res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(float res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(double res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(enbt::raw_uuid res) {
                    write([res](auto& s) { s.write(res); });
                }

                inline void write(const std::string& res) {
                    write([&res](auto& s) { s.write(res); });
                }

                inline void write(std::string_view res) {
                    write([res](auto& s) { s.write(res); });
                }

                template <class FN>
                void write(FN&& fn)
                    requires(std::is_invocable_v<FN, value_write_stream&>)
                {
                    if (has_value)
                        throw std::runtime_error("Tried to write optional multiple times.");
                    if (!is_written) {
                        write_type_id(write_stream, enbt::type_id(enbt::type::optional, true));
                        is_written = true;
                    }
                    has_value = true;
                    value_write_stream inner(write_stream);
                    fn(inner);
                }
            };

            void write(const enbt::value&);
            void write(bool res);
            void write(uint8_t res);
            void write(uint16_t res);
            void write(uint32_t res);
            void write(uint64_t res);
            void write(int8_t res);
            void write(int16_t res);
            void write(int32_t res);
            void write(int64_t res);
            void write(float res);
            void write(double res);
            void write(enbt::raw_uuid res);
            void write(const std::string& res);
            void write(std::string_view res);
            compound write_compound();
            compound_fixed write_compound(size_t size);
            darray write_darray();
            array write_array(size_t size);
            optional write_optional();
            void write_log_item(const enbt::value&);

            template <class FN>
            void write_log_item(FN&& fn)
                requires(std::is_invocable_v<FN, value_write_stream&>)
            {
                written_type_id = enbt::type_id(enbt::type::log_item);
                if (need_to_write_type_id)
                    write_type_id(write_stream, written_type_id);
                std::ostringstream temp_stream;
                value_write_stream inner(temp_stream);
                fn(inner);
                write_compress_len(write_stream, temp_stream.tellp());
                write_stream << temp_stream.rdbuf();
            }

            template <class T, std::size_t N>
            void write_sarray_dir(const T (&array)[N]) {
                enbt::type_id tid = simple_array<T>::enbt_type;
                if (need_to_write_type_id)
                    write_type_id(write_stream, tid);
                write_compress_len(write_stream, N);
                write_stream.write((const char*)array, N * sizeof(T));
                written_type_id = simple_array<T>::enbt_type;
            }

            template <class T>
            void write_sarray_dir(const T* array, std::size_t size) {
                enbt::type_id tid = simple_array<T>::enbt_type;
                if (need_to_write_type_id)
                    write_type_id(write_stream, tid);
                write_compress_len(write_stream, size);
                write_stream.write((const char*)array, size * sizeof(T));
                written_type_id = simple_array<T>::enbt_type;
            }

            template <class T>
            sarray<T> write_sarray(size_t size) {
                written_type_id = simple_array<T>::enbt_type;
                return sarray<T>(write_stream, size, need_to_write_type_id);
            }

            value_write_stream(std::ostream& write_stream, bool need_to_write_type_id = true);
        };

        namespace collection {
            template <template <class...> class map_base = std::unordered_map>
            class compound_relaxed {
                map_base<std::string, std::function<void(value_read_stream&)>> automated_collector;

            public:
                compound_relaxed() = default;
                ~compound_relaxed() = default;

                template <class FN>
                compound_relaxed& collect(const std::string& name, FN&& fn)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    automated_collector[name] = std::forward<FN>(fn);
                    return *this;
                }

                compound_relaxed& collect_into(const std::string& name, bool& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, uint8_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, uint16_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, uint32_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, uint64_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, int8_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, int16_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, int32_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, int64_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, float& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, double& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, enbt::raw_uuid& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_into(const std::string& name, std::string& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, bool& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, uint8_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, uint16_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, uint32_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, uint64_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, int8_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, int16_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, int32_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, int64_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, float& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, double& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, enbt::raw_uuid& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_relaxed& collect_as(const std::string& name, std::string& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                template <class FN>
                compound_relaxed& collect_iterate(const std::string& name, FN&& fn)
                    requires(
                        std::is_invocable_v<FN, value_read_stream&>
                        || std::is_invocable_v<FN, std::string_view, value_read_stream&>
                        || std::is_invocable_v<FN, const std::string&, value_read_stream&>
                    )
                {
                    automated_collector[name] = [fn](value_read_stream& stream) {
                        stream.iterate(fn);
                    };
                    return *this;
                }

                template <class SIZE_FN, class FN>
                compound_relaxed& collect_iterate(const std::string& name, SIZE_FN&& fn_size, FN&& fn)
                    requires(
                        (
                            std::is_invocable_v<FN, value_read_stream&>
                            || std::is_invocable_v<FN, std::string_view, value_read_stream&>
                            || std::is_invocable_v<FN, const std::string&, value_read_stream&>
                        )
                        && std::is_invocable_v<SIZE_FN, uint64_t>
                    )
                {
                    automated_collector[name] = [fn, fn_size](value_read_stream& stream) {
                        stream.iterate(fn_size, fn);
                    };
                    return *this;
                }

                template <class FN>
                compound_relaxed& make_collect(value_read_stream& stream, FN&& on_uncollected)
                    requires(std::is_invocable_v<FN, const std::string&, value_read_stream&>)
                {
                    stream.iterate([this, &on_uncollected](const std::string& name, value_read_stream& stream) {
                        if (auto it = automated_collector.find(name); it != automated_collector.end())
                            it->second(stream);
                        else
                            on_uncollected(name, stream);
                    });
                    return *this;
                }

                compound_relaxed& make_collect(value_read_stream& stream) {
                    return make_collect(stream, [](auto&, auto&) {});
                }

                compound_relaxed& force_all_collect(value_read_stream& stream) {
                    std::unordered_set<std::string> collected_items;
                    collected_items.reserve(automated_collector.size());
                    stream.iterate([this, &collected_items](auto& name, auto& stream) {
                        if (auto it = automated_collector.find(name); it != automated_collector.end()) {
                            it->second(stream);
                            collected_items.emplace(name);
                        } else
                            throw enbt::exception("Not all elements is collected, skipped item: " + name);
                    });
                    for (auto& [it, _] : automated_collector)
                        if (!collected_items.contains(it))
                            throw enbt::exception("Not all elements is collected, invalid format");
                    return *this;
                }
            };

            template <template <class...> class map_base = std::unordered_map, template <class...> class arr_base = std::vector>
            class compound_strict {
                map_base<std::string, std::function<void(value_read_stream&)>> automated_collector;
                arr_base<std::string> collector_strict_order_data;

            public:
                compound_strict() = default;
                ~compound_strict() = default;

                template <class FN>
                compound_strict& collect(const std::string& name, FN&& fn)
                    requires(std::is_invocable_v<FN, value_read_stream&>)
                {
                    automated_collector[name] = std::forward<FN>(fn);
                    collector_strict_order_data.push_back(name);
                    return *this;
                }

                compound_strict& collect_into(const std::string& name, bool& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, uint8_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, uint16_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, uint32_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, uint64_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, int8_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, int16_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, int32_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, int64_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, float& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, double& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, enbt::raw_uuid& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_into(const std::string& name, std::string& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, bool& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, uint8_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, uint16_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, uint32_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, uint64_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, int8_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, int16_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, int32_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, int64_t& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, float& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, double& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, enbt::raw_uuid& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                compound_strict& collect_as(const std::string& name, std::string& res) {
                    return collect(name, [&res](auto& stream) { stream.read_into(res); });
                }

                template <class FN>
                compound_strict& collect_iterate(const std::string& name, FN&& fn)
                    requires(
                        std::is_invocable_v<FN, value_read_stream&>
                        || std::is_invocable_v<FN, std::string_view, value_read_stream&>
                        || std::is_invocable_v<FN, const std::string&, value_read_stream&>
                    )
                {
                    automated_collector[name] = [fn](value_read_stream& stream) {
                        stream.iterate(fn);
                    };
                    collector_strict_order_data.push_back(name);
                    return *this;
                }

                template <class SIZE_FN, class FN>
                compound_strict& collect_iterate(const std::string& name, SIZE_FN&& fn_size, FN&& fn)
                    requires(
                        (
                            std::is_invocable_v<FN, value_read_stream&>
                            || std::is_invocable_v<FN, std::string_view, value_read_stream&>
                            || std::is_invocable_v<FN, const std::string&, value_read_stream&>
                        )
                        && std::is_invocable_v<SIZE_FN, uint64_t>
                    )
                {
                    automated_collector[name] = [fn, fn_size](value_read_stream& stream) {
                        stream.iterate(fn_size, fn);
                    };
                    collector_strict_order_data.push_back(name);
                    return *this;
                }

                compound_strict& make_collect(value_read_stream& stream) {
                    stream.iterate([this, order = size_t(0)](const std::string& name, value_read_stream& stream) mutable {
                        if (order == collector_strict_order_data.size())
                            throw enbt::exception("Too much elements");

                        if (auto& excepted = collector_strict_order_data.at(order++); excepted != name)
                            throw enbt::exception("Invalid order, excepted: " + excepted + ", but got: " + name);
                        automated_collector.at(name)(stream);
                    });
                    return *this;
                }

                compound_strict& force_all_collect(value_read_stream& stream) {
                    std::unordered_set<std::string> collected_items;
                    collected_items.reserve(automated_collector.size());
                    stream.iterate([this, &collected_items, order = size_t(0)](auto& name, auto& stream) mutable {
                        if (order == collector_strict_order_data.size())
                            throw enbt::exception("Too much elements");
                        if (auto& excepted = collector_strict_order_data.at(order++); excepted != name)
                            throw enbt::exception("Invalid order, excepted: " + excepted + ", but got: " + name);
                        automated_collector.at(name)(stream);
                        collected_items.emplace(name);
                    });

                    for (auto& [it, _] : automated_collector)
                        if (!collected_items.contains(it))
                            throw enbt::exception("Not all elements is collected, invalid format");
                    return *this;
                }
            };
        };
    }
}

#endif /* ENBT_IO */
