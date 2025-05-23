#ifndef ENBT_IO
#define ENBT_IO
#include "enbt.hpp"
#include <istream>
#include <type_traits>

namespace enbt {
    namespace io_helper {
        void write_compress_len(std::ostream& write_stream, std::uint64_t len);
        void write_type_id(std::ostream& write_stream, enbt::type_id tid);
        void write_string(std::ostream& write_stream, const value& val);
        void write_define_len(std::ostream& write_stream, std::uint64_t len, enbt::type_id tid);
        void initialize_version(std::ostream& write_stream);
        void write_compound(std::ostream& write_stream, const value& val);
        void write_array(std::ostream& write_stream, const value& val);
        void write_darray(std::ostream& write_stream, const value& val);
        void write_simple_array(std::ostream& write_stream, const value& val);
        void write_log_item(std::ostream& write_stream, const value& val);
        void write_value(std::ostream& write_stream, const value& val);
        void write_token(std::ostream& write_stream, const value& val);


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
        bool find_value_compound(std::istream& read_stream, enbt::type_id tid, const std::string& key);
        void index_static_array(std::istream& read_stream, std::uint64_t index, std::uint64_t len, enbt::type_id target_id);
        void index_dyn_array(std::istream& read_stream, std::uint64_t index, std::uint64_t len);
        void index_array(std::istream& read_stream, std::uint64_t index, enbt::type_id arr_tid);
        void index_array(std::istream& read_stream, std::uint64_t index);

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
            value_path(value_path&& move);

            value_path&& operator[](std::string_view index);
            value_path&& operator[](std::uint64_t index);
        };

        //move read_stream cursor to value,
        //value_path similar: "0/the test/4/54",
        //return success status
        //can throw enbt::exception
        bool move_to_value_path(std::istream& read_stream, const value_path& value_path);
        value get_value_path(std::istream& read_stream, const value_path& value_path);

        //lightweight reader class for reading from stream without allocations,
        // all functions except peek_* is final and do not preserve position of read_stream, and must be used once for lifetime of value_read_stream
        // peek_* functions require istream to be seekable
        class value_read_stream {
            std::istream& read_stream;

            enbt::type_id current_type_id;
            bool readed = false;

            value_read_stream(std::istream& read_stream, enbt::type_id current_type_id)
                : read_stream(read_stream), current_type_id(current_type_id) {}

        public:
            value_read_stream(std::istream& read_stream);
            ~value_read_stream();

            enbt::value read(); //default read
            void skip();

            enbt::type_id get_type_id() const {
                return current_type_id;
            }

            template <class FN>
            value_read_stream& peek_at(std::uint64_t index, FN&& callback) {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                auto old_state = read_stream.rdstate();
                auto old_pos = read_stream.tellg();
                try {
                    index_array(read_stream, index, current_type_id);
                    value_read_stream stream(read_stream);
                    callback(stream);
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
                return *this;
            }

            template <class FN>
            value_read_stream& peek_at(const std::string& chr, FN&& callback) {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                auto old_state = read_stream.rdstate();
                auto old_pos = read_stream.tellg();
                try {
                    if (find_value_compound(read_stream, current_type_id, chr)) {
                        value_read_stream stream(read_stream);
                        callback(stream);
                        read_stream.setstate(old_state);
                        read_stream.seekg(old_pos);
                        readed = false;
                    } else
                        throw enbt::exception("Key not found");
                } catch (...) {
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
                return *this;
            }

            size_t peek_size();

            template <class FN>
            void iterate(FN&& callback)
                requires std::is_invocable_v<FN, std::string_view, value_read_stream&> || std::is_invocable_v<FN, const std::string&, value_read_stream&>
            {
                iterate([](std::uint64_t) {}, std::move(callback));
            }

            template <class FN>
            void iterate(FN&& callback)
                requires std::is_invocable_v<FN, value_read_stream&>
            {
                iterate([](std::uint64_t) {}, std::move(callback));
            }

            template <class FN>
            void peek_iterate(FN&& callback)
                requires std::is_invocable_v<FN, std::string_view, value_read_stream&> || std::is_invocable_v<FN, const std::string&, value_read_stream&>
            {
                peek_iterate([](std::uint64_t) {}, std::move(callback));
            }

            template <class FN>
            void peek_iterate(FN&& callback)
                requires std::is_invocable_v<FN, value_read_stream&>
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
                auto old_state = read_stream.rdstate();
                auto old_pos = read_stream.tellg();
                try {
                    iterate_into(arr, size);
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
                readed = false;
            }

            template <class T>
            std::vector<T> iterate_into() {
                if (readed)
                    throw enbt::exception("Invalid read state, item has been already readed");
                if (current_type_id.type == enbt::type::sarray) {
                    if (simple_array<T>::enbt_type.length != current_type_id.length)
                        throw enbt::exception("Type mismatch");
                    std::uint64_t len = read_compress_len(read_stream);
                    std::vector<T> res;
                    res.resize(len);
                    read_stream.read((char*)res.data(), len * sizeof(T));
                    readed = true;
                    enbt::endian_helpers::convert_endian_arr(current_type_id.get_endian(), res);
                    return res;
                } else {
                    size_t index = 0;
                    std::vector<T> res;
                    iterate(
                        [&](size_t len) {
                            res.reserve(len);
                        },
                        [&](value_read_stream& self) {
                            res.push_back(self.read());
                        }
                    );
                    return res;
                }
            }

            template <class T>
            std::vector<T> peek_iterate_into() {
                auto old_state = read_stream.rdstate();
                auto old_pos = read_stream.tellg();
                try {
                    auto res = iterate_into<T>();
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                    return res;
                } catch (...) {
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }

            template <class SIZE_FN, class FN>
            void iterate(SIZE_FN&& size_callback, FN&& callback)
                requires(std::is_invocable_v<FN, std::string_view, value_read_stream&> || std::is_invocable_v<FN, const std::string&, value_read_stream&>) && std::is_invocable_v<SIZE_FN, uint64_t>
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
                requires(std::is_invocable_v<FN, std::string_view, value_read_stream&> || std::is_invocable_v<FN, const std::string&, value_read_stream&>) && std::is_invocable_v<SIZE_FN, uint64_t>
            {
                auto old_state = read_stream.rdstate();
                auto old_pos = read_stream.tellg();
                try {
                    iterate(size_callback, callback);
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }

            template <class SIZE_FN, class FN>
            void iterate(SIZE_FN&& size_callback, FN&& callback)
                requires std::is_invocable_v<FN, value_read_stream&> && std::is_invocable_v<SIZE_FN, uint64_t>
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
                requires std::is_invocable_v<FN, value_read_stream&> && std::is_invocable_v<SIZE_FN, uint64_t>
            {
                auto old_state = read_stream.rdstate();
                auto old_pos = read_stream.tellg();
                try {
                    iterate(size_callback, callback);
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }

            template <class FN>
            void join_log_item(FN&& callback)
                requires std::is_invocable_v<FN, value_read_stream&>
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
                requires std::is_invocable_v<FN, value_read_stream&>
            {
                auto old_state = read_stream.rdstate();
                auto old_pos = read_stream.tellg();
                try {
                    join_log_item(callback);
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.setstate(old_state);
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
                requires std::is_invocable_v<ARRAY_FN, value_read_stream&> && (std::is_invocable_v<COMPOUND_FN, std::string_view, value_read_stream&> || std::is_invocable_v<COMPOUND_FN, const std::string&, value_read_stream&>)
            {
                blind_iterate([](std::uint64_t) {}, std::move(compound), std::move(array_or_log_item));
            }

            template <class SIZE_FN, class COMPOUND_FN, class ARRAY_FN>
            void blind_iterate(
                SIZE_FN&& size_callback, COMPOUND_FN&& compound, ARRAY_FN&& array_or_log_item
            )
                requires std::is_invocable_v<ARRAY_FN, value_read_stream&> && (std::is_invocable_v<COMPOUND_FN, std::string_view, value_read_stream&> || std::is_invocable_v<COMPOUND_FN, const std::string&, value_read_stream&>) && std::is_invocable_v<SIZE_FN, uint64_t>
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
                requires std::is_invocable_v<ARRAY_FN, value_read_stream&> && (std::is_invocable_v<COMPOUND_FN, std::string_view, value_read_stream&> || std::is_invocable_v<COMPOUND_FN, const std::string&, value_read_stream&>)
            {
                auto old_state = read_stream.rdstate();
                auto old_pos = read_stream.tellg();
                try {
                    blind_iterate([](std::uint64_t) {}, std::move(compound), std::move(array_or_log_item));
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                    throw;
                }
            }

            template <class SIZE_FN, class COMPOUND_FN, class ARRAY_FN>
            void peek_blind_iterate(SIZE_FN&& size_callback, COMPOUND_FN&& compound, ARRAY_FN&& array_or_log_item)
                requires std::is_invocable_v<ARRAY_FN, value_read_stream&> && (std::is_invocable_v<COMPOUND_FN, std::string_view, value_read_stream&> || std::is_invocable_v<COMPOUND_FN, const std::string&, value_read_stream&>) && std::is_invocable_v<SIZE_FN, uint64_t>
            {
                auto old_state = read_stream.rdstate();
                auto old_pos = read_stream.tellg();
                try {
                    blind_iterate(size_callback, std::move(compound), std::move(array_or_log_item));
                    read_stream.setstate(old_state);
                    read_stream.seekg(old_pos);
                    readed = false;
                } catch (...) {
                    read_stream.setstate(old_state);
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

                template <class FN>
                darray& write(FN&& fn)
                    requires std::is_invocable_v<FN, value_write_stream&>
                {
                    value_write_stream inner(write_stream);
                    fn(inner);
                    items++;
                    return *this;
                }

                //fn(item, inner)
                template <class Iterable, class FN>
                darray& iterable(const Iterable& iter, const FN& fn)
                    requires std::is_invocable_v<FN, value_write_stream&>
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
                enbt::type_id current_type_id;
                bool type_set = false;

                template <class FN>
                void write_fn(FN&& fn) {
                    if (items_to_write == 0)
                        throw std::invalid_argument("array is full");
                    auto pos = write_stream.tellp();
                    value_write_stream inner(write_stream, !type_set);
                    fn(inner);
                    if (!type_set) {
                        current_type_id = inner.get_written_type_id();
                        type_set = true;
                    } else if (inner.get_written_type_id() != current_type_id) {
                        write_stream.seekp(pos);
                        throw enbt::exception("array type mismatch");
                    }
                    items_to_write--;
                }

            public:
                array(std::ostream& write_stream, size_t size, bool write_type_id);
                ~array();
                array& write(const enbt::value&);

                //fn(item, inner)
                template <class Iterable, class FN>
                array& iterable(const Iterable& iter, const FN& fn) {
                    for (const auto& item : iter)
                        write_fn([&](value_write_stream& inner) {
                            fn(item, inner);
                        });
                    return *this;
                }

                template <class Iterable, class FN>
                array& iterable(const Iterable& iter) {
                    for (const auto& item : iter)
                        write_fn([&](value_write_stream& inner) {
                            inner.write(item);
                        });
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
                sarray& iterable(const Iterable& iter, const FN& cast_fn) {
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

                template <class FN>
                compound& write(std::string_view filed_name, FN&& fn)
                    requires std::is_invocable_v<FN, value_write_stream&>
                {
                    write_string(write_stream, filed_name);
                    value_write_stream inner(write_stream);
                    fn(inner);
                    items++;
                    return *this;
                }

                //fn(name, item, inner)
                template <class Iterable, class FN>
                compound& iterable(const Iterable& iter, const FN& fn) {
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

            void write(const enbt::value&);
            compound write_compound();
            darray write_darray();
            array write_array(size_t size);

            template <class T, std::size_t N>
            void write_sarray_dir(const T (&array)[N]) {
                enbt::type_id tid = simple_array<T>::enbt_type;
                if (need_to_write_type_id)
                    write_type_id(write_stream, tid);
                write_compress_len(write_stream, N);
                write_stream.write((char*)array, N * sizeof(T));
                written_type_id = simple_array<T>::enbt_type;
            }

            template <class T>
            sarray<T> write_sarray(size_t size) {
                written_type_id = simple_array<T>::enbt_type;
                return sarray<T>(write_stream, size, need_to_write_type_id);
            }

            value_write_stream(std::ostream& write_stream, bool need_to_write_type_id = true);
        };
    }
}

#endif /* ENBT_IO */
