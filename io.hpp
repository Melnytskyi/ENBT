#ifndef ENBT_IO
#define ENBT_IO
#include "enbt.hpp"
#include <functional>
#include <istream>

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

        class value_read_stream {
            std::istream& read_stream;

            enbt::type_id current_type_id;

            value_read_stream(std::istream& read_stream, enbt::type_id current_type_id)
                : read_stream(read_stream), current_type_id(current_type_id) {}

        public:
            value_read_stream(std::istream& read_stream);

            enbt::value read(); //default read

            enbt::type_id get_type_id() const {
                return current_type_id;
            }

            //less memory consumption
            void iterate(std::function<void(std::string_view name, value_read_stream& self)> callback); //compound
            void iterate(std::function<void(value_read_stream& self)> callback);                        //array

            void iterate(std::function<void(std::uint64_t)> size_callback, std::function<void(std::string_view name, value_read_stream& self)> callback); //compound
            void iterate(std::function<void(std::uint64_t)> size_callback, std::function<void(value_read_stream& self)> callback);                        //array
            void join_log_item(std::function<void(value_read_stream& self)> callback);

            void blind_iterate(
                std::function<void(std::string_view name, value_read_stream& self)> compound,
                std::function<void(value_read_stream& self)> array_or_log_item
            );

            void blind_iterate(
                std::function<void(std::uint64_t)> size_callback,
                std::function<void(std::string_view name, value_read_stream& self)> compound,
                std::function<void(value_read_stream& self)> array_or_log_item
            );
        };

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
                darray& write(const std::function<void(value_write_stream& inner)>& fn);

                //fn(item, inner)
                template <class Iterable, class FN>
                darray& iterable(const Iterable& iter, const FN& fn) {
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

                void write(const std::function<void(value_write_stream& inner)>& fn);

            public:
                array(std::ostream& write_stream, size_t size, bool write_type_id);
                ~array();
                array& write(const enbt::value&);

                //fn(item, inner)
                template <class Iterable, class FN>
                array& iterable(const Iterable& iter, const FN& fn) {
                    for (const auto& item : iter)
                        write([&](value_write_stream& inner) {
                            fn(item, inner);
                        });
                    return *this;
                }

                template <class Iterable, class FN>
                array& iterable(const Iterable& iter) {
                    for (const auto& item : iter)
                        write([&](value_write_stream& inner) {
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
                compound& write(std::string_view filed_name, const std::function<void(value_write_stream& inner)>& fn);

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
