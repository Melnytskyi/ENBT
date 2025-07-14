#include "enbt.hpp"
#include "io.hpp"
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>
#pragma region value constructors

namespace enbt {
    namespace endian_helpers {
        void endian_swap(void* value_ptr, std::size_t len) {
            std::byte* prox = static_cast<std::byte*>(value_ptr);
            std::reverse(prox, prox + len);
        }
    }

    raw_uuid raw_uuid::generate_v4() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        raw_uuid uuid;
        for (std::size_t i = 0; i < 16; i++)
            uuid.data[i] = dis(gen);
        uuid.data[6] = (uuid.data[6] & 0x0F) | 0x40;
        uuid.data[8] = (uuid.data[8] & 0x3F) | 0x80;
        return uuid;
    }

    raw_uuid raw_uuid::generate_v7() {
        auto current_time = std::chrono::system_clock::now().time_since_epoch().count() / 1000;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        raw_uuid uuid;
        for (std::size_t i = 4; i < 16; i++)
            uuid.data[i] = dis(gen);
        uuid.data[6] = (uuid.data[6] & 0x0F) | 0x70;
        uuid.data[8] = (uuid.data[8] & 0x3F) | 0x80;
        uuid.data[3] = (current_time >> 24) & 0xFF;
        uuid.data[2] = (current_time >> 16) & 0xFF;
        uuid.data[1] = (current_time >> 8) & 0xFF;
        uuid.data[0] = current_time & 0xFF;
        return uuid;
    }

    raw_uuid raw_uuid::as_null() {
        return raw_uuid{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    }

    enbt::type_len calc_type_len(std::size_t len) {
        if (len > UINT32_MAX)
            return enbt::type_len::Long;
        else if (len > UINT16_MAX)
            return enbt::type_len::Default;
        else if (len > UINT8_MAX)
            return enbt::type_len::Short;
        else
            return enbt::type_len::Tiny;
    }

    value::value() {
        data = nullptr;
        data_len = 0;
        data_type_id = enbt::type_id{type::none, type_len::Tiny};
    }

    value::value(std::string_view str) {
        data_type_id.type = type::string;
        data = (std::uint8_t*)new std::string(str);
    }

    value::value(const std::string& str) {
        data_type_id.type = type::string;
        data = (std::uint8_t*)new std::string(str);
    }

    value::value(std::string&& str) {
        data_type_id.type = type::string;
        data = (std::uint8_t*)new std::string(std::move(str));
    }

    value::value(const char* str) {
        data_type_id.type = type::string;
        data = (std::uint8_t*)new std::string(str);
    }

    value::value(const char* str, size_t len) {
        data_type_id.type = type::string;
        data = (std::uint8_t*)new std::string(str, len);
    }

    value::value(std::initializer_list<value> arr)
        : value(std::vector<value>(arr)) {
    }

    value::value(const std::vector<value>& array, enbt::type_id tid) {
        if (tid.type == type::darray)
            ;
        else if (tid.type == type::array) {
            if (array.size()) {
                enbt::type_id tid_check = array[0].type_id();
                for (auto& check : array)
                    if (!check.type_equal(tid_check))
                        throw exception("Invalid tid");
            }
        } else
            throw exception("Invalid tid");
        data_type_id = tid;
        data_type_id.length = calc_type_len(array.size());
        data = (std::uint8_t*)new std::vector<value>(array);
    }

    value::value(const std::unordered_map<std::string, value>& compound) {
        data_type_id = enbt::type_id{type::compound, calc_type_len(compound.size()), false};
        data = (std::uint8_t*)new std::unordered_map<std::string, value>(compound);
    }

    value::value(std::vector<value>&& array) {
        data_len = array.size();
        data_type_id.type = type::array;
        data_type_id.is_signed = 0;
        data_type_id.endian = endian::native;
        if (data_len <= UINT8_MAX)
            data_type_id.length = type_len::Tiny;
        else if (data_len <= UINT16_MAX)
            data_type_id.length = type_len::Short;
        else if (data_len <= UINT32_MAX)
            data_type_id.length = type_len::Default;
        else
            data_type_id.length = type_len::Long;
        data = (std::uint8_t*)new std::vector<value>(std::move(array));
    }

    value::value(std::vector<value>&& array, enbt::type_id tid) {
        if (tid.type == type::darray)
            ;
        else if (tid.type == type::array) {
            if (array.size()) {
                enbt::type_id tid_check = array[0].type_id();
                for (auto& check : array)
                    if (!check.type_equal(tid_check))
                        throw exception("Invalid tid");
            }
        } else
            throw exception("Invalid tid");
        data_type_id = tid;
        data_type_id.length = calc_type_len(array.size());
        data = (std::uint8_t*)new std::vector<value>(std::move(array));
    }

    value::value(std::unordered_map<std::string, value>&& compound) {
        data_type_id = enbt::type_id{type::compound, calc_type_len(compound.size()), false};
        data = (std::uint8_t*)new std::unordered_map<std::string, value>(std::move(compound));
    }

    value::value(const std::uint8_t* arr, std::size_t len) {
        data_type_id = enbt::type_id{type::sarray, type_len::Tiny, false};
        std::uint8_t* carr = new std::uint8_t[len];
        for (std::size_t i = 0; i < len; i++)
            carr[i] = arr[i];
        data = (std::uint8_t*)carr;
        data_len = len;
    }

    value::value(const std::uint16_t* arr, std::size_t len, std::endian endian, bool convert_endian) {
        data_type_id = enbt::type_id{type::sarray, type_len::Short, endian, false};
        std::uint16_t* str = new std::uint16_t[len];
        for (std::size_t i = 0; i < len; i++)
            str[i] = arr[i];
        if (convert_endian)
            endian_helpers::convert_endian_arr(endian, str, len);


        data = (std::uint8_t*)str;
        data_len = len;
    }

    value::value(const std::uint32_t* arr, std::size_t len, std::endian endian, bool convert_endian) {
        data_type_id = enbt::type_id{type::sarray, type_len::Default, endian, false};
        std::uint32_t* str = new std::uint32_t[len];
        for (std::size_t i = 0; i < len; i++)
            str[i] = arr[i];
        if (convert_endian)
            endian_helpers::convert_endian_arr(endian, str, len);
        data = (std::uint8_t*)str;
        data_len = len;
    }

    value::value(const std::uint64_t* arr, std::size_t len, std::endian endian, bool convert_endian) {
        data_type_id = enbt::type_id{type::sarray, type_len::Long, endian, false};
        std::uint64_t* str = new std::uint64_t[len];
        for (std::size_t i = 0; i < len; i++)
            str[i] = arr[i];
        if (convert_endian)
            endian_helpers::convert_endian_arr(endian, str, len);
        data = (std::uint8_t*)str;
        data_len = len;
    }

    value::value(const std::int8_t* arr, std::size_t len) {
        data_type_id = enbt::type_id{type::sarray, type_len::Tiny, true};
        std::int8_t* str = new std::int8_t[len];
        for (std::size_t i = 0; i < len; i++)
            str[i] = arr[i];
        data = (std::uint8_t*)str;
        data_len = len;
    }

    value::value(const std::int16_t* arr, std::size_t len, std::endian endian, bool convert_endian) {
        data_type_id = enbt::type_id{type::sarray, type_len::Short, endian, true};
        std::int16_t* str = new std::int16_t[len];
        for (std::size_t i = 0; i < len; i++)
            str[i] = arr[i];
        if (convert_endian)
            endian_helpers::convert_endian_arr(endian, str, len);


        data = (std::uint8_t*)str;
        data_len = len;
    }

    value::value(const std::int32_t* arr, std::size_t len, std::endian endian, bool convert_endian) {
        data_type_id = enbt::type_id{type::sarray, type_len::Default, endian, true};
        std::int32_t* str = new std::int32_t[len];
        for (std::size_t i = 0; i < len; i++)
            str[i] = arr[i];
        if (convert_endian)
            endian_helpers::convert_endian_arr(endian, str, len);
        data = (std::uint8_t*)str;
        data_len = len;
    }

    value::value(const std::int64_t* arr, std::size_t len, std::endian endian, bool convert_endian) {
        data_type_id = enbt::type_id{type::sarray, type_len::Long, endian, true};
        std::int64_t* str = new std::int64_t[len];
        for (std::size_t i = 0; i < len; i++)
            str[i] = arr[i];
        if (convert_endian)
            endian_helpers::convert_endian_arr(endian, str, len);
        data = (std::uint8_t*)str;
        data_len = len;
    }

    value::value(raw_uuid uuid, std::endian endian, bool convert_endian) {
        data_type_id = enbt::type_id{type::uuid};
        set_data(uuid);
        if (convert_endian)
            endian_helpers::convert_endian(endian, data, data_len);
    }

    value::value(bool byte) {
        data_type_id = enbt::type_id{type::bit, type_len::Tiny, byte};
        data_len = 0;
    }

    value::value(std::int8_t byte) {
        set_data(byte);
        data_type_id = enbt::type_id{type::integer, type_len::Tiny, true};
    }

    value::value(std::uint8_t byte) {
        set_data(byte);
        data_type_id = enbt::type_id{type::integer, type_len::Tiny};
    }

    value::value(std::int16_t sh, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &sh, sizeof(std::int16_t));
        set_data(sh);
        data_type_id = enbt::type_id{type::integer, type_len::Short, endian, true};
    }

    value::value(std::int32_t in, bool as_var, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &in, sizeof(std::int32_t));
        set_data(in);
        if (as_var)
            data_type_id = enbt::type_id{type::var_integer, type_len::Default, endian, true};
        else
            data_type_id = enbt::type_id{type::integer, type_len::Default, endian, true};
    }

    value::value(std::int64_t lon, bool as_var, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &lon, sizeof(std::int64_t));
        set_data(lon);
        if (as_var)
            data_type_id = enbt::type_id{type::var_integer, type_len::Long, endian, true};
        else
            data_type_id = enbt::type_id{type::integer, type_len::Long, endian, true};
    }

    value::value(std::int32_t in, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &in, sizeof(std::int32_t));
        set_data(in);
        data_type_id = enbt::type_id{type::integer, type_len::Default, endian, true};
    }

    value::value(std::int64_t lon, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &lon, sizeof(std::int64_t));
        set_data(lon);
        data_type_id = enbt::type_id{type::integer, type_len::Long, endian, true};
    }

    value::value(std::uint16_t sh, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &sh, sizeof(std::uint16_t));
        set_data(sh);
        data_type_id = enbt::type_id{type::integer, type_len::Short, endian};
    }

    value::value(std::uint32_t in, bool as_var, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &in, sizeof(std::uint32_t));
        set_data(in);
        if (as_var)
            data_type_id = enbt::type_id{type::var_integer, type_len::Default, endian, false};
        else
            data_type_id = enbt::type_id{type::integer, type_len::Default, endian, false};
    }

    value::value(std::uint64_t lon, bool as_var, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &lon, sizeof(std::uint64_t));
        set_data(lon);
        if (as_var)
            data_type_id = enbt::type_id{type::var_integer, type_len::Long, endian, false};
        else
            data_type_id = enbt::type_id{type::integer, type_len::Long, endian, false};
    }

    value::value(std::uint32_t in, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &in, sizeof(std::uint32_t));
        set_data(in);
        data_type_id = enbt::type_id{type::integer, type_len::Default, endian, false};
    }

    value::value(std::uint64_t lon, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &lon, sizeof(std::uint64_t));
        set_data(lon);
        data_type_id = enbt::type_id{type::integer, type_len::Long, endian, false};
    }

    value::value(float flo, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &flo, sizeof(float));
        set_data(flo);
        data_type_id = enbt::type_id{type::floating, type_len::Default, endian};
    }

    value::value(double dou, std::endian endian, bool convert_endian) {
        if (convert_endian)
            endian_helpers::convert_endian(endian, &dou, sizeof(double));
        set_data(dou);
        data_type_id = enbt::type_id{type::floating, type_len::Long, endian};
    }

    value::value(value_variants val, enbt::type_id tid, std::size_t length, bool convert_endian) {
        data_type_id = tid;
        data_len = 0;
        switch (tid.type) {
        case type::integer:
            switch (data_type_id.length) {
            case type_len::Tiny:
                if (data_type_id.is_signed)
                    set_data(std::get<std::int8_t>(val));
                else
                    set_data(std::get<std::uint8_t>(val));
                break;
            case type_len::Short:
                if (data_type_id.is_signed)
                    set_data(std::get<std::int16_t>(val));
                else
                    set_data(std::get<std::uint16_t>(val));
                break;
            case type_len::Default:
                if (data_type_id.is_signed)
                    set_data(std::get<std::int32_t>(val));
                else
                    set_data(std::get<std::uint32_t>(val));
                break;
            case type_len::Long:
                if (data_type_id.is_signed)
                    set_data(std::get<std::int64_t>(val));
                else
                    set_data(std::get<std::uint64_t>(val));
            }
            break;
        case type::floating:
            switch (data_type_id.length) {
            case type_len::Default:
                set_data(std::get<float>(val));
                break;
            case type_len::Long:
                set_data(std::get<double>(val));
            }
            break;
        case type::var_integer:
            switch (data_type_id.length) {
            case type_len::Default:
                if (data_type_id.is_signed)
                    set_data(std::get<std::int32_t>(val));
                else
                    set_data(std::get<std::uint32_t>(val));
                break;
            case type_len::Long:
                if (data_type_id.is_signed)
                    set_data(std::get<std::int64_t>(val));
                else
                    set_data(std::get<std::uint64_t>(val));
            }
            break;
        case type::uuid:
            set_data(std::get<std::uint8_t*>(val), 16);
            break;
        case type::sarray: {
            //assign and check, not bug
            if (tid.is_signed = data_type_id.is_signed) {
                switch (data_type_id.length) {
                case type_len::Tiny:
                    set_data(std::get<std::int8_t*>(val), length);
                    break;
                case type_len::Short:
                    set_data(std::get<std::int16_t*>(val), length);
                    break;
                case type_len::Default:
                    set_data(std::get<std::int32_t*>(val), length);
                    break;
                case type_len::Long:
                    set_data(std::get<std::int64_t*>(val), length);
                }
            } else {
                switch (data_type_id.length) {
                case type_len::Tiny:
                    set_data(std::get<std::uint8_t*>(val), length);
                    break;
                case type_len::Short:
                    set_data(std::get<std::uint16_t*>(val), length);
                    break;
                case type_len::Default:
                    set_data(std::get<std::uint32_t*>(val), length);
                    break;
                case type_len::Long:
                    set_data(std::get<std::uint64_t*>(val), length);
                }
            }
            break;
        }
        case type::array:
        case type::darray:
            set_data(new std::vector<value>(*std::get<std::vector<value>*>(val)));
            break;
        case type::compound:
            data = (std::uint8_t*)new std::unordered_map<std::string, value>(*std::get<std::unordered_map<std::string, value>*>(val));
            break;
        case type::bit:
            set_data(clone_data((std::uint8_t*)std::get<bool>(val), tid, length));
            data_len = length;
            break;
        case type::optional:
            data = (std::uint8_t*)(std::holds_alternative<value*>(val) ? std::get<value*>(val) : nullptr);
            data_len = 0;
            break;
        case type::string:
            data = (std::uint8_t*)new std::string(*std::get<std::string*>(val));
            break;
        case type::log_item:
            data = (std::uint8_t*)std::get<value*>(val);
            data_len = 0;
            break;
        default:
            data = nullptr;
            data_len = 0;
        }
    }

    value::value(std::nullopt_t) {
        data_type_id = enbt::type_id{type::optional};
        data_len = 0;
        data = nullptr;
    }

    value::value(enbt::type_id tid, std::size_t len) {
        switch (tid.type) {
        case type::compound:
            operator=(compound());
            break;
        case type::array:
        case type::darray:
            operator=(value(std::vector<value>(len), tid));
            break;
        case type::sarray:
            if (len) {
                switch (tid.length) {
                case type_len::Tiny:
                    data = new std::uint8_t[len](0);
                    break;
                case type_len::Short:
                    data = (std::uint8_t*)new std::uint16_t[len](0);
                    break;
                case type_len::Default:
                    data = (std::uint8_t*)new std::uint32_t[len](0);
                    break;
                case type_len::Long:
                    data = (std::uint8_t*)new std::uint64_t[len](0);
                    break;
                }
            }
            data_type_id = tid;
            data_len = len;

            break;
        case type::optional:
            data_type_id = tid;
            data_type_id.is_signed = false;
            data_len = 0;
            data = nullptr;
            break;
        default:
            operator=(value());
        }
    }

    value::value(type typ, std::size_t len) {
        switch (len) {
        case 0:
            operator=(enbt::type_id(typ, type_len::Tiny, false));
            break;
        case 1:
            operator=(enbt::type_id(typ, type_len::Short, false));
            break;
        case 2:
            operator=(enbt::type_id(typ, type_len::Default, false));
            break;
        case 3:
            operator=(enbt::type_id(typ, type_len::Long, false));
            break;
        default:
            operator=(enbt::type_id(typ, type_len::Default, false));
        }
    }

    value::value(const value& copy) {
        operator=(copy);
    }

    value::value(bool optional, value&& _value) {
        if (optional) {
            data_type_id = enbt::type_id(type::optional, type_len::Tiny, true);
            data = (std::uint8_t*)new value(std::move(_value));
        } else {
            data_type_id = enbt::type_id(type::optional, type_len::Tiny, false);
            data = nullptr;
        }
        data_len = 0;
    }

    value::value(bool optional, const value& _value) {
        if (optional) {
            data_type_id = enbt::type_id(type::optional, type_len::Tiny, true);
            data = (std::uint8_t*)new value(_value);
        } else {
            data_type_id = enbt::type_id(type::optional, type_len::Tiny, false);
            data = nullptr;
        }
        data_len = 0;
    }

    value::value(value&& copy) noexcept {
        operator=(std::move(copy));
    }

    value::value(const compound& copy)
        : value((const value&)copy) {}

    value::value(compound&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const fixed_array& copy)
        : value((const value&)copy) {}

    value::value(fixed_array&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const dynamic_array& copy)
        : value((const value&)copy) {}

    value::value(dynamic_array&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const simple_array_ui8& copy)
        : value((const value&)copy) {}

    value::value(simple_array_ui8&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const simple_array_ui16& copy)
        : value((const value&)copy) {}

    value::value(simple_array_ui16&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const simple_array_ui32& copy)
        : value((const value&)copy) {}

    value::value(simple_array_ui32&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const simple_array_ui64& copy)
        : value((const value&)copy) {}

    value::value(simple_array_ui64&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const simple_array_i8& copy)
        : value((const value&)copy) {}

    value::value(simple_array_i8&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const simple_array_i16& copy)
        : value((const value&)copy) {}

    value::value(simple_array_i16&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const simple_array_i32& copy)
        : value((const value&)copy) {}

    value::value(simple_array_i32&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const simple_array_i64& copy)
        : value((const value&)copy) {}

    value::value(simple_array_i64&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const bit& copy)
        : value((const value&)copy) {}

    value::value(bit&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const optional& copy)
        : value((const value&)copy) {}

    value::value(optional&& copy) noexcept
        : value((value&&)std::move(copy)) {}

    value::value(const uuid& copy)
        : value((const value&)copy) {}

    value::value(uuid&& copy) noexcept
        : value((value&&)std::move(copy)) {}

#pragma endregion

    value& value::operator[](std::size_t index) {
        if (is_array())
            return ((std::vector<value>*)data)->operator[](index);
        throw std::invalid_argument("Invalid tid, cannot index array");
    }

    value::value_variants value::get_content(std::uint8_t* data, std::size_t data_len, enbt::type_id data_type_id) {
        std::uint8_t* real_data = get_data(data, data_type_id, data_len);
        switch (data_type_id.type) {
        case type::integer:
        case type::var_integer:
            switch (data_type_id.length) {
            case type_len::Tiny:
                if (data_type_id.is_signed)
                    return (std::int8_t)real_data[0];
                else
                    return (std::uint8_t)real_data[0];
            case type_len::Short:
                if (data_type_id.is_signed)
                    return ((std::int16_t*)real_data)[0];
                else
                    return ((std::uint16_t*)real_data)[0];
            case type_len::Default:
                if (data_type_id.is_signed)
                    return ((std::int32_t*)real_data)[0];
                else
                    return ((std::uint32_t*)real_data)[0];
            case type_len::Long:
                if (data_type_id.is_signed)
                    return ((std::int64_t*)real_data)[0];
                else
                    return ((std::uint64_t*)real_data)[0];
            default:
                return nullptr;
            }
        case type::floating:
            switch (data_type_id.length) {
            case type_len::Default:
                return ((float*)real_data)[0];
            case type_len::Long:
                return ((double*)real_data)[0];
            default:
                return nullptr;
            }
        case type::uuid:
            return *(raw_uuid*)real_data;
        case type::sarray:
            if (data_type_id.is_signed) {
                switch (data_type_id.length) {
                case type_len::Tiny:
                    return (std::int8_t*)data;
                case type_len::Short:
                    return (std::int16_t*)data;
                case type_len::Default:
                    return (std::int32_t*)data;
                case type_len::Long:
                    return (std::int64_t*)data;
                default:
                    return data;
                }
            } else {
                switch (data_type_id.length) {
                case type_len::Tiny:
                    return data;
                case type_len::Short:
                    return (std::uint16_t*)data;
                case type_len::Default:
                    return (std::uint32_t*)data;
                case type_len::Long:
                    return (std::uint64_t*)data;
                default:
                    return data;
                }
            }
        case type::array:
        case type::darray:
            return ((std::vector<value>*)data);
        case type::compound:
            return ((std::unordered_map<std::string, value>*)data);
        case type::optional:
            if (data)
                return (value*)data;
            else
                return nullptr;
        case type::bit:
            return (bool)data_type_id.is_signed;
        case type::string:
            return (std::string*)data;
        case type::log_item:
            return (value*)data;
        default:
            return nullptr;
        }
    }

    uint8_t* value::clone_data(std::uint8_t* data, enbt::type_id data_type_id, std::size_t data_len) {
        switch (data_type_id.type) {
        case type::sarray: {
            switch (data_type_id.length) {
            case type_len::Tiny: {
                std::uint8_t* res = new std::uint8_t[data_len];
                for (std::size_t i = 0; i < data_len; i++)
                    res[i] = data[i];
                return res;
            }
            case type_len::Short: {
                std::uint16_t* res = new std::uint16_t[data_len];
                std::uint16_t* proxy = (std::uint16_t*)data;
                for (std::size_t i = 0; i < data_len; i++)
                    res[i] = proxy[i];
                return (std::uint8_t*)res;
            }
            case type_len::Default: {
                std::uint32_t* res = new std::uint32_t[data_len];
                std::uint32_t* proxy = (std::uint32_t*)data;
                for (std::size_t i = 0; i < data_len; i++)
                    res[i] = proxy[i];
                return (std::uint8_t*)res;
            }
            case type_len::Long: {
                std::uint64_t* res = new std::uint64_t[data_len];
                std::uint64_t* proxy = (std::uint64_t*)data;
                for (std::size_t i = 0; i < data_len; i++)
                    res[i] = proxy[i];
                return (std::uint8_t*)res;
            }
            default:
                break;
            }
        } break;
        case type::array:
        case type::darray:
            return (std::uint8_t*)new std::vector<value>(*(std::vector<value>*)data);
        case type::compound:
            if (data_type_id.is_signed)
                return (std::uint8_t*)new std::unordered_map<std::uint16_t, value>(*(std::unordered_map<std::uint16_t, value>*)data);
            else
                return (std::uint8_t*)new std::unordered_map<std::string, value>(*(std::unordered_map<std::string, value>*)data);
        case type::optional: {
            if (data) {
                value& source = *(value*)data;
                return (std::uint8_t*)new value(source);
            } else
                return nullptr;
        }
        case type::uuid:
            return (std::uint8_t*)new raw_uuid(*(raw_uuid*)data);
        case type::string:
            return (std::uint8_t*)new std::string(*(std::string*)data);
        case type::log_item:
            return (std::uint8_t*)new value(*(value*)data);
        default:
            if (data_len > 8) {
                std::uint8_t* data_cloned = new std::uint8_t[data_len];
                for (std::size_t i = 0; i < data_len; i++)
                    data_cloned[i] = data[i];
                return data_cloned;
            }
        }
        return data;
    }

    value& value::operator[](const char* index) {
        if (is_compound()) {
            return ((std::unordered_map<std::string, value>*)data)->operator[](index);
        }
        throw std::invalid_argument("Invalid tid, cannot index compound");
    }

    void value::remove(std::string name) {
        ((std::unordered_map<std::string, value>*)data)->erase(name);
    }

    const value& value::operator[](std::size_t index) const {
        if (is_array())
            return ((std::vector<value>*)data)->operator[](index);
        throw std::invalid_argument("Invalid tid, cannot index array");
    }

    const value& value::operator[](const char* index) const {
        if (is_compound()) {
            return ((std::unordered_map<std::string, value>*)data)->at(index);
        }
        throw std::invalid_argument("Invalid tid, cannot index compound");
    }

    value& value::at(const std::string& index) {
        if (is_compound()) {
            return ((std::unordered_map<std::string, value>*)data)->at(index);
        }
        throw std::invalid_argument("Invalid tid, cannot index compound");
    }

    const value& value::at(const std::string& index) const {
        if (is_compound()) {
            return ((std::unordered_map<std::string, value>*)data)->at(index);
        }
        throw std::invalid_argument("Invalid tid, cannot index compound");
    }

    value& value::at(std::size_t index) {
        if (is_array())
            return ((std::vector<value>*)data)->at(index);
        throw std::invalid_argument("Invalid tid, cannot index array");
    }

    const value& value::at(std::size_t index) const {
        if (is_array())
            return ((std::vector<value>*)data)->at(index);
        throw std::invalid_argument("Invalid tid, cannot index array");
    }

    void merge_compounds(std::unordered_map<std::string, value>& left, const std::unordered_map<std::string, value>& right) {
        for (auto& [name, val] : right) {
            auto& it = left[name];
            if (it.is_compound() && val.is_compound())
                it.merge(val);
            else
                it = val;
        }
    }

    void merge_compounds(std::unordered_map<std::string, value>& left, std::unordered_map<std::string, value>&& right) {
        for (auto& [name, val] : right) {
            auto& it = left[name];
            if (it.is_compound() && val.is_compound())
                it.merge(std::move(val));
            else
                it = std::move(val);
        }
    }

    value& value::merge(const value& copy) & {
        merge_compounds(*(std::unordered_map<std::string, value>*)data, *(std::unordered_map<std::string, value>*)copy.data);
        return *this;
    }

    value& value::merge(value&& move) & {
        merge_compounds(*(std::unordered_map<std::string, value>*)data, std::move(*(std::unordered_map<std::string, value>*)move.data));
        return *this;
    }

    value value::merge(const value& copy) && {
        merge_compounds(*(std::unordered_map<std::string, value>*)data, *(std::unordered_map<std::string, value>*)copy.data);
        return std::move(*this);
    }

    value value::merge(value&& move) && {
        merge_compounds(*(std::unordered_map<std::string, value>*)data, std::move(*(std::unordered_map<std::string, value>*)move.data));
        return std::move(*this);
    }

    value value::get_index(std::size_t index) const {
        if (is_sarray()) {
            if (data_len <= index)
                throw std::out_of_range("SArray len is: " + std::to_string(data_len) + ", but try index at: " + std::to_string(index));
            if (data_type_id.is_signed) {
                switch (data_type_id.length) {
                case type_len::Tiny:
                    return (std::int8_t)data[index];
                case type_len::Short:
                    return ((std::int16_t*)data)[index];
                case type_len::Default:
                    return ((std::int32_t*)data)[index];
                case type_len::Long:
                    return ((std::int64_t*)data)[index];
                default:
                    break;
                }
            } else {
                switch (data_type_id.length) {
                case type_len::Tiny:
                    return data[index];
                case type_len::Short:
                    return ((std::uint16_t*)data)[index];
                case type_len::Default:
                    return ((std::uint32_t*)data)[index];
                case type_len::Long:
                    return ((std::uint64_t*)data)[index];
                default:
                    break;
                }
            }
            return value();
        } else if (is_array())
            return ((std::vector<value>*)data)->operator[](index);
        else
            throw std::invalid_argument("Invalid tid, cannot index array");
    }

    void value::freeze() {
        if (data_type_id.type == type::darray) {
            bool first = true;
            enbt::type_id data_type_id;
            for (value val : *((std::vector<value>*)data)) {
                if (first) {
                    data_type_id = val.data_type_id;
                    first = false;
                } else if (val.data_type_id != data_type_id) {
                    throw std::invalid_argument("This array has different types");
                }
            }
            data_len = ((std::vector<value>*)data)->size();
            data_type_id.type = type::array;
            if (data_len <= UINT8_MAX)
                data_type_id.length = type_len::Tiny;
            else if (data_len <= UINT16_MAX)
                data_type_id.length = type_len::Short;
            else if (data_len <= UINT32_MAX)
                data_type_id.length = type_len::Default;
            else
                data_type_id.length = type_len::Long;
        } else
            throw std::invalid_argument("Cannot freeze non-dynamic array");
    }

    void value::unfreeze() {
        if (data_type_id.type == type::array) {
            data_type_id.type = type::darray;
            data_len = 0;
            data_type_id.length = type_len::Tiny;
        } else
            throw std::invalid_argument("Cannot unfreeze non-array");
    }

    template <class Target>
    Target simpleIntConvert(const value::value_variants& val) {
        return std::visit(
            [](auto& it) -> Target {
                using T = std::decay_t<decltype(it)>;
                if constexpr (std::is_same_v<T, nullptr_t>)
                    return Target(0);
                else if constexpr (std::is_same_v<T, bool>)
                    return Target(it);
                else if constexpr (std::is_same_v<T, std::uint8_t>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, std::int8_t>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, std::uint16_t>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, std::int16_t>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, std::uint32_t>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, std::int32_t>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, std::uint64_t>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, std::int64_t>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, float>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, double>)
                    return (Target)it;
                else if constexpr (std::is_same_v<T, std::string>) {
                    if constexpr (std::is_same_v<Target, bool>) {
                        if (it == "true")
                            return true;
                        else
                            return false;
                    } else if constexpr (std::is_unsigned_v<Target>)
                        return (Target)std::stoull(*it);
                    else if constexpr (std::is_floating_point_v<Target>)
                        return (Target)std::stod(*it);
                    else
                        return (Target)std::stoll(*it);
                } else
                    throw exception("Invalid type for convert");
            },
            val
        );
    }

    value& value::operator=(uint8_t set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(int8_t set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(uint16_t set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(int16_t set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(uint32_t set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(int32_t set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(uint64_t set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(int64_t set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(float set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(double set_value) {
        return operator=(value(set_value));
    }

    value& value::operator=(bool set_value) {
        if (data_type_id.type == enbt::type::bit)
            data_type_id.is_signed = set_value;
        else
            operator=(value(set_value));
        return *this;
    }

    value& value::operator=(raw_uuid set_value) {
        return operator=(value(set_value));
    }

    value::operator bool() const {
        return simpleIntConvert<bool>(content());
    }

    value::operator std::int8_t() const {
        return simpleIntConvert<std::int8_t>(content());
    }

    value::operator std::int16_t() const {
        return simpleIntConvert<std::int16_t>(content());
    }

    value::operator std::int32_t() const {
        return simpleIntConvert<std::int32_t>(content());
    }

    value::operator std::int64_t() const {
        return simpleIntConvert<std::int64_t>(content());
    }

    value::operator std::uint8_t() const {
        return simpleIntConvert<std::uint8_t>(content());
    }

    value::operator std::uint16_t() const {
        return simpleIntConvert<std::uint16_t>(content());
    }

    value::operator std::uint32_t() const {
        return simpleIntConvert<std::uint32_t>(content());
    }

    value::operator std::uint64_t() const {
        return simpleIntConvert<std::uint64_t>(content());
    }

    value::operator float() const {
        return simpleIntConvert<float>(content());
    }

    value::operator double() const {
        return simpleIntConvert<double>(content());
    }

    value::operator std::string&() {
        return *std::get<std::string*>(content());
    }

    value::operator const std::string&() const {
        return *std::get<std::string*>(content());
    }

    value::operator std::string() const {
        return std::visit(
            [](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string*>)
                    return *arg;
                else if constexpr (std::is_same_v<T, bool>)
                    return std::string(arg ? "true" : "false");
                else if constexpr (std::is_integral_v<T>)
                    return std::to_string(arg);
                else if constexpr (std::is_same_v<T, std::uint8_t*>)
                    return std::string((char*)arg);
                else
                    return std::string();
            },
            content()
        );
    }

    enbt::value value::cast_to(enbt::type_id id) const {
        static auto convert_ = [](enbt::type_id id, const enbt::value& self) -> enbt::value {
            switch (id.type) {
            case type::none:
                return enbt::value();
            case type::integer:
                switch (id.length) {
                case type_len::Tiny:
                    return id.is_signed ? value((int8_t)self) : value((uint8_t)self);
                case type_len::Short:
                    return id.is_signed ? value((int16_t)self) : value((uint16_t)self);
                case type_len::Default:
                    return id.is_signed ? value((int32_t)self) : value((uint32_t)self);
                case type_len::Long:
                    return id.is_signed ? value((int64_t)self) : value((uint64_t)self);
                default:
                    break;
                }
                break;
            case type::var_integer:
            case type::comp_integer:
                switch (id.length) {
                case type_len::Default: {
                    enbt::value res(id.is_signed ? value((int32_t)self) : value((uint32_t)self));
                    res.data_type_id.type = id.type;
                    return res;
                }
                case type_len::Long: {
                    enbt::value res(id.is_signed ? value((int64_t)self) : value((uint64_t)self));
                    res.data_type_id.type = id.type;
                    return res;
                }
                default:
                    break;
                }
                break;
            case type::floating:
                switch (id.length) {
                case type_len::Default:
                    return (float)self;
                case type_len::Long:
                    return (double)self;
                default:
                    break;
                }
                break;
            case type::string:
                return (std::string)self;
            default:
                break;
            }
            throw enbt::exception("Can not convert to this type");
        };
        auto res = convert_(id, *this);
        res.data_type_id.endian = id.endian;
        return std::move(res);
    }

    value::operator const std::uint8_t*() const {
        return std::get<std::uint8_t*>(content());
    }

    value::operator const std::int8_t*() const {
        return std::get<std::int8_t*>(content());
    }

    value::operator const char*() const {
        return (char*)std::get<std::uint8_t*>(content());
    }

    value::operator const std::int16_t*() const {
        return std::get<std::int16_t*>(content());
    }

    value::operator const std::uint16_t*() const {
        return std::get<std::uint16_t*>(content());
    }

    value::operator const std::int32_t*() const {
        return std::get<std::int32_t*>(content());
    }

    value::operator const std::uint32_t*() const {
        return std::get<std::uint32_t*>(content());
    }

    value::operator const std::int64_t*() const {
        return std::get<std::int64_t*>(content());
    }

    value::operator const std::uint64_t*() const {
        return std::get<std::uint64_t*>(content());
    }

    value::operator std::unordered_map<std::string, value>() const {
        if (is_compound())
            return *(std::unordered_map<std::string, value>*)data;
        throw std::invalid_argument("Invalid tid, cannot convert compound");
    }

    value::operator raw_uuid() const {
        return std::get<raw_uuid>(content());
    }

    value::operator std::optional<value>() const {
        if (data_type_id.type == type::optional)
            if (data_type_id.is_signed)
                return std::optional<value>(*(const value*)data);
            else
                return std::nullopt;
        else
            return std::optional<value>(*this);
    }

    raw_uuid value::as_uuid() const {
        return (raw_uuid) * this;
    }

    const std::string& value::as_string() const {
        return (const std::string&)*this;
    }

    std::string& value::as_string() {
        return (std::string&)*this;
    }

    compound_ref value::as_compound() {
        return compound::make_ref(*this);
    }

    compound_const_ref value::as_compound() const {
        return compound::make_ref(*this);
    }

    dynamic_array_ref value::as_array() {
        return dynamic_array::make_ref(*this);
    }

    dynamic_array_ref value::as_array() const {
        return dynamic_array::make_ref(*this);
    }

    dynamic_array_ref value::as_dyn_array() {
        return dynamic_array::make_ref(*this);
    }

    dynamic_array_ref value::as_dyn_array() const {
        return dynamic_array::make_ref(*this);
    }

    fixed_array_ref value::as_fixed_array() {
        return fixed_array::make_ref(*this);
    }

    fixed_array_ref value::as_fixed_array() const {
        return fixed_array::make_ref(*this);
    }

    simple_array_ref_ui8 value::as_ui8_array() {
        return simple_array_ui8::make_ref(*this);
    }

    simple_array_const_ref_ui8 value::as_ui8_array() const {
        return simple_array_ui8::make_ref(*this);
    }

    simple_array_ref_ui16 value::as_ui16_array() {
        return simple_array_ui16::make_ref(*this);
    }

    simple_array_const_ref_ui16 value::as_ui16_array() const {
        return simple_array_ui16::make_ref(*this);
    }

    simple_array_ref_ui32 value::as_ui32_array() {
        return simple_array_ui32::make_ref(*this);
    }

    simple_array_const_ref_ui32 value::as_ui32_array() const {
        return simple_array_ui32::make_ref(*this);
    }

    simple_array_ref_ui64 value::as_ui64_array() {
        return simple_array_ui64::make_ref(*this);
    }

    simple_array_const_ref_ui64 value::as_ui64_array() const {
        return simple_array_ui64::make_ref(*this);
    }

    simple_array_ref_i8 value::as_i8_array() {
        return simple_array_i8::make_ref(*this);
    }

    simple_array_const_ref_i8 value::as_i8_array() const {
        return simple_array_i8::make_ref(*this);
    }

    simple_array_ref_i16 value::as_i16_array() {
        return simple_array_i16::make_ref(*this);
    }

    simple_array_const_ref_i16 value::as_i16_array() const {
        return simple_array_i16::make_ref(*this);
    }

    simple_array_ref_i32 value::as_i32_array() {
        return simple_array_i32::make_ref(*this);
    }

    simple_array_const_ref_i32 value::as_i32_array() const {
        return simple_array_i32::make_ref(*this);
    }

    simple_array_ref_i64 value::as_i64_array() {
        return simple_array_i64::make_ref(*this);
    }

    simple_array_const_ref_i64 value::as_i64_array() const {
        return simple_array_i64::make_ref(*this);
    }

    std::pair<std::string, value> value::copy_interator::operator*() const {
        switch (iterate_type.type) {
        case type::sarray:
            if (iterate_type.is_signed) {
                switch (iterate_type.length) {
                case type_len::Tiny:
                    return {"", value(*(std::int8_t*)pointer)};
                    break;
                case type_len::Short:
                    return {"", value(*(std::int16_t*)pointer)};
                    break;
                case type_len::Default:
                    return {"", value(*(std::int32_t*)pointer)};
                    break;
                case type_len::Long:
                    return {"", value(*(std::int64_t*)pointer)};
                    break;
                default:
                    break;
                }
            } else {
                switch (iterate_type.length) {
                case type_len::Tiny:
                    return {"", value(*(std::uint8_t*)pointer)};
                    break;
                case type_len::Short:
                    return {"", value(*(std::uint16_t*)pointer)};
                    break;
                case type_len::Default:
                    return {"", value(*(std::uint32_t*)pointer)};
                    break;
                case type_len::Long:
                    return {"", value(*(std::uint64_t*)pointer)};
                    break;
                default:
                    break;
                }
            }
            break;
        case type::array:
        case type::darray:
            return {"", value(*(*(std::vector<value>::iterator*)pointer))};
        case type::compound: {
            auto& tmp = (*(std::unordered_map<std::string, value>::iterator*)pointer);
            return std::pair<std::string, value>(
                tmp->first,
                value(tmp->second)
            );
        }
        }
        throw exception("Unreachable exception in non debug environment");
    }

    compound_ref compound_ref::merge(const value& copy) & {
        merge_compounds(*proxy, *copy.as_compound().proxy);
        return *this;
    }

    compound_ref compound_ref::merge(value&& move) & {
        merge_compounds(*proxy, std::move(*move.as_compound().proxy));
        return *this;
    }

    compound_ref compound_ref::merge(const std::unordered_map<std::string, value>& copy) & {
        merge_compounds(*proxy, copy);
        return *this;
    }

    compound_ref compound_ref::merge(std::unordered_map<std::string, value>&& move) & {
        merge_compounds(*proxy, std::move(move));
        return *this;
    }

    compound compound::merge(const value& copy) && {
        merge_compounds(*proxy, *copy.as_compound().proxy);
        return std::move(*this);
    }

    compound compound::merge(value&& copy) && {
        merge_compounds(*proxy, std::move(*copy.as_compound().proxy));
        return std::move(*this);
    }

    compound compound::merge(const std::unordered_map<std::string, value>& copy) && {
        merge_compounds(*proxy, copy);
        return std::move(*this);
    }

    compound compound::merge(std::unordered_map<std::string, value>&& copy) && {
        merge_compounds(*proxy, std::move(copy));
        return std::move(*this);
    }

    namespace io_helper {
        namespace __impl__ {
            template <class T>
            T _read_as_(std::istream& input_stream) {
                T res;
                input_stream.read((char*)&res, sizeof(T));
                return res;
            }
        }

        template <class T>
        T read_var(std::istream& read_stream, std::endian endian) {
            static constexpr size_t max_offset = (sizeof(T) / 5 * 5 + ((sizeof(T) % 5) > 0)) * 8;
            std::make_unsigned_t<T> decoded_int = 0;
            size_t bit_offset = 0;
            char current_byte = 0;
            do {
                if (bit_offset == max_offset)
                    throw enbt::exception("var value too big");
                current_byte = __impl__::_read_as_<char>(read_stream);
                decoded_int |= T(current_byte & 0b01111111) << bit_offset;
                bit_offset += 7;
            } while ((current_byte & 0b10000000) != 0);
            if constexpr (std::is_signed_v<T>)
                return endian_helpers::convert_endian(endian, (T)((decoded_int >> 1) ^ -(decoded_int & 1)));
            else
                return endian_helpers::convert_endian(endian, decoded_int);
        }

        template <class T>
        T read_value(std::istream& read_stream, std::endian endian = std::endian::native) {
            T tmp;
            if constexpr (std::is_same<T, enbt::raw_uuid>()) {
                read_stream.read((char*)tmp.data, 16);
                endian_helpers::convert_endian(endian, tmp.data, 16);
            } else {
                read_stream.read((char*)&tmp, sizeof(T));
                endian_helpers::convert_endian(endian, tmp);
            }
            return tmp;
        }

        template <class T>
        T* read_array(std::istream& read_stream, std::size_t len, std::endian endian = std::endian::native) {
            T* tmp = new T[len];
            if constexpr (sizeof(T) == 1)
                read_stream.read((char*)tmp, len);
            else {
                read_stream.read((char*)tmp, len * sizeof(T));
                endian_helpers::convert_endian(endian, tmp, len);
            }
            return tmp;
        }

        void write_compress_len(std::ostream& write_stream, std::uint64_t len) {
            union {
                std::uint64_t full = 0;
                std::uint8_t part[8];

            } b;

            b.full = endian_helpers::convert_endian(std::endian::big, len);

            constexpr struct {
                std::uint64_t b64 : 62 = -1;
                std::uint64_t b32 : 30 = -1;
                std::uint64_t b16 : 14 = -1;
                std::uint64_t b8 : 6 = -1;
            } m;

            if (len <= m.b8) {
                write_stream << b.part[7];
            } else if (len <= m.b16) {
                b.part[0] |= 1;
                write_stream << b.part[7];
                write_stream << b.part[6];
            } else if (len <= m.b32) {
                b.part[0] |= 2;
                write_stream << b.part[7];
                write_stream << b.part[6];
                write_stream << b.part[5];
                write_stream << b.part[4];
            } else if (len <= m.b64) {
                b.part[0] |= 3;
                write_stream << b.part[7];
                write_stream << b.part[6];
                write_stream << b.part[5];
                write_stream << b.part[4];
                write_stream << b.part[3];
                write_stream << b.part[2];
                write_stream << b.part[1];
                write_stream << b.part[0];
            } else
                throw std::overflow_error("uint64_t cannot put in to uint60_t");
        }

        template <class t>
        void write_var(std::ostream& write_stream, t value, std::endian endian = std::endian::native) {
            constexpr size_t sign_bit_offset = sizeof(t) * 8 - 1;
            constexpr t least_bits = ~t(0x7f);

            value = endian_helpers::convert_endian(endian, value);
            if constexpr (std::is_signed_v<t>)
                value = ((value) << 1) ^ ((value) >> sign_bit_offset);
            while (value & least_bits) {
                write_stream << (uint8_t(value) | 0x80);
                value >>= 7;
            }
            write_stream << uint8_t(value);
        }

        template <class t>
        void write_var(std::ostream& write_stream, value::value_variants value, std::endian endian = std::endian::native) {
            write_var(write_stream, std::get<t>(value), endian);
        }

        void write_type_id(std::ostream& write_stream, enbt::type_id tid) {
            write_stream << tid.raw;
        }

        template <class t>
        void write_value(std::ostream& write_stream, t value, std::endian endian = std::endian::native) {
            if constexpr (std::is_same<t, enbt::raw_uuid>())
                endian_helpers::convert_endian(endian, value.data, 16);
            else
                value = endian_helpers::convert_endian(endian, value);
            write_stream.write((char*)&value, sizeof(t));
        }

        template <class t>
        void write_value(std::ostream& write_stream, value::value_variants value, std::endian endian = std::endian::native) {
            return write_value(write_stream, std::get<t>(value), endian);
        }

        template <class t>
        void write_array(std::ostream& write_stream, t* values, std::size_t len, std::endian endian = std::endian::native) {
            if constexpr (sizeof(t) == 1) {
                write_stream.write((const char*)values, len);
            } else {
                endian_helpers::convert_endian(endian, values, len);
                write_stream.write((char*)values, len * sizeof(t));
                endian_helpers::convert_endian(endian, values, len);
            }
        }

        template <class t>
        void write_array(std::ostream& write_stream, value::value_variants* values, std::size_t len, std::endian endian = std::endian::native) {
            std::vector<t> arr(len);
            for (std::size_t i = 0; i < len; i++)
                arr[i] = std::get<t>(values[i]);
            write_array(write_stream, arr.data(), len, endian);
        }

        void write_string(std::ostream& write_stream, const value& val) {
            const std::string& str_ref = (const std::string&)val;
            std::size_t real_size = str_ref.size();
            std::size_t size_without_null = real_size ? (str_ref[real_size - 1] != 0 ? real_size : real_size - 1) : 0;
            write_compress_len(write_stream, size_without_null);
            write_array(write_stream, str_ref.data(), size_without_null);
        }

        template <class t>
        void write_define_len(std::ostream& write_stream, t value) {
            return write_value(write_stream, value, std::endian::little);
        }

        void write_define_len(std::ostream& write_stream, std::uint64_t len, enbt::type_id tid) {
            switch (tid.length) {
            case enbt::type_len::Tiny:
                if (len != ((std::uint8_t)len))
                    throw enbt::exception("cannot convert value to std::uint8_t");
                write_define_len(write_stream, (std::uint8_t)len);
                break;
            case enbt::type_len::Short:
                if (len != ((std::uint16_t)len))
                    throw enbt::exception("cannot convert value to std::uint16_t");
                write_define_len(write_stream, (std::uint16_t)len);
                break;
            case enbt::type_len::Default:
                if (len != ((std::uint32_t)len))
                    throw enbt::exception("cannot convert value to std::uint32_t");
                write_define_len(write_stream, (std::uint32_t)len);
                break;
            case enbt::type_len::Long:
                return write_define_len(write_stream, (std::uint64_t)len);
                break;
            }
        }

        void initialize_version(std::ostream& write_stream) {
            write_stream << (char)ENBT_VERSION_HEX;
        }

        void write_compound(std::ostream& write_stream, const value& val) {
            auto result = std::get<std::unordered_map<std::string, value>*>(val.content());
            write_define_len(write_stream, result->size(), val.type_id());
            for (auto& it : *result) {
                write_string(write_stream, it.first);
                write_token(write_stream, it.second);
            }
        }

        void write_array(std::ostream& write_stream, const value& val) {
            if (!val.is_array())
                throw enbt::exception("this is not array for serialize it");
            auto result = (std::vector<value>*)val.get_internal_ptr();
            std::size_t len = result->size();
            write_define_len(write_stream, len, val.type_id());
            if (len) {
                enbt::type_id tid = (*result)[0].type_id();
                write_type_id(write_stream, tid);
                if (tid.type != enbt::type::bit) {
                    for (const auto& it : *result)
                        write_value(write_stream, it);
                } else {
                    tid.is_signed = false;
                    std::int8_t i = 0;
                    std::uint8_t value = 0;
                    for (auto& it : *result) {
                        if (i >= 8) {
                            i = 0;
                            write_stream << value;
                        }
                        if (i)
                            value = (((bool)it) << i);
                        else
                            value = (bool)it;
                    }
                    if (i)
                        write_stream << value;
                }
            }
        }

        void write_darray(std::ostream& write_stream, const value& val) {
            if (!val.is_array())
                throw enbt::exception("this is not array for serialize it");
            auto result = (std::vector<value>*)val.get_internal_ptr();
            write_define_len(write_stream, result->size(), val.type_id());
            for (auto& it : *result)
                write_token(write_stream, it);
        }

        void write_simple_array(std::ostream& write_stream, const value& val) {
            write_compress_len(write_stream, val.size());
            switch (val.type_id().length) {
            case enbt::type_len::Tiny:
                write_array(write_stream, val.get_internal_ptr(), val.size());
                break;
            case enbt::type_len::Short:
                if (val.type_id().is_signed)
                    write_array(write_stream, (std::uint16_t*)val.get_internal_ptr(), val.size(), val.type_id().get_endian());
                else
                    write_array(write_stream, (std::uint16_t*)val.get_internal_ptr(), val.size());
                break;
            case enbt::type_len::Default:
                if (val.type_id().is_signed)
                    write_array(write_stream, (std::uint32_t*)val.get_internal_ptr(), val.size(), val.type_id().get_endian());
                else
                    write_array(write_stream, (std::uint32_t*)val.get_internal_ptr(), val.size());
                break;
            case enbt::type_len::Long:
                if (val.type_id().is_signed)
                    write_array(write_stream, (std::uint64_t*)val.get_internal_ptr(), val.size(), val.type_id().get_endian());
                else
                    write_array(write_stream, (std::uint64_t*)val.get_internal_ptr(), val.size());
                break;
            default:
                break;
            }
        }

        void write_log_item(std::ostream& write_stream, const value& val) {
            std::stringstream ss;
            ss << std::ios::binary;
            write_token(ss, val);
            write_compress_len(write_stream, ss.view().size());
            write_array(write_stream, (uint8_t*)ss.view().data(), ss.view().size());
        }

        void write_value(std::ostream& write_stream, const value& val) {
            enbt::type_id tid = val.type_id();
            switch (tid.type) {
            case enbt::type::integer:
                switch (tid.length) {
                case enbt::type_len::Tiny:
                    if (tid.is_signed)
                        return write_value<std::int8_t>(write_stream, val.content(), tid.get_endian());
                    else
                        return write_value<std::uint8_t>(write_stream, val.content(), tid.get_endian());
                case enbt::type_len::Short:
                    if (tid.is_signed)
                        return write_value<std::int16_t>(write_stream, val.content(), tid.get_endian());
                    else
                        return write_value<std::uint16_t>(write_stream, val.content(), tid.get_endian());
                case enbt::type_len::Default:
                    if (tid.is_signed)
                        return write_value<std::int32_t>(write_stream, val.content(), tid.get_endian());
                    else
                        return write_value<std::uint32_t>(write_stream, val.content(), tid.get_endian());
                case enbt::type_len::Long:
                    if (tid.is_signed)
                        return write_value<std::int64_t>(write_stream, val.content(), tid.get_endian());
                    else
                        return write_value<std::uint64_t>(write_stream, val.content(), tid.get_endian());
                }
                return;
            case enbt::type::floating:
                switch (tid.length) {
                case enbt::type_len::Tiny:
                case enbt::type_len::Short:
                    throw enbt::exception("not implemented");
                case enbt::type_len::Default:
                    return write_value<float>(write_stream, val.content(), tid.get_endian());
                case enbt::type_len::Long:
                    return write_value<double>(write_stream, val.content(), tid.get_endian());
                }
                return;
            case enbt::type::var_integer:
                switch (tid.length) {
                case enbt::type_len::Tiny:
                case enbt::type_len::Short:
                    throw enbt::exception("not implemented");
                case enbt::type_len::Default:
                    if (tid.is_signed)
                        return write_var<std::int32_t>(write_stream, val.content(), tid.get_endian());
                    else
                        return write_var<std::uint32_t>(write_stream, val.content(), tid.get_endian());
                case enbt::type_len::Long:
                    if (tid.is_signed)
                        return write_var<std::int64_t>(write_stream, val.content(), tid.get_endian());
                    else
                        return write_var<std::uint64_t>(write_stream, val.content(), tid.get_endian());
                }
                return;
            case enbt::type::comp_integer:
                return write_compress_len(write_stream, (uint64_t)val);
            case enbt::type::uuid:
                return write_value<enbt::raw_uuid>(write_stream, val.content(), tid.get_endian());
            case enbt::type::sarray:
                return write_simple_array(write_stream, val);
            case enbt::type::darray:
                return write_darray(write_stream, val);
            case enbt::type::compound:
                return write_compound(write_stream, val);
            case enbt::type::array:
                return write_array(write_stream, val);
            case enbt::type::optional:
                if (val.contains())
                    write_token(write_stream, *val.get_optional());
                break;
            case enbt::type::string:
                return write_string(write_stream, val);
            case enbt::type::log_item: {
            }
            }
        }

        void write_token(std::ostream& write_stream, const value& val) {
            write_stream << val.type_id().raw;
            write_value(write_stream, val);
        }

        enbt::type_id read_type_id(std::istream& read_stream) {
            enbt::type_id result;
            result.raw = __impl__::_read_as_<std::uint8_t>(read_stream);
            return result;
        }

        template <class T>
        T read_define_len(std::istream& read_stream) {
            return read_value<T>(read_stream, std::endian::little);
        }

        std::size_t read_define_len(std::istream& read_stream, enbt::type_id tid) {
            switch (tid.length) {
            case enbt::type_len::Tiny:
                return read_define_len<std::uint8_t>(read_stream);
            case enbt::type_len::Short:
                return read_define_len<std::uint16_t>(read_stream);
            case enbt::type_len::Default:
                return read_define_len<std::uint32_t>(read_stream);
            case enbt::type_len::Long: {
                std::uint64_t val = read_define_len<std::uint64_t>(read_stream);
                if ((std::size_t)val != val)
                    throw std::overflow_error("array length too big for this platform");
                return val;
            }
            default:
                return 0;
            }
        }

        std::uint64_t read_define_len64(std::istream& read_stream, enbt::type_id tid) {
            switch (tid.length) {
            case enbt::type_len::Tiny:
                return read_define_len<std::uint8_t>(read_stream);
            case enbt::type_len::Short:
                return read_define_len<std::uint16_t>(read_stream);
            case enbt::type_len::Default:
                return read_define_len<std::uint32_t>(read_stream);
            case enbt::type_len::Long:
                return read_define_len<std::uint64_t>(read_stream);
            default:
                return 0;
            }
        }

        std::uint64_t read_compress_len(std::istream& read_stream) {
            union {
                std::uint8_t complete = 0;

                struct {
                    std::uint8_t len : 6;
                    std::uint8_t len_flag : 2;
                } partial;
            } b;

            read_stream.read((char*)&b.complete, 1);
            switch (b.partial.len_flag) {
            case 0:
                return b.partial.len;
            case 1: {
                std::uint16_t full = b.partial.len;
                full <<= 8;
                full |= __impl__::_read_as_<std::uint8_t>(read_stream);
                return endian_helpers::convert_endian(std::endian::little, full);
            }
            case 2: {
                std::uint32_t full = b.partial.len;
                std::uint8_t buf[3];
                read_stream.read((char*)buf, 3);
                full <<= 24;
                full |= buf[2];
                full <<= 16;
                full |= buf[1];
                full <<= 8;
                full |= buf[0];
                return endian_helpers::convert_endian(std::endian::little, full);
            }
            case 3: {
                std::uint64_t full = b.partial.len;
                std::uint8_t buf[7];
                read_stream.read((char*)buf, 7);
                full <<= 56;
                full |= buf[6];
                full <<= 48;
                full |= buf[5];
                full <<= 40;
                full |= buf[4];
                full <<= 24;
                full |= buf[3];
                full <<= 24;
                full |= buf[2];
                full <<= 16;
                full |= buf[1];
                full <<= 8;
                full |= buf[0];
                return endian_helpers::convert_endian(std::endian::little, full);
            }
            default:
                return 0;
            }
        }

        std::string read_string(std::istream& read_stream) {
            std::uint64_t read = read_compress_len(read_stream);
            std::string res;
            res.resize(read);
            read_stream.read(res.data(), read);
            return res;
        }

        value read_compound(std::istream& read_stream, enbt::type_id tid) {
            std::size_t len = read_define_len(read_stream, tid);
            std::unordered_map<std::string, value> result;
            result.reserve(len);
            for (std::size_t i = 0; i < len; i++) {
                std::string key = read_string(read_stream);
                result[key] = read_token(read_stream);
            }
            return result;
        }

        std::vector<value> read_array(std::istream& read_stream, enbt::type_id tid) {
            std::size_t len = read_define_len(read_stream, tid);
            if (!len)
                return {};
            enbt::type_id a_tid = read_type_id(read_stream);
            std::vector<value> result(len);
            if (a_tid == enbt::type::bit) {
                std::int8_t i = 0;
                std::uint8_t value = __impl__::_read_as_<std::uint8_t>(read_stream);
                for (auto& it : result) {
                    if (i >= 8) {
                        i = 0;
                        value = __impl__::_read_as_<std::uint8_t>(read_stream);
                    }
                    it = (bool)(value & (1 << i));
                }
            } else {
                for (std::size_t i = 0; i < len; i++)
                    result[i] = read_value(read_stream, a_tid);
            }
            return result;
        }

        std::vector<value> read_darray(std::istream& read_stream, enbt::type_id tid) {
            std::size_t len = read_define_len(read_stream, tid);
            std::vector<value> result(len);
            for (std::size_t i = 0; i < len; i++)
                result[i] = read_token(read_stream);
            return result;
        }

        value read_sarray(std::istream& read_stream, enbt::type_id tid) {
            std::uint64_t len = read_compress_len(read_stream);
            auto endian = tid.get_endian();
            switch (tid.length) {
            case enbt::type_len::Tiny: {
                std::uint8_t* arr = read_array<std::uint8_t>(read_stream, len, endian);
                return enbt::value::build_inline(arr, len, tid);
            }
            case enbt::type_len::Short: {
                std::uint16_t* arr = read_array<std::uint16_t>(read_stream, len, endian);
                return enbt::value::build_inline((std::uint8_t*)arr, len, tid);
            }
            case enbt::type_len::Default: {
                std::uint32_t* arr = read_array<std::uint32_t>(read_stream, len, endian);
                return enbt::value::build_inline((std::uint8_t*)arr, len, tid);
            }
            case enbt::type_len::Long: {
                std::uint64_t* arr = read_array<std::uint64_t>(read_stream, len, endian);
                return enbt::value::build_inline((std::uint8_t*)arr, len, tid);
            }
            default:
                throw enbt::exception();
            }
        }

        value read_log_item(std::istream& read_stream) {
            read_compress_len(read_stream);
            return read_token(read_stream);
        }

        value read_value(std::istream& read_stream, enbt::type_id tid) {
            switch (tid.type) {
            case enbt::type::integer:
                switch (tid.length) {
                case enbt::type_len::Tiny:
                    if (tid.is_signed)
                        return read_value<std::int8_t>(read_stream, tid.get_endian());
                    else
                        return read_value<std::uint8_t>(read_stream, tid.get_endian());
                case enbt::type_len::Short:
                    if (tid.is_signed)
                        return read_value<std::int16_t>(read_stream, tid.get_endian());
                    else
                        return read_value<std::uint16_t>(read_stream, tid.get_endian());
                case enbt::type_len::Default:
                    if (tid.is_signed)
                        return read_value<std::int32_t>(read_stream, tid.get_endian());
                    else
                        return read_value<std::uint32_t>(read_stream, tid.get_endian());
                case enbt::type_len::Long:
                    if (tid.is_signed)
                        return read_value<std::int64_t>(read_stream, tid.get_endian());
                    else
                        return read_value<std::uint64_t>(read_stream, tid.get_endian());
                default:
                    return value();
                }
            case enbt::type::floating:
                switch (tid.length) {
                case enbt::type_len::Default:
                    return read_value<float>(read_stream, tid.get_endian());
                case enbt::type_len::Long:
                    return read_value<double>(read_stream, tid.get_endian());
                default:
                    return value();
                }
            case enbt::type::var_integer:
                switch (tid.length) {
                case enbt::type_len::Default:
                    if (tid.is_signed)
                        return read_var<std::int32_t>(read_stream, tid.get_endian());
                    else
                        return read_var<std::uint32_t>(read_stream, tid.get_endian());
                case enbt::type_len::Long:
                    if (tid.is_signed)
                        return read_var<std::int64_t>(read_stream, tid.get_endian());
                    else
                        return read_var<std::uint64_t>(read_stream, tid.get_endian());
                default:
                    return value();
                }
            case enbt::type::comp_integer: {
                std::uint64_t val = read_compress_len(read_stream);
                switch (tid.length) {
                case enbt::type_len::Tiny:
                    if (val != ((std::uint8_t)val))
                        throw enbt::exception("invalid encoding for comp_integer, type is too small");
                    return value((std::uint8_t)val);
                case enbt::type_len::Short:
                    if (val != ((std::uint16_t)val))
                        throw enbt::exception("invalid encoding for comp_integer, type is too small");
                    return value((std::uint16_t)val);
                case enbt::type_len::Default:
                    if (val != ((std::uint32_t)val))
                        throw enbt::exception("invalid encoding for comp_integer, type is too small");
                    return value((std::uint32_t)val);
                case enbt::type_len::Long:
                    return value(val);
                default:
                    return value();
                }
            }
            case enbt::type::uuid:
                return read_value<enbt::raw_uuid>(read_stream, tid.get_endian());
            case enbt::type::sarray:
                return read_sarray(read_stream, tid);
            case enbt::type::darray:
                return value(read_darray(read_stream, tid), tid);
            case enbt::type::compound:
                return read_compound(read_stream, tid);
            case enbt::type::array:
                return value(read_array(read_stream, tid), tid);
            case enbt::type::optional:
                return tid.is_signed ? value(true, read_token(read_stream)) : value(false, value());
            case enbt::type::bit:
                return value((bool)tid.is_signed);
            case enbt::type::string:
                return read_string(read_stream);
            default:
                return value();
            }
        }

        value read_token(std::istream& read_stream) {
            return read_value(read_stream, read_type_id(read_stream));
        }

        value read_file(std::istream& read_stream) {
            check_version(read_stream);
            return read_token(read_stream);
        }

        std::vector<value> read_list_file(std::istream& read_stream) {
            check_version(read_stream);
            std::vector<value> result;
            while (!read_stream.eof())
                result.push_back(read_token(read_stream));
            return result;
        }

        void check_version(std::istream& read_stream) {
            if (read_value<std::uint8_t>(read_stream) != ENBT_VERSION_HEX)
                throw enbt::exception("unsupported version");
        }

        void skip_compound(std::istream& read_stream, enbt::type_id tid) {
            std::uint64_t len = read_define_len64(read_stream, tid);
            for (std::uint64_t i = 0; i < len; i++) {
                skip_string(read_stream);
                skip_token(read_stream);
            }
        }

        std::uint8_t can_fast_index(enbt::type_id tid) {
            switch (tid.type) {
            case enbt::type::integer:
            case enbt::type::floating:
                switch (tid.length) {
                case enbt::type_len::Tiny:
                    return 1;
                case enbt::type_len::Short:
                    return 2;
                case enbt::type_len::Default:
                    return 4;
                case enbt::type_len::Long:
                    return 8;
                }
                return 0;
            case enbt::type::uuid:
                return 16;
            case enbt::type::bit:
                return 1;
            default:
                return 0;
            }
        }

        void skip_array(std::istream& read_stream, enbt::type_id tid) {
            std::uint64_t len = read_define_len64(read_stream, tid);
            if (!len)
                return;
            auto items_tid = read_type_id(read_stream);
            if (int index_multiplier = can_fast_index(items_tid); !index_multiplier)
                for (std::uint64_t i = 0; i < len; i++)
                    skip_value(read_stream, items_tid);
            else {
                if (tid == enbt::type::bit) {
                    std::uint64_t actual_len = len / 8;
                    if (len % 8)
                        ++actual_len;
                    read_stream.seekg(read_stream.tellg() += actual_len);
                } else
                    read_stream.seekg(read_stream.tellg() += len * index_multiplier);
            }
        }

        void skip_darray(std::istream& read_stream, enbt::type_id tid) {
            std::uint64_t len = read_define_len64(read_stream, tid);
            for (std::uint64_t i = 0; i < len; i++)
                skip_token(read_stream);
        }

        void skip_sarray(std::istream& read_stream, enbt::type_id tid) {
            std::uint64_t index = read_compress_len(read_stream);
            switch (tid.length) {
            case enbt::type_len::Tiny:
                read_stream.seekg(read_stream.tellg() += index);
                break;
            case enbt::type_len::Short:
                read_stream.seekg(read_stream.tellg() += index * 2);
                break;
            case enbt::type_len::Default:
                read_stream.seekg(read_stream.tellg() += index * 4);
                break;
            case enbt::type_len::Long:
                read_stream.seekg(read_stream.tellg() += index * 8);
                break;
            default:
                break;
            }
        }

        void skip_string(std::istream& read_stream) {
            std::uint64_t len = read_compress_len(read_stream);
            read_stream.seekg(read_stream.tellg() += len);
        }

        void skip_log_item(std::istream& read_stream) {
            uint64_t val = read_compress_len(read_stream);
            read_stream.seekg(read_stream.tellg() += val);
        }

        void skip_value(std::istream& read_stream, enbt::type_id tid) {
            switch (tid.type) {
            case enbt::type::floating:
            case enbt::type::integer:
                switch (tid.length) {
                case enbt::type_len::Tiny:
                    read_stream.seekg(read_stream.tellg() += 1);
                    break;
                case enbt::type_len::Short:
                    read_stream.seekg(read_stream.tellg() += 2);
                    break;
                case enbt::type_len::Default:
                    read_stream.seekg(read_stream.tellg() += 4);
                    break;
                case enbt::type_len::Long:
                    read_stream.seekg(read_stream.tellg() += 8);
                    break;
                }
                break;
            case enbt::type::var_integer:
                switch (tid.length) {
                case enbt::type_len::Tiny:
                case enbt::type_len::Short:
                    throw enbt::exception("not implemented");
                case enbt::type_len::Default:
                    if (tid.is_signed)
                        read_var<std::int32_t>(read_stream, std::endian::native);
                    else
                        read_var<std::uint32_t>(read_stream, std::endian::native);
                    break;
                case enbt::type_len::Long:
                    if (tid.is_signed)
                        read_var<std::int64_t>(read_stream, std::endian::native);
                    else
                        read_var<std::uint64_t>(read_stream, std::endian::native);
                }
                break;
            case enbt::type::comp_integer: {
                uint64_t val = read_compress_len(read_stream);
                switch (tid.length) {
                case enbt::type_len::Tiny:
                    if (val != ((std::uint8_t)val))
                        throw enbt::exception("invalid encoding for comp_integer, type is too small");
                    break;
                case enbt::type_len::Short:
                    if (val != ((std::uint16_t)val))
                        throw enbt::exception("invalid encoding for comp_integer, type is too small");
                    break;
                case enbt::type_len::Default:
                    if (val != ((std::uint32_t)val))
                        throw enbt::exception("invalid encoding for comp_integer, type is too small");
                    break;
                case enbt::type_len::Long:
                    break;
                default:
                    break;
                }
                break;
            }
            case enbt::type::uuid:
                read_stream.seekg(read_stream.tellg() += 16);
                break;
            case enbt::type::sarray:
                skip_sarray(read_stream, tid);
                break;
            case enbt::type::darray:
                skip_darray(read_stream, tid);
                break;
            case enbt::type::compound:
                skip_compound(read_stream, tid);
                break;
            case enbt::type::array:
                skip_array(read_stream, tid);
                break;
            case enbt::type::optional:
                if (tid.is_signed)
                    skip_token(read_stream);
                break;
            case enbt::type::none:
            case enbt::type::bit:
                break;
            case enbt::type::string:
                skip_string(read_stream);
                break;
            case enbt::type::log_item:
                skip_log_item(read_stream);
                break;
            }
        }

        void skip_token(std::istream& read_stream) {
            return skip_value(read_stream, read_type_id(read_stream));
        }

        bool find_value_compound(std::istream& read_stream, enbt::type_id tid, const std::string& key) {
            std::size_t len = read_define_len(read_stream, tid);
            for (std::size_t i = 0; i < len; i++) {
                if (read_string(read_stream) != key)
                    skip_value(read_stream, read_type_id(read_stream));
                else
                    return true;
            }
            return false;
        }

        void index_static_array(std::istream& read_stream, std::uint64_t index, std::uint64_t len, enbt::type_id target_id) {
            if (index >= len)
                throw enbt::exception('[' + std::to_string(index) + "] out of range " + std::to_string(len));
            if (std::uint8_t skipper = can_fast_index(target_id)) {
                if (target_id != enbt::type::bit)
                    read_stream.seekg(read_stream.tellg() += index * skipper);
                else
                    read_stream.seekg(read_stream.tellg() += index / 8);
            } else
                for (std::uint64_t i = 0; i < index; i++)
                    skip_value(read_stream, target_id);
        }

        void index_dyn_array(std::istream& read_stream, std::uint64_t index, std::uint64_t len) {
            if (index >= len)
                throw enbt::exception('[' + std::to_string(index) + "] out of range " + std::to_string(len));
            for (std::uint64_t i = 0; i < index; i++)
                skip_token(read_stream);
        }

        void index_array(std::istream& read_stream, std::uint64_t index, enbt::type_id arr_tid) {
            switch (arr_tid.type) {
            case enbt::type::array: {
                std::uint64_t len = read_define_len64(read_stream, arr_tid);
                if (!len)
                    throw enbt::exception("this array is empty");
                auto target_id = read_type_id(read_stream);
                index_static_array(read_stream, index, len, target_id);
                break;
            }
            case enbt::type::darray:
                index_dyn_array(read_stream, index, read_define_len64(read_stream, arr_tid));
                break;
            default:
                throw enbt::exception("invalid type id");
            }
        }

        void index_array(std::istream& read_stream, std::uint64_t index) {
            index_array(read_stream, index, read_type_id(read_stream));
        }

        value_path::index::operator std::string() const {
            if (std::holds_alternative<std::string>(value))
                return std::get<std::string>(value);
            else
                return std::to_string(std::get<std::uint64_t>(value));
        }

        value_path::index::operator std::uint64_t() const {
            if (std::holds_alternative<std::string>(value))
                return std::stoull(std::get<std::string>(value));
            else
                return std::get<std::uint64_t>(value);
        }

        std::vector<std::string> split_s(std::string str, std::string delimiter) {
            std::vector<std::string> res;
            std::size_t pos = 0;
            std::string token;
            while ((pos = str.find(delimiter)) != std::string::npos) {
                token = str.substr(0, pos);
                res.push_back(token);
                str.erase(0, pos + delimiter.length());
            }
            res.push_back(str);
            return res;
        }

        value_path::value_path(std::string_view stringized_path) {
            for (auto&& tmp : split_s(std::string(stringized_path), "."))
                path.push_back({tmp});
        }

        value_path::value_path(const std::vector<index>& copy)
            : path(copy) {}

        value_path::value_path(std::vector<index>&& move)
            : path(std::move(move)) {}

        value_path::value_path(const value_path& copy)
            : path(copy.path) {}

        value_path::value_path(value_path&& move)
            : path(std::move(move.path)) {}

        value_path&& value_path::operator[](std::string_view index) {
            path.push_back({(std::string)index});
            return std::move(*this);
        }

        value_path&& value_path::operator[](std::uint64_t index) {
            path.push_back({index});
            return std::move(*this);
        }

        bool move_to_value_path(std::istream& read_stream, const value_path& value_path) {
            try {
                for (auto&& tmp : value_path.path) {
                    auto tid = read_type_id(read_stream);
                    switch (tid.type) {
                    case enbt::type::array:
                    case enbt::type::darray:
                        index_array(read_stream, tmp, tid);
                        continue;
                    case enbt::type::compound:
                        if (!find_value_compound(read_stream, tid, tmp))
                            return false;
                        continue;
                    default:
                        return false;
                    }
                }
                return true;
            } catch (const std::out_of_range&) {
                throw;
            } catch (const std::exception&) {
                return false;
            }
        }

        value get_value_path(std::istream& read_stream, const value_path& value_path) {
            auto old_pos = read_stream.tellg();
            bool is_bit_value = false;
            bool bit_value;
            try {
                for (auto&& tmp : value_path.path) {
                    auto tid = read_type_id(read_stream);
                    switch (tid.type) {
                    case enbt::type::array: {
                        std::uint64_t len = read_define_len64(read_stream, tid);
                        auto target_id = read_type_id(read_stream);
                        std::uint64_t index = tmp;
                        index_static_array(read_stream, index, len, target_id);
                        if (target_id.type == enbt::type::bit) {
                            is_bit_value = true;
                            bit_value = __impl__::_read_as_<std::uint8_t>(read_stream) << index % 8;
                        }
                        continue;
                    }
                    case enbt::type::darray:
                        index_array(read_stream, std::stoull(tmp), tid);
                        continue;
                    case enbt::type::compound:
                        if (!find_value_compound(read_stream, tid, tmp))
                            return false;
                        continue;
                    default:
                        throw std::invalid_argument("invalid path to value");
                    }
                }
            } catch (...) {
                read_stream.seekg(old_pos);
                throw;
            }
            read_stream.seekg(old_pos);
            if (is_bit_value)
                return bit_value;
            return read_token(read_stream);
        }

        value_read_stream::value_read_stream(std::istream& read_stream)
            : read_stream(read_stream) {
            current_type_id = read_type_id(read_stream);
        }

        value_read_stream::~value_read_stream() {
            if (!readed)
                skip();
        }

        enbt::value value_read_stream::read() {
            if (readed)
                throw enbt::exception("Invalid read state, item has been already readed");
            readed = true;
            return read_value(read_stream, current_type_id);
        }

        void value_read_stream::skip() {
            if (readed)
                throw enbt::exception("Invalid read state, item has been already readed");
            skip_value(read_stream, current_type_id);
            readed = true;
        }

        size_t value_read_stream::peek_size() {
            if (readed)
                throw enbt::exception("Invalid read state, item has been already readed");
            auto old_state = read_stream.rdstate();
            auto old_pos = read_stream.tellg();
            std::uint64_t len = 0;
            if (current_type_id.type == enbt::type::compound) {
                len = read_define_len64(read_stream, current_type_id);
            } else if (current_type_id.type == enbt::type::array) {
                len = read_define_len64(read_stream, current_type_id);
            } else if (current_type_id.type == enbt::type::darray) {
                len = read_define_len64(read_stream, current_type_id);
            } else if (current_type_id.type == enbt::type::sarray) {
                len = read_compress_len(read_stream);
            } else if (current_type_id.type == enbt::type::string) {
                len = read_compress_len(read_stream);
            } else {
                read_stream.setstate(old_state);
                read_stream.seekg(old_pos);
                return 0;
            }
            read_stream.setstate(old_state);
            read_stream.seekg(old_pos);
            return len;
        }

        value_write_stream::darray::darray(std::ostream& write_stream, bool write_type_id)
            : write_stream(write_stream), type_id_written(write_type_id) {
            if (write_type_id) {
                write_stream.write("\0\0\0\0\0\0\0\0\0", 9); //typeid + len
                type_size_field_pos = write_stream.tellp();
                type_size_field_pos -= 9;
            } else {
                write_stream.write("\0\0\0\0\0\0\0\0", 8);
                type_size_field_pos = write_stream.tellp();
                type_size_field_pos -= 8;
            }
        }

        value_write_stream::darray::~darray() {
            auto pos = write_stream.tellp();
            write_stream.seekp(type_size_field_pos);
            auto typ = enbt::type_id(enbt::type::darray, enbt::type_len::Long);
            if (type_id_written)
                write_type_id(write_stream, typ);
            write_define_len(write_stream, (uint64_t)items);
            write_stream.seekp(pos);
        }

        value_write_stream::darray& value_write_stream::darray::write(const enbt::value& value) {
            write_token(write_stream, value);
            items++;
            return *this;
        }

        value_write_stream::array::array(std::ostream& write_stream, size_t size, bool write_type_id_)
            : write_stream(write_stream), items_to_write(size) {
            enbt::type_id type(enbt::type::array, enbt::calc_type_len(size));
            if (write_type_id_)
                write_type_id(write_stream, type);
            write_define_len(write_stream, size, type);
        }

        value_write_stream::array::~array() {}

        value_write_stream::array& value_write_stream::array::write(const enbt::value& it) {
            if (items_to_write == 0)
                throw std::invalid_argument("array is full");
            if (!type_set) {
                current_type_id = it.type_id();
                write_type_id(write_stream, current_type_id);
                type_set = true;
            }
            write_value(write_stream, it);
            items_to_write--;
            return *this;
        }

        value_write_stream::compound::compound(std::ostream& write_stream, bool set_type_id)
            : write_stream(write_stream), type_id_written(set_type_id) {
            if (set_type_id) {
                write_stream.write("\0\0\0\0\0\0\0\0\0", 9); //typeid + len
                type_size_field_pos = write_stream.tellp();
                type_size_field_pos -= 9;
            } else {
                write_stream.write("\0\0\0\0\0\0\0\0", 8);
                type_size_field_pos = write_stream.tellp();
                type_size_field_pos -= 8;
            }
        }

        value_write_stream::compound::~compound() {
            auto pos = write_stream.tellp();
            write_stream.seekp(type_size_field_pos);
            auto typ = enbt::type_id(enbt::type::compound, enbt::type_len::Long);
            if (type_id_written)
                write_type_id(write_stream, typ);
            write_define_len(write_stream, (uint64_t)items);
            write_stream.seekp(pos);
        }

        value_write_stream::compound& value_write_stream::compound::write(std::string_view filed_name, const enbt::value& value) {
            write_string(write_stream, filed_name);
            write_token(write_stream, value);
            items++;
            return *this;
        }

        void value_write_stream::write(const enbt::value& value) {
            written_type_id = value.type_id();
            if (need_to_write_type_id)
                write_token(write_stream, value);
            else
                write_value(write_stream, value);
        }

        value_write_stream::compound value_write_stream::write_compound() {
            written_type_id = enbt::type_id(enbt::type::compound, enbt::type_len::Long);
            return compound(write_stream, need_to_write_type_id);
        }

        value_write_stream::darray value_write_stream::write_darray() {
            written_type_id = enbt::type_id(enbt::type::darray, enbt::type_len::Long);
            return darray(write_stream, need_to_write_type_id);
        }

        value_write_stream::array value_write_stream::write_array(size_t size) {
            written_type_id = enbt::type_id(enbt::type::array, enbt::calc_type_len(size));
            return array(write_stream, size, need_to_write_type_id);
        }

        value_write_stream::value_write_stream(std::ostream& write_stream, bool need_to_write_type_id)
            : write_stream(write_stream), need_to_write_type_id(need_to_write_type_id) {
        }
    }
}

namespace senbt {
    enbt::value parse_(std::string_view& string);

    std::string_view consume(std::string_view& string) {
        auto pos = string.find_first_of(" \t\n\r,})]");
        if (pos == std::string_view::npos) {
            auto res = string;
            string = {};
            return res;
        } else {
            auto res = string.substr(0, pos);
            string = string.substr(pos);
            return res;
        }
    }

    void skip_empty(std::string_view& string) {
        while (true) {
            auto pos = string.find_first_not_of(" \t\r\b\n");
            if (pos != std::string_view::npos)
                string = string.substr(pos);
            if (string.starts_with("//")) {
                auto pos = string.find_first_of('\n');
                if (pos != std::string_view::npos)
                    string = string.substr(pos);
                continue;
            }
            if (string.starts_with("/*")) {
                auto pos = string.find("*/");
                if (pos != std::string_view::npos)
                    string = string.substr(pos + 2);
                continue;
            }
            break;
        }
    }

    uint64_t parse_numeric_part(std::string_view string) {
        if (string.starts_with("0x") | string.starts_with("0X"))
            return std::stoull(std::string(string), nullptr, 16);
        else if (string.starts_with("0b") | string.starts_with("0B"))
            return std::stoull(std::string(string), nullptr, 2);
        else if (string.starts_with("0o") | string.starts_with("0O"))
            return std::stoull(std::string(string), nullptr, 8);
        else
            return std::stoull(std::string(string), nullptr, 10);
    }

    enbt::value parse_numeric(std::string_view string) {
        bool is_negative = string.starts_with('-');
        bool is_floating = false;
        bool is_var = false;
        bool is_comp = false;
        enbt::type_len len = enbt::type_len::Long;

        if (string.starts_with('-') | string.starts_with('+'))
            string = string.substr(1);

        if (string.ends_with('f') | string.ends_with('F')) {
            is_floating = true;
            len = enbt::type_len::Default;
            string = string.substr(0, string.size() - 1);
        } else if (string.ends_with('d') | string.ends_with('D')) {
            is_floating = true;
            len = enbt::type_len::Long;
            string = string.substr(0, string.size() - 1);
        } else if (string.ends_with('v')) {
            is_var = true;
            len = enbt::type_len::Default;
            string = string.substr(0, string.size() - 1);
        } else if (string.ends_with('V')) {
            is_var = true;
            len = enbt::type_len::Long;
            string = string.substr(0, string.size() - 1);
        } else if (string.ends_with('c')) {
            is_comp = true;
            len = enbt::type_len::Default;
            string = string.substr(0, string.size() - 1);
        } else if (string.ends_with('C')) {
            is_comp = true;
            len = enbt::type_len::Long;
            string = string.substr(0, string.size() - 1);
        } else if (string.ends_with('i')) {
            len = enbt::type_len::Default;
            string = string.substr(0, string.size() - 1);
        } else if (string.ends_with('I') | string.ends_with('l') | string.ends_with('L')) {
            len = enbt::type_len::Long;
            string = string.substr(0, string.size() - 1);
        } else if (string.ends_with('s') | string.ends_with('S')) {
            len = enbt::type_len::Short;
            string = string.substr(0, string.size() - 1);
        } else if (string.ends_with('b') | string.ends_with('B')) {
            len = enbt::type_len::Tiny;
            string = string.substr(0, string.size() - 1);
        }

        auto pos = string.find('.');
        if (pos != std::string_view::npos) {
            is_floating = true;
            if (is_var | is_comp)
                throw std::invalid_argument("floating point numbers cannot be var or comp integers");
        }

        if (is_floating) {
            auto first_part = parse_numeric_part(string.substr(0, pos));
            auto second_part = parse_numeric_part(string.substr(pos + 1));

            double res
                = is_negative ? -((double)first_part + (double)second_part / std::pow(10, string.size() - pos - 1))
                              : (double)first_part + (double)second_part / std::pow(10, string.size() - pos - 1);
            return len == enbt::type_len::Long ? enbt::value(res) : enbt::value((float)res);
        } else {
            enbt::value val = is_negative ? enbt::value(-int64_t(parse_numeric_part(string))) : enbt::value(parse_numeric_part(string));
            if (is_var)
                return val.cast_to(enbt::type_id(enbt::type::var_integer, len));
            else if (is_comp)
                return val.cast_to(enbt::type_id(enbt::type::comp_integer, len));
            else
                return val.cast_to(enbt::type_id(enbt::type::integer, len));
        }
    }

    std::string parse_string(std::string_view& string) {
        char quote = string[0];
        string = string.substr(1);
        std::string res;
        while (!string.empty()) {
            if (string.starts_with('\\')) {
                string = string.substr(1);
                switch (string[0]) {
                case '"':
                case '\'':
                case '\\':
                    res.push_back(string[0]);
                    break;
                case 'n':
                    res.push_back('\n');
                    break;
                case 't':
                    res.push_back('\t');
                    break;
                case 'r':
                    res.push_back('\r');
                    break;
                case 'b':
                    res.push_back('\b');
                    break;
                case 'f':
                    res.push_back('\f');
                    break;
                default:
                    throw std::invalid_argument("Unsupported escape sequence");
                }
            } else if (string.starts_with(quote)) {
                string = string.substr(1);
                return res;
            } else {
                res.push_back(string[0]);
                string = string.substr(1);
            }
        }
        throw std::invalid_argument("unterminated string");
    }

    enbt::value parse_compound(std::string_view& string) {
        string = string.substr(1);
        if (string.empty())
            throw std::invalid_argument("expected '}'");
        enbt::compound result;
        while (true) {
            skip_empty(string);
            if (string.starts_with('}')) {
                string = string.substr(1);
                break;
            }
            auto key = parse_string(string);
            skip_empty(string);
            if (string.empty())
                throw std::invalid_argument("expected ':'");
            if (string[0] != ':')
                throw std::invalid_argument("expected ':'");
            string = string.substr(1);
            result[key] = parse_(string);
            skip_empty(string);
            if (string.empty())
                throw std::invalid_argument("expected ',' or '}'");
            if (string[0] == ',')
                string = string.substr(1);
            else if (string[0] == '}') {
                string = string.substr(1);
                break;
            } else
                throw std::invalid_argument("expected ',' or '}'");
        }
        return result;
    }

    enbt::dynamic_array parse_darray(std::string_view& string) {
        string = string.substr(1);
        if (string.empty())
            throw std::invalid_argument("expected ']'");
        enbt::dynamic_array result;
        while (true) {
            skip_empty(string);
            if (string.starts_with(']')) {
                string = string.substr(1);
                break;
            }
            result.push_back(parse_(string));
            skip_empty(string);
            if (string.empty())
                throw std::invalid_argument("expected ',' or ']'");
            if (string[0] == ',')
                string = string.substr(1);
            else if (string[0] == ']') {
                string = string.substr(1);
                break;
            } else
                throw std::invalid_argument("expected ',' or ']'");
        }
        return result;
    }

    enbt::value parse_array(std::string_view& string) {
        string = string.substr(1);
        if (string.empty())
            throw std::invalid_argument("expected '['");
        if (string[0] != '[')
            throw std::invalid_argument("expected '['");
        string = string.substr(1);
        if (string.empty())
            throw std::invalid_argument("expected ']'");
        enbt::fixed_array result;
        while (true) {
            skip_empty(string);
            if (string.starts_with(']')) {
                string = string.substr(1);
                break;
            }
            result.push_back(parse_(string));
            skip_empty(string);
            if (string.empty())
                throw std::invalid_argument("expected ',' or ']'");
            if (string[0] == ',')
                string = string.substr(1);
            else if (string[0] == ']') {
                string = string.substr(1);
                break;
            } else
                throw std::invalid_argument("expected ',' or ']'");
        }
        return result;
    }

    // clang-format off
    template <char ty, bool sign>
    using parse_numeric_t = 
        std::conditional_t<sign,
            std::conditional_t<ty == 'b', std::int8_t,
            std::conditional_t<ty == 's', std::int16_t,
            std::conditional_t<ty == 'i', std::int32_t,
            std::conditional_t<ty == 'l', std::int64_t,
        void>>>>,
            std::conditional_t<ty == 'b', std::uint8_t,
            std::conditional_t<ty == 's', std::uint16_t,
            std::conditional_t<ty == 'i', std::uint32_t,
            std::conditional_t<ty == 'l', std::uint64_t,
        void>>>>>;

    // clang-format on

    template <char ty, bool sign>
    auto parse_sarray_typed(std::string_view& string) {
        string = string.substr(1);
        std::vector<parse_numeric_t<ty, sign>> result;
        while (true) {
            skip_empty(string);
            if (string.starts_with(']')) {
                string = string.substr(1);
                break;
            }
            result.push_back(parse_numeric(string));
            skip_empty(string);
            if (string.empty())
                throw std::invalid_argument("expected ',' or ']'");
            if (string[0] == ',')
                string = string.substr(1);
            else if (string[0] == ']') {
                string = string.substr(1);
                break;
            } else
                throw std::invalid_argument("expected ',' or ']'");
        }
        return enbt::value(result.data(), result.size());
    }

    enbt::value parse_sarray(std::string_view& string) {
        string = string.substr(1);
        if (string.empty())
            throw std::invalid_argument("expected simple array definition");
        bool is_unsigned = false;
        if (string[0] == 'u' | string[0] == 'U') {
            is_unsigned = true;
            string = string.substr(1);
        }
        if (string.empty())
            throw std::invalid_argument("expected simple array definition");

        switch (string[0]) {
        case 'b':
        case 'B':
            return is_unsigned ? parse_sarray_typed<'b', false>(string) : parse_sarray_typed<'b', true>(string);
        case 's':
        case 'S':
            return is_unsigned ? parse_sarray_typed<'s', false>(string) : parse_sarray_typed<'s', true>(string);
        case 'i':
        case 'I':
            return is_unsigned ? parse_sarray_typed<'i', false>(string) : parse_sarray_typed<'i', true>(string);
        case 'l':
        case 'L':
            return is_unsigned ? parse_sarray_typed<'l', false>(string) : parse_sarray_typed<'l', true>(string);
        default:
            throw std::invalid_argument("invalid simple array type");
        }
    }

    enbt::value parse_optional(std::string_view& string) {
        string = string.substr(1);
        if (string.empty())
            throw std::invalid_argument("expected '('");
        if (string[0] != '(')
            throw std::invalid_argument("expected '('");
        string = string.substr(1);
        skip_empty(string);
        if (string.empty())
            throw std::invalid_argument("expected ')'");
        if (string[0] == ')') {
            string = string.substr(1);
            return enbt::value(false, enbt::value());
        } else {
            auto result = parse_(string);
            skip_empty(string);
            if (string.empty())
                throw std::invalid_argument("expected ')'");
            if (string[0] != ')')
                throw std::invalid_argument("expected ')'");
            string = string.substr(1);
            return enbt::value(true, result);
        }
    }

    enbt::value parse_log_item(std::string_view& string) {
        string = string.substr(1);
        if (string.empty())
            throw std::invalid_argument("expected '('");
        auto result = parse_(string);
        skip_empty(string);
        if (string.empty())
            throw std::invalid_argument("expected ')'");
        if (string[0] != ')')
            throw std::invalid_argument("expected ')'");
        string = string.substr(1);
        return result;
    }

    enbt::value parse_uuid(std::string_view& string) {
        if (string.starts_with("uuid") | string.starts_with("UUID"))
            string = string.substr(4);
        else
            string = string.substr(1);
        if (string.empty())
            throw std::invalid_argument("expected '\"' or \"'\"");
        if (!string.starts_with('\'') && !string.starts_with('\"'))
            throw std::invalid_argument("expected '\"' or \"'\"");
        auto str = parse_string(string);
        if (str.size() != 36)
            throw std::invalid_argument("invalid uuid string");
        enbt::raw_uuid result;
        for (std::size_t i = 0, j = 0; i < 36; i++) {
            if (i == 8 | i == 13 | i == 18 | i == 23) {
                if (str[i] != '-')
                    throw std::invalid_argument("invalid uuid string");
            } else {
                if (str[i] >= '0' & str[i] <= '9')
                    result.data[j / 2] |= (str[i] - '0') << (j % 2 ? 0 : 4);
                else if (str[i] >= 'a' & str[i] <= 'f')
                    result.data[j / 2] |= (str[i] - 'a' + 10) << (j % 2 ? 0 : 4);
                else if (str[i] >= 'A' & str[i] <= 'F')
                    result.data[j / 2] |= (str[i] - 'A' + 10) << (j % 2 ? 0 : 4);
                else
                    throw std::invalid_argument("invalid uuid string");

                j++;
            }
        }
        return result;
    }

    enbt::value parse_true(std::string_view string) {
        if (string == "t" | string == "T")
            return enbt::value(true);
        else if (string == "true" | string == "TRUE")
            return enbt::value(true);
        else
            throw std::invalid_argument("invalid boolean value");
    }

    enbt::value parse_false(std::string_view string) {
        if (string == "f" | string == "F")
            return enbt::value(false);
        else if (string == "false" | string == "FALSE")
            return enbt::value(false);
        else
            throw std::invalid_argument("invalid boolean value");
    }

    enbt::value parse_none(std::string_view string) {
        if (string == "n" | string == "N")
            return enbt::value();
        else if (string == "none" | string == "NONE")
            return enbt::value();
        else if (string == "null" | string == "NULL")
            return enbt::value();
        else
            throw std::invalid_argument("invalid none value");
    }

    enbt::value parse_(std::string_view& string) {
        skip_empty(string);
        switch (string[0]) {
        case '{':
            return parse_compound(string);
        case '[':
            return parse_darray(string);
        case 'a':
            return parse_array(string);
        case 's':
            return parse_sarray(string);
        case '?':
            return parse_optional(string);
        case 't':
        case 'T':
            return parse_true(consume(string));
        case 'f':
        case 'F':
            return parse_false(consume(string));
        case '\'':
        case '\"':
            return parse_string(string);
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '.':
        case '-':
            return parse_numeric(consume(string));
        case 'n':
        case 'N':
            return parse_none(consume(string));
        case '(':
            return parse_log_item(string);
        case 'u':
        case 'U':
            return parse_uuid(string);
        default:
            throw std::invalid_argument("invalid value");
        }
    }

    enbt::value parse(std::string_view string) {
        return parse_(string);
    }

    enbt::value parse_mod(std::string_view& string) {
        return parse_(string);
    }

    std::string de_format(const std::string& string) {
        //replaces special symbols with \t \n \b \r \f \' \" and \\

        std::string res;
        res.reserve(string.size() * 1.2);
        for (std::size_t i = 0; i < string.size(); i++) {
            switch (string[i]) {
            case '\t':
                res += "\\t";
                break;
            case '\n':
                res += "\\n";
                break;
            case '\b':
                res += "\\b";
                break;
            case '\r':
                res += "\\r";
                break;
            case '\f':
                res += "\\f";
                break;
            case '\'':
                res += "\\'";
                break;
            case '\"':
                res += "\\\"";
                break;
            case '\\':
                res += "\\\\";
                break;
            default:
                res.push_back(string[i]);
            }
        }

        return res;
    }

    void serialize(std::string& res, const enbt::value& value, std::string& spaces, bool compress, bool type_erasure) {
        switch (value.get_type()) {
        case enbt::type::none:
            res += "null";
            break;
        case enbt::type::integer:
            if (type_erasure) {
                res += (value.get_type_sign() ? std::to_string((int64_t)value) : std::to_string((uint64_t)value));
                break;
            }
            switch (value.get_type_len()) {
            case enbt::type_len::Tiny:
                res += (value.get_type_sign() ? std::to_string((int8_t)value) : std::to_string((uint8_t)value)) + "b";
                break;
            case enbt::type_len::Short:
                res += (value.get_type_sign() ? std::to_string((int16_t)value) : std::to_string((uint16_t)value)) + "s";
                break;
            case enbt::type_len::Default:
                res += (value.get_type_sign() ? std::to_string((int32_t)value) : std::to_string((uint32_t)value)) + "i";
                break;
            case enbt::type_len::Long:
                res += (value.get_type_sign() ? std::to_string((int64_t)value) : std::to_string((uint64_t)value)) + "l";
                break;
            }
            break;
        case enbt::type::floating:
            if (type_erasure) {
                res += std::to_string((double)value);
                break;
            }
            switch (value.get_type_len()) {
            case enbt::type_len::Tiny:
            case enbt::type_len::Short:
                throw enbt::exception("not implemented");
            case enbt::type_len::Default:
                res += std::to_string((float)value) + "f";
                break;
            case enbt::type_len::Long:
                res += std::to_string((double)value) + "d";
                break;
            }
            break;
        case enbt::type::var_integer:
            if (type_erasure) {
                res += (value.get_type_sign() ? std::to_string((int64_t)value) : std::to_string((uint64_t)value));
                break;
            }
            switch (value.get_type_len()) {
            case enbt::type_len::Tiny:
            case enbt::type_len::Short:
                throw enbt::exception("not implemented");
            case enbt::type_len::Default:
                res += (value.get_type_sign() ? std::to_string((int32_t)value) : std::to_string((uint32_t)value)) + "v";
                break;
            case enbt::type_len::Long:
                res += (value.get_type_sign() ? std::to_string((int64_t)value) : std::to_string((uint64_t)value)) + "V";
                break;
            }
            break;
        case enbt::type::comp_integer:
            if (type_erasure) {
                res += (value.get_type_sign() ? std::to_string((int64_t)value) : std::to_string((uint64_t)value));
                break;
            }
            switch (value.get_type_len()) {
            case enbt::type_len::Tiny:
            case enbt::type_len::Short:
                throw enbt::exception("not implemented");
            case enbt::type_len::Default:
                res += (value.get_type_sign() ? std::to_string((int32_t)value) : std::to_string((uint32_t)value)) + "c";
                break;
            case enbt::type_len::Long:
                res += (value.get_type_sign() ? std::to_string((int64_t)value) : std::to_string((uint64_t)value)) + "C";
                break;
            }
            break;
        case enbt::type::uuid: {
            res += "uuid\"";
            enbt::raw_uuid tmp = value;
            for (std::size_t i = 0; i < 16; i++) {
                res += "0123456789abcdef"[tmp.data[i] >> 4];
                res += "0123456789abcdef"[tmp.data[i] & 0xF];
                if (i == 3 | i == 5 | i == 7 | i == 9)
                    res += '-';
            }
            res += '"';
            break;
        }
        case enbt::type::sarray: {
            res += "s";
            if (!value.get_type_sign())
                res += 'u';
            switch (value.get_type_len()) {
            case enbt::type_len::Tiny:
                res += "b";
                break;
            case enbt::type_len::Short:
                res += "s";
                break;
            case enbt::type_len::Default:
                res += "i";
                break;
            case enbt::type_len::Long:
                res += "l";
                break;
            }
            if (value.size()) {
                res += "[\n";
                if (!compress)
                    spaces.push_back('\t');
                if (value.get_type_sign()) {
                    switch (value.get_type_len()) {
                    case enbt::type_len::Tiny: {
                        auto data = enbt::simple_array_i8::make_ref(value);
                        for (std::size_t i = 0; i < data.size(); i++) {
                            res += spaces + std::to_string(data[i]);
                            if (i != data.size() - 1)
                                res += ',';
                            if (!compress)
                                res += '\n';
                        }
                        break;
                    }
                    case enbt::type_len::Short: {
                        auto data = enbt::simple_array_i16::make_ref(value);
                        for (std::size_t i = 0; i < data.size(); i++) {
                            res += spaces + std::to_string(data[i]);
                            if (i != data.size() - 1)
                                res += ',';
                            if (!compress)
                                res += '\n';
                        }
                        break;
                    }
                    case enbt::type_len::Default: {
                        auto data = enbt::simple_array_i32::make_ref(value);
                        for (std::size_t i = 0; i < data.size(); i++) {
                            res += spaces + std::to_string(data[i]);
                            if (i != data.size() - 1)
                                res += ',';
                            if (!compress)
                                res += '\n';
                        }
                        break;
                    }
                    case enbt::type_len::Long: {
                        auto data = enbt::simple_array_i64::make_ref(value);
                        for (std::size_t i = 0; i < data.size(); i++) {
                            res += spaces + std::to_string(data[i]);
                            if (i != data.size() - 1)
                                res += ',';
                            if (!compress)
                                res += '\n';
                        }
                        break;
                    }
                    }
                } else {
                    switch (value.get_type_len()) {
                    case enbt::type_len::Tiny: {
                        auto data = enbt::simple_array_ui8::make_ref(value);
                        for (std::size_t i = 0; i < data.size(); i++) {
                            res += spaces + std::to_string(data[i]);
                            if (i != data.size() - 1)
                                res += ',';
                            if (!compress)
                                res += '\n';
                        }
                        break;
                    }
                    case enbt::type_len::Short: {
                        auto data = enbt::simple_array_ui16::make_ref(value);
                        for (std::size_t i = 0; i < data.size(); i++) {
                            res += spaces + std::to_string(data[i]);
                            if (i != data.size() - 1)
                                res += ',';
                            if (!compress)
                                res += '\n';
                        }
                        break;
                    }
                    case enbt::type_len::Default: {
                        auto data = enbt::simple_array_ui32::make_ref(value);
                        for (std::size_t i = 0; i < data.size(); i++) {
                            res += spaces + std::to_string(data[i]);
                            if (i != data.size() - 1)
                                res += ',';
                            if (!compress)
                                res += '\n';
                        }
                        break;
                    }
                    case enbt::type_len::Long: {
                        auto data = enbt::simple_array_ui64::make_ref(value);
                        for (std::size_t i = 0; i < data.size(); i++) {
                            res += spaces + std::to_string(data[i]);
                            if (i != data.size() - 1)
                                res += ',';
                            if (!compress)
                                res += '\n';
                        }
                        break;
                    }
                    }
                }
                res.pop_back();
                if (!compress) {
                    spaces.pop_back();
                    res.pop_back();
                    res += '\n';
                }
                res += spaces + ']';
            } else
                res += "[]";
            break;
        }
        case enbt::type::compound: {
            auto compound = value.as_compound();
            if (compound.size()) {
                if (!compress)
                    spaces.push_back('\t');
                res += "{";
                for (auto&& [name, value] : compound) {
                    if (!compress)
                        res += '\n';
                    res += spaces + '"' + de_format(name) + "\": ";
                    serialize(res, value, spaces, compress, type_erasure);
                    res += ',';
                }
                res.pop_back();
                if (!compress) {
                    spaces.pop_back();
                    res += '\n';
                }

                res += spaces + "}";
            } else
                res += "{}";
            break;
        }
        case enbt::type::darray: {
            auto arr = enbt::dynamic_array::make_ref(value);
            if (!compress)
                spaces.push_back('\t');
            res += "[";
            for (auto&& value : arr) {
                if (!compress)
                    res += '\n';
                res += spaces;
                serialize(res, value, spaces, compress, type_erasure);
                res += ',';
            }
            if (!compress)
                spaces.pop_back();

            if (arr.size()) {
                res.pop_back();
                if (!compress)
                    res += '\n';
                res += spaces + "]";
            } else
                res += "]";
            break;
        }
        case enbt::type::array: {
            auto arr = enbt::dynamic_array::make_ref(value);
            if (!compress)
                spaces.push_back('\t');
            if (type_erasure)
                res += "[";
            else
                res += "a[";
            for (auto&& value : arr) {
                if (!compress)
                    res += '\n';
                res += spaces;
                serialize(res, value, spaces, compress, type_erasure);
                res += ',';
            }
            if (!compress)
                spaces.pop_back();

            if (arr.size()) {
                res.pop_back();
                if (!compress)
                    res += '\n';
                res += spaces + "]";
            } else
                res += ']';
            break;
        }
        case enbt::type::optional: {
            auto val = value.get_optional();
            if (!compress) {
                if (val) {
                    spaces.push_back('\t');
                    res += "?(\n" + spaces;
                    serialize(res, *val, spaces, compress, type_erasure);
                    spaces.pop_back();
                    res += '\n' + spaces + ")";
                } else
                    res += "?()";
            } else {
                if (val) {
                    res += "?(";
                    serialize(res, *val, spaces, compress, type_erasure);
                    res += ")";
                } else
                    res += "?()";
            }
            break;
        }
        case enbt::type::bit:
            res += value ? "true" : "false";
            break;
        case enbt::type::string:
            res += '"' + de_format((const std::string&)value) + '"';
            break;
        case enbt::type::log_item:
            if (!compress) {
                spaces.push_back('\t');
                res += "(\n" + spaces;
                serialize(res, value.get_log_value(), spaces, compress, type_erasure);
                spaces.pop_back();
                res += '\n' + spaces + ")";
            } else {
                res += "(";
                serialize(res, value.get_log_value(), spaces, compress, type_erasure);
                res += ")";
            }
            break;
        }
    }

    std::string serialize(const enbt::value& value, bool compress, bool type_erasure) {
        std::string res;
        std::string spaces;
        serialize(res, value, spaces, compress, type_erasure);
        return res;
    }
}
