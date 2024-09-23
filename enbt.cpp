#include "enbt.hpp"
#include <sstream>
#pragma region value constructors

namespace enbt {
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
        data_type_id.length = calc_len(array.size());
        data = (std::uint8_t*)new std::vector<value>(array);
    }

    value::value(const std::unordered_map<std::string, value>& compound) {
        data_type_id = enbt::type_id{type::compound, calc_len(compound.size()), false};
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
        data_type_id.length = calc_len(array.size());
        data = (std::uint8_t*)new std::vector<value>(std::move(array));
    }

    value::value(std::unordered_map<std::string, value>&& compound) {
        data_type_id = enbt::type_id{type::compound, calc_len(compound.size()), false};
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
        if (std::holds_alternative<nullptr_t>(val))
            return Target(0);

        else if (std::holds_alternative<bool>(val))
            return Target(std::get<bool>(val));

        else if (std::holds_alternative<std::uint8_t>(val))
            return (Target)std::get<std::uint8_t>(val);
        else if (std::holds_alternative<std::int8_t>(val))
            return (Target)std::get<std::int8_t>(val);

        else if (std::holds_alternative<std::uint16_t>(val))
            return (Target)std::get<std::uint16_t>(val);
        else if (std::holds_alternative<std::int16_t>(val))
            return (Target)std::get<std::int16_t>(val);

        else if (std::holds_alternative<std::uint32_t>(val))
            return (Target)std::get<std::uint32_t>(val);
        else if (std::holds_alternative<std::int32_t>(val))
            return (Target)std::get<std::int32_t>(val);

        else if (std::holds_alternative<std::uint64_t>(val))
            return (Target)std::get<std::uint64_t>(val);
        else if (std::holds_alternative<std::int64_t>(val))
            return (Target)std::get<std::int64_t>(val);

        else if (std::holds_alternative<float>(val))
            return (Target)std::get<float>(val);
        else if (std::holds_alternative<double>(val))
            return (Target)std::get<double>(val);
        else if (std::holds_alternative<std::string*>(val)) {
            if constexpr (std::is_unsigned_v<Target>)
                return std::stoull(*std::get<std::string*>(val));
            else if constexpr (std::is_floating_point_v<Target>)
                return std::stod(*std::get<std::string*>(val));
            else
                return std::stoll(*std::get<std::string*>(val));
        } else
            throw exception("Invalid type for convert");
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
            std::vector<char> res;
            res.resize(read);
            read_stream.read(res.data(), read);
            return std::string(res.data(), read);
        }

        value read_compound(std::istream& read_stream, enbt::type_id tid) {
            std::size_t len = read_define_len(read_stream, tid);
            std::unordered_map<std::string, value> result;
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

        bool move_to_value_path(std::istream& read_stream, const std::string& value_path) {
            try {
                for (auto&& tmp : split_s(value_path, "/")) {
                    auto tid = read_type_id(read_stream);
                    switch (tid.type) {
                    case enbt::type::array:
                    case enbt::type::darray:
                        index_array(read_stream, std::stoull(tmp), tid);
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

        value get_value_path(std::istream& read_stream, const std::string& value_path) {
            auto old_pos = read_stream.tellg();
            bool is_bit_value = false;
            bool bit_value;
            try {
                for (auto&& tmp : split_s(value_path, "/")) {
                    auto tid = read_type_id(read_stream);
                    switch (tid.type) {
                    case enbt::type::array: {
                        std::uint64_t len = read_define_len64(read_stream, tid);
                        auto target_id = read_type_id(read_stream);
                        std::uint64_t index = std::stoull(tmp);
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
    };
}
