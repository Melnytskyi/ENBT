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

    value::value(const char* str, size_t len){
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
}