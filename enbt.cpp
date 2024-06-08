#include "enbt.hpp"
#include <sstream>
#pragma region ENBT constructors

ENBT::ENBT() {
    data = nullptr;
    data_len = 0;
    data_type_id = Type_ID{Type::none, TypeLen::Tiny};
}

ENBT::ENBT(const std::string& str) {
    data_type_id.type = Type::string;
    data = (uint8_t*)new std::string(str);
}

ENBT::ENBT(std::string&& str) {
    data_type_id.type = Type::string;
    data = (uint8_t*)new std::string(std::move(str));
}

ENBT::ENBT(const char* str) {
    data_type_id.type = Type::string;
    data = (uint8_t*)new std::string(str);
}

ENBT::ENBT(const char* str, size_t len) {
    data_type_id.type = Type::string;
    data = (uint8_t*)new std::string(str, len);
}

ENBT::ENBT(const std::initializer_list<ENBT>& arr)
    : ENBT() {
    size_t len = arr.size();
    ENBT* struct_arr = new ENBT[len];
    size_t i = 0;
    for (const ENBT& it : arr)
        struct_arr[i++] = it;

    *this = ENBT(struct_arr, ENBT::Type_ID(ENBT::Type_ID::Type::structure, calcLen(len)), len);
}

ENBT::ENBT(const std::vector<ENBT>& array, Type_ID tid) {
    if (tid.type == Type::darray)
        ;
    else if (tid.type == Type::array) {
        if (array.size()) {
            Type_ID tid_check = array[0].type_id();
            for (auto& check : array)
                if (!check.type_equal(tid_check))
                    throw EnbtException("Invalid tid");
        }
    } else
        throw EnbtException("Invalid tid");
    checkLen(tid, array.size());
    data_type_id = tid;
    data = (uint8_t*)new std::vector<ENBT>(array);
}

ENBT::ENBT(const std::unordered_map<std::string, ENBT>& compound, TypeLen len_type) {
    data_type_id = Type_ID{Type::compound, len_type, false};
    checkLen(data_type_id, compound.size());
    data = (uint8_t*)new std::unordered_map<std::string, ENBT>(compound);
}

ENBT::ENBT(std::vector<ENBT>&& array) {
    data_len = array.size();
    data_type_id.type = Type::array;
    data_type_id.is_signed = 0;
    data_type_id.endian = Endian::native;
    if (data_len <= UINT8_MAX)
        data_type_id.length = TypeLen::Tiny;
    else if (data_len <= UINT16_MAX)
        data_type_id.length = TypeLen::Short;
    else if (data_len <= UINT32_MAX)
        data_type_id.length = TypeLen::Default;
    else
        data_type_id.length = TypeLen::Long;
    data = (uint8_t*)new std::vector<ENBT>(std::move(array));
}

ENBT::ENBT(std::vector<ENBT>&& array, Type_ID tid) {
    if (tid.type == Type::darray)
        ;
    else if (tid.type == Type::array) {
        if (array.size()) {
            Type_ID tid_check = array[0].type_id();
            for (auto& check : array)
                if (!check.type_equal(tid_check))
                    throw EnbtException("Invalid tid");
        }
    } else
        throw EnbtException("Invalid tid");
    checkLen(tid, array.size());
    data_type_id = tid;
    data = (uint8_t*)new std::vector<ENBT>(std::move(array));
}

ENBT::ENBT(std::unordered_map<std::string, ENBT>&& compound, TypeLen len_type) {
    data_type_id = Type_ID{Type::compound, len_type, false};
    checkLen(data_type_id, compound.size());
    data = (uint8_t*)new std::unordered_map<std::string, ENBT>(std::move(compound));
}

ENBT::ENBT(const uint8_t* utf8_str) {
    size_t size = len(utf8_str);
    data_type_id = Type_ID{Type::sarray, TypeLen::Tiny, false};
    uint8_t* str = new uint8_t[size];
    for (size_t i = 0; i < size; i++)
        str[i] = utf8_str[i];
    data = (uint8_t*)str;
    data_len = size;
}

ENBT::ENBT(const uint16_t* utf16_str, std::endian str_endian, bool convert_endian) {
    size_t size = len(utf16_str);
    data_type_id = Type_ID{Type::sarray, TypeLen::Short, str_endian, false};
    uint16_t* str = new uint16_t[size];
    for (size_t i = 0; i < size; i++)
        str[i] = utf16_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, size);

    data = (uint8_t*)str;
    data_len = size;
}

ENBT::ENBT(const uint32_t* utf32_str, std::endian str_endian, bool convert_endian) {
    size_t size = len(utf32_str);
    data_type_id = Type_ID{Type::sarray, TypeLen::Default, str_endian, false};
    uint32_t* str = new uint32_t[size];
    for (size_t i = 0; i < size; i++)
        str[i] = utf32_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, size);
    data = (uint8_t*)str;
    data_len = size;
}

ENBT::ENBT(const uint64_t* utf64_str, std::endian str_endian, bool convert_endian) {
    size_t size = len(utf64_str);
    data_type_id = Type_ID{Type::sarray, TypeLen::Default, str_endian, false};
    uint64_t* str = new uint64_t[size];
    for (size_t i = 0; i < size; i++)
        str[i] = utf64_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, size);
    data = (uint8_t*)str;
    data_len = size;
}

ENBT::ENBT(const uint8_t* utf8_str, size_t slen) {
    data_type_id = Type_ID{Type::sarray, TypeLen::Tiny, false};
    uint8_t* str = new uint8_t[slen];
    for (size_t i = 0; i < slen; i++)
        str[i] = utf8_str[i];
    data = (uint8_t*)str;
    data_len = slen;
}

ENBT::ENBT(const uint16_t* utf16_str, size_t slen, std::endian str_endian, bool convert_endian) {
    data_type_id = Type_ID{Type::sarray, TypeLen::Short, str_endian, false};
    uint16_t* str = new uint16_t[slen];
    for (size_t i = 0; i < slen; i++)
        str[i] = utf16_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, slen);


    data = (uint8_t*)str;
    data_len = slen;
}

ENBT::ENBT(const uint32_t* utf32_str, size_t slen, std::endian str_endian, bool convert_endian) {
    data_type_id = Type_ID{Type::sarray, TypeLen::Default, str_endian, false};
    uint32_t* str = new uint32_t[slen];
    for (size_t i = 0; i < slen; i++)
        str[i] = utf32_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, slen);
    data = (uint8_t*)str;
    data_len = slen;
}

ENBT::ENBT(const uint64_t* utf64_str, size_t slen, std::endian str_endian, bool convert_endian) {
    data_type_id = Type_ID{Type::sarray, TypeLen::Long, str_endian, false};
    uint64_t* str = new uint64_t[slen];
    for (size_t i = 0; i < slen; i++)
        str[i] = utf64_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, slen);
    data = (uint8_t*)str;
    data_len = slen;
}

ENBT::ENBT(const int8_t* utf8_str) {
    size_t size = len(utf8_str);
    data_type_id = Type_ID{Type::sarray, TypeLen::Tiny, true};
    int8_t* str = new int8_t[size];
    for (size_t i = 0; i < size; i++)
        str[i] = utf8_str[i];
    data = (uint8_t*)str;
    data_len = size;
}

ENBT::ENBT(const int16_t* utf16_str, std::endian str_endian, bool convert_endian) {
    size_t size = len(utf16_str);
    data_type_id = Type_ID{Type::sarray, TypeLen::Short, str_endian, true};
    int16_t* str = new int16_t[size];
    for (size_t i = 0; i < size; i++)
        str[i] = utf16_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, size);

    data = (uint8_t*)str;
    data_len = size;
}

ENBT::ENBT(const int32_t* utf32_str, std::endian str_endian, bool convert_endian) {
    size_t size = len(utf32_str);
    data_type_id = Type_ID{Type::sarray, TypeLen::Default, str_endian, true};
    int32_t* str = new int32_t[size];
    for (size_t i = 0; i < size; i++)
        str[i] = utf32_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, size);
    data = (uint8_t*)str;
    data_len = size;
}

ENBT::ENBT(const int64_t* utf64_str, std::endian str_endian, bool convert_endian) {
    size_t size = len(utf64_str);
    data_type_id = Type_ID{Type::sarray, TypeLen::Default, str_endian, true};
    int64_t* str = new int64_t[size];
    for (size_t i = 0; i < size; i++)
        str[i] = utf64_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, size);
    data = (uint8_t*)str;
    data_len = size;
}

ENBT::ENBT(const int8_t* utf8_str, size_t slen) {
    data_type_id = Type_ID{Type::sarray, TypeLen::Tiny, true};
    int8_t* str = new int8_t[slen];
    for (size_t i = 0; i < slen; i++)
        str[i] = utf8_str[i];
    data = (uint8_t*)str;
    data_len = slen;
}

ENBT::ENBT(const int16_t* utf16_str, size_t slen, std::endian str_endian, bool convert_endian) {
    data_type_id = Type_ID{Type::sarray, TypeLen::Short, str_endian, true};
    int16_t* str = new int16_t[slen];
    for (size_t i = 0; i < slen; i++)
        str[i] = utf16_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, slen);


    data = (uint8_t*)str;
    data_len = slen;
}

ENBT::ENBT(const int32_t* utf32_str, size_t slen, std::endian str_endian, bool convert_endian) {
    data_type_id = Type_ID{Type::sarray, TypeLen::Default, str_endian, true};
    int32_t* str = new int32_t[slen];
    for (size_t i = 0; i < slen; i++)
        str[i] = utf32_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, slen);
    data = (uint8_t*)str;
    data_len = slen;
}

ENBT::ENBT(const int64_t* utf64_str, size_t slen, std::endian str_endian, bool convert_endian) {
    data_type_id = Type_ID{Type::sarray, TypeLen::Long, str_endian, true};
    int64_t* str = new int64_t[slen];
    for (size_t i = 0; i < slen; i++)
        str[i] = utf64_str[i];
    if (convert_endian)
        ConvertEndianArr(str_endian, str, slen);
    data = (uint8_t*)str;
    data_len = slen;
}

ENBT::ENBT(UUID uuid, std::endian endian, bool convert_endian) {
    data_type_id = Type_ID{Type::uuid};
    SetData(uuid);
    if (convert_endian)
        ConvertEndian(endian, data, data_len);
}

ENBT::ENBT(bool byte) {
    data_type_id = Type_ID{Type::bit, TypeLen::Tiny, byte};
    data_len = 0;
}

ENBT::ENBT(int8_t byte) {
    SetData(byte);
    data_type_id = Type_ID{Type::integer, TypeLen::Tiny, true};
}

ENBT::ENBT(uint8_t byte) {
    SetData(byte);
    data_type_id = Type_ID{Type::integer, TypeLen::Tiny};
}

ENBT::ENBT(int16_t sh, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &sh, sizeof(int16_t));
    SetData(sh);
    data_type_id = Type_ID{Type::integer, TypeLen::Short, endian, true};
}

ENBT::ENBT(int32_t in, bool as_var, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &in, sizeof(int32_t));
    SetData(in);
    if (as_var)
        data_type_id = Type_ID{Type::var_integer, TypeLen::Default, endian, true};
    else
        data_type_id = Type_ID{Type::integer, TypeLen::Default, endian, true};
}

ENBT::ENBT(int64_t lon, bool as_var, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &lon, sizeof(int64_t));
    SetData(lon);
    if (as_var)
        data_type_id = Type_ID{Type::var_integer, TypeLen::Long, endian, true};
    else
        data_type_id = Type_ID{Type::integer, TypeLen::Long, endian, true};
}

ENBT::ENBT(int32_t in, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &in, sizeof(int32_t));
    SetData(in);
    data_type_id = Type_ID{Type::integer, TypeLen::Default, endian, true};
}

ENBT::ENBT(int64_t lon, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &lon, sizeof(int64_t));
    SetData(lon);
    data_type_id = Type_ID{Type::integer, TypeLen::Long, endian, true};
}

ENBT::ENBT(uint16_t sh, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &sh, sizeof(uint16_t));
    SetData(sh);
    data_type_id = Type_ID{Type::integer, TypeLen::Short, endian};
}

ENBT::ENBT(uint32_t in, bool as_var, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &in, sizeof(uint32_t));
    SetData(in);
    if (as_var)
        data_type_id = Type_ID{Type::var_integer, TypeLen::Default, endian, false};
    else
        data_type_id = Type_ID{Type::integer, TypeLen::Default, endian, false};
}

ENBT::ENBT(uint64_t lon, bool as_var, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &lon, sizeof(uint64_t));
    SetData(lon);
    if (as_var)
        data_type_id = Type_ID{Type::var_integer, TypeLen::Long, endian, false};
    else
        data_type_id = Type_ID{Type::integer, TypeLen::Long, endian, false};
}

ENBT::ENBT(uint32_t in, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &in, sizeof(uint32_t));
    SetData(in);
    data_type_id = Type_ID{Type::integer, TypeLen::Default, endian, false};
}

ENBT::ENBT(uint64_t lon, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &lon, sizeof(uint64_t));
    SetData(lon);
    data_type_id = Type_ID{Type::integer, TypeLen::Long, endian, false};
}

ENBT::ENBT(float flo, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &flo, sizeof(float));
    SetData(flo);
    data_type_id = Type_ID{Type::floating, TypeLen::Default, endian};
}

ENBT::ENBT(double dou, std::endian endian, bool convert_endian) {
    if (convert_endian)
        ConvertEndian(endian, &dou, sizeof(double));
    SetData(dou);
    data_type_id = Type_ID{Type::floating, TypeLen::Long, endian};
}

ENBT::ENBT(EnbtValue val, Type_ID tid, size_t length, bool convert_endian) {
    data_type_id = tid;
    data_len = 0;
    switch (tid.type) {
    case Type::integer:
        switch (data_type_id.length) {
        case TypeLen::Tiny:
            if (data_type_id.is_signed)
                SetData(std::get<int8_t>(val));
            else
                SetData(std::get<uint8_t>(val));
            break;
        case TypeLen::Short:
            if (data_type_id.is_signed)
                SetData(std::get<int16_t>(val));
            else
                SetData(std::get<uint16_t>(val));
            break;
        case TypeLen::Default:
            if (data_type_id.is_signed)
                SetData(std::get<int32_t>(val));
            else
                SetData(std::get<uint32_t>(val));
            break;
        case TypeLen::Long:
            if (data_type_id.is_signed)
                SetData(std::get<int64_t>(val));
            else
                SetData(std::get<uint64_t>(val));
        }
        break;
    case Type::floating:
        switch (data_type_id.length) {
        case TypeLen::Default:
            SetData(std::get<float>(val));
            break;
        case TypeLen::Long:
            SetData(std::get<double>(val));
        }
        break;
    case Type::var_integer:
        switch (data_type_id.length) {
        case TypeLen::Default:
            if (data_type_id.is_signed)
                SetData(std::get<int32_t>(val));
            else
                SetData(std::get<uint32_t>(val));
            break;
        case TypeLen::Long:
            if (data_type_id.is_signed)
                SetData(std::get<int64_t>(val));
            else
                SetData(std::get<uint64_t>(val));
        }
        break;
    case Type::uuid:
        SetData(std::get<uint8_t*>(val), 16);
        break;
    case Type::sarray: {
        switch (data_type_id.length) {
        case TypeLen::Tiny:
            SetData(std::get<uint8_t*>(val), length);
            break;
        case TypeLen::Short:
            SetData(std::get<uint16_t*>(val), length);
            break;
        case TypeLen::Default:
            SetData(std::get<uint32_t*>(val), length);
            break;
        case TypeLen::Long:
            SetData(std::get<uint64_t*>(val), length);
        }
        tid.is_signed = convert_endian;
        break;
    }
    case Type::array:
    case Type::darray:
        SetData(new std::vector<ENBT>(*std::get<std::vector<ENBT>*>(val)));
        break;
    case Type::compound:
        data = (uint8_t*)new std::unordered_map<std::string, ENBT>(*std::get<std::unordered_map<std::string, ENBT>*>(val));
        break;
    case Type::structure:
        SetData(CloneData((uint8_t*)std::get<ENBT*>(val), tid, length));
        data_len = length;
        break;
    case Type::bit:
        SetData(CloneData((uint8_t*)std::get<bool>(val), tid, length));
        data_len = length;
        break;
    case Type::optional:
        data = (uint8_t*)(std::holds_alternative<ENBT*>(val) ? std::get<ENBT*>(val) : nullptr);
        data_len = 0;
        break;
    case Type::string:
        data = (uint8_t*)new std::string(*std::get<std::string*>(val));
        break;
    case Type::log_item:
        data = (uint8_t*)std::get<ENBT*>(val);
        data_len = 0;
        break;
    default:
        data = nullptr;
        data_len = 0;
    }
}

ENBT::ENBT(ENBT* structureValues, size_t elems, TypeLen len_type) {
    data_type_id = Type_ID{Type::structure, len_type};
    if (!structureValues)
        throw EnbtException("structure is nullptr");
    if (!elems)
        throw EnbtException("structure canont be zero elements");
    checkLen(data_type_id, elems * 4);
    data = CloneData((uint8_t*)structureValues, data_type_id, elems);
    data_len = elems;
}

ENBT::ENBT(std::nullopt_t) {
    data_type_id = Type_ID{Type::optional};
    data_len = 0;
    data = nullptr;
}

ENBT::ENBT(Type_ID tid, size_t len) {
    switch (tid.type) {
    case Type::compound:
        operator=(ENBT(std::unordered_map<std::string, ENBT>(), tid.length));
        break;
    case Type::array:
    case Type::darray:
        operator=(ENBT(std::vector<ENBT>(len), tid));
        break;
    case Type::sarray:
        if (len) {
            switch (tid.length) {
            case TypeLen::Tiny:
                data = new uint8_t[len](0);
                break;
            case TypeLen::Short:
                data = (uint8_t*)new uint16_t[len](0);
                break;
            case TypeLen::Default:
                data = (uint8_t*)new uint32_t[len](0);
                break;
            case TypeLen::Long:
                data = (uint8_t*)new uint64_t[len](0);
                break;
            }
        }
        data_type_id = tid;
        data_len = len;

        break;
    case Type::optional:
        data_type_id = tid;
        data_type_id.is_signed = false;
        data_len = 0;
        data = nullptr;
        break;
    default:
        operator=(ENBT());
    }
}

ENBT::ENBT(Type typ, size_t len) {
    switch (len) {
    case 0:
        operator=(Type_ID(typ, TypeLen::Tiny, false));
        break;
    case 1:
        operator=(Type_ID(typ, TypeLen::Short, false));
        break;
    case 2:
        operator=(Type_ID(typ, TypeLen::Default, false));
        break;
    case 3:
        operator=(Type_ID(typ, TypeLen::Long, false));
        break;
    default:
        operator=(Type_ID(typ, TypeLen::Default, false));
    }
}

ENBT::ENBT(const ENBT& copy) {
    operator=(copy);
}

ENBT::ENBT(bool optional, ENBT&& value) {
    if (optional) {
        data_type_id = Type_ID(Type::optional, TypeLen::Tiny, true);
        data = (uint8_t*)new ENBT(std::move(value));
    } else {
        data_type_id = Type_ID(Type::optional, TypeLen::Tiny, false);
        data = nullptr;
    }
    data_len = 0;
}

ENBT::ENBT(bool optional, const ENBT& value) {
    if (optional) {
        data_type_id = Type_ID(Type::optional, TypeLen::Tiny, true);
        data = (uint8_t*)new ENBT(value);
    } else {
        data_type_id = Type_ID(Type::optional, TypeLen::Tiny, false);
        data = nullptr;
    }
    data_len = 0;
}

ENBT::ENBT(ENBT&& copy) noexcept {
    operator=(std::move(copy));
}

#pragma endregion


ENBT& ENBT::operator[](size_t index){
	if (is_array())
		return ((std::vector<ENBT>*)data)->operator[](index);
	if (data_type_id.type == Type_ID::Type::structure)
		return ((ENBT*)data)[index];
    throw std::invalid_argument("Invalid tid, cannot index array");
}

ENBT::EnbtValue ENBT::GetContent(uint8_t* data, size_t data_len, Type_ID data_type_id) {
    uint8_t* real_data = getData(data, data_type_id, data_len);
    switch (data_type_id.type) {
    case Type::integer:
    case Type::var_integer:
        switch (data_type_id.length) {
        case TypeLen::Tiny:
            if (data_type_id.is_signed)
                return (int8_t)real_data[0];
            else
                return (uint8_t)real_data[0];
        case TypeLen::Short:
            if (data_type_id.is_signed)
                return ((int16_t*)real_data)[0];
            else
                return ((uint16_t*)real_data)[0];
        case TypeLen::Default:
            if (data_type_id.is_signed)
                return ((int32_t*)real_data)[0];
            else
                return ((uint32_t*)real_data)[0];
        case TypeLen::Long:
            if (data_type_id.is_signed)
                return ((int64_t*)real_data)[0];
            else
                return ((uint64_t*)real_data)[0];
        default:
            return nullptr;
        }
    case Type::floating:
        switch (data_type_id.length) {
        case TypeLen::Default:
            return ((float*)real_data)[0];
        case TypeLen::Long:
            return ((double*)real_data)[0];
        default:
            return nullptr;
        }
    case Type::uuid:
        return *(UUID*)real_data;
    case Type::sarray:
        switch (data_type_id.length) {
        case TypeLen::Tiny:
            return data;
        case TypeLen::Short:
            return (uint16_t*)data;
        case TypeLen::Default:
            return (uint32_t*)data;
        case TypeLen::Long:
            return (uint64_t*)data;
        default:
            return data;
        }
        return real_data;
    case Type::array:
    case Type::darray:
        return ((std::vector<ENBT>*)data);
    case Type::compound:
        return ((std::unordered_map<std::string, ENBT>*)data);
    case Type::structure:
    case Type::optional:
        if (data)
            return (ENBT*)data;
        else
            return nullptr;
    case Type::bit:
        return (bool)data_type_id.is_signed;
    case Type::string:
        return (std::string*)data;
    case Type::log_item:
        return (ENBT*)data;
    default:
        return nullptr;
    }
}

uint8_t* ENBT::CloneData(uint8_t* data, Type_ID data_type_id, size_t data_len) {
    switch (data_type_id.type) {
    case Type::sarray: {
        switch (data_type_id.length) {
        case TypeLen::Tiny: {
            uint8_t* res = new uint8_t[data_len];
            for (size_t i = 0; i < data_len; i++)
                res[i] = data[i];
            return res;
        }
        case TypeLen::Short: {
            uint16_t* res = new uint16_t[data_len];
            uint16_t* proxy = (uint16_t*)data;
            for (size_t i = 0; i < data_len; i++)
                res[i] = proxy[i];
            return (uint8_t*)res;
        }
        case TypeLen::Default: {
            uint32_t* res = new uint32_t[data_len];
            uint32_t* proxy = (uint32_t*)data;
            for (size_t i = 0; i < data_len; i++)
                res[i] = proxy[i];
            return (uint8_t*)res;
        }
        case TypeLen::Long: {
            uint64_t* res = new uint64_t[data_len];
            uint64_t* proxy = (uint64_t*)data;
            for (size_t i = 0; i < data_len; i++)
                res[i] = proxy[i];
            return (uint8_t*)res;
        }
        default:
            break;
        }
    } break;
    case Type::array:
    case Type::darray:
        return (uint8_t*)new std::vector<ENBT>(*(std::vector<ENBT>*)data);
    case Type::compound:
        if (data_type_id.is_signed)
            return (uint8_t*)new std::unordered_map<uint16_t, ENBT>(*(std::unordered_map<uint16_t, ENBT>*)data);
        else
            return (uint8_t*)new std::unordered_map<std::string, ENBT>(*(std::unordered_map<std::string, ENBT>*)data);
    case Type::structure: {
        ENBT* cloned = new ENBT[data_len];
        ENBT* source = (ENBT*)data;
        for (size_t i = 0; i < data_len; i++)
            cloned[i] = source[i];
        return (uint8_t*)cloned;
    }

    case Type::optional: {
        if (data) {
            ENBT& source = *(ENBT*)data;
            return (uint8_t*)new ENBT(source);
        } else
            return nullptr;
    }
    case Type::uuid:
        return (uint8_t*)new UUID(*(UUID*)data);
    case Type::string:
        return (uint8_t*)new std::string(*(std::string*)data);
    case Type::log_item:
        return (uint8_t*)new ENBT(*(ENBT*)data);
    default:
        if (data_len > 8) {
            uint8_t* data_cloned = new uint8_t[data_len];
            for (size_t i = 0; i < data_len; i++)
                data_cloned[i] = data[i];
            return data_cloned;
        }
    }
    return data;
}

ENBT& ENBT::operator[](const char* index) {
    if (is_compound()) {
        return ((std::unordered_map<std::string, ENBT>*)data)->operator[](index);
    }
    throw std::invalid_argument("Invalid tid, cannot index compound");
}

void ENBT::remove(std::string name) {
    ((std::unordered_map<std::string, ENBT>*)data)->erase(name);
}

const ENBT& ENBT::operator[](size_t index) const {
    if (is_array())
        return ((std::vector<ENBT>*)data)->operator[](index);
    if (data_type_id.type == Type_ID::Type::structure)
        return ((ENBT*)data)[index];
    throw std::invalid_argument("Invalid tid, cannot index array");
}
const ENBT& ENBT::operator[](const char* index) const {
    if (is_compound()) {
        return ((std::unordered_map<std::string, ENBT>*)data)->operator[](index);
    }
    throw std::invalid_argument("Invalid tid, cannot index compound");
}

ENBT ENBT::getIndex(size_t index) const {
	if (is_sarray())
	{
		if (data_len <= index)
			throw std::out_of_range("SArray len is: " + std::to_string(data_len) + ", but try index at: " + std::to_string(index));
		if (data_type_id.is_signed) {
			switch (data_type_id.length)
			{
			case TypeLen::Tiny:
				return (int8_t)data[index];
			case TypeLen::Short:
				return ((int16_t*)data)[index];
			case TypeLen::Default:
				return ((int32_t*)data)[index];
			case TypeLen::Long:
				return ((int64_t*)data)[index];
			default:
				break;
			}
		}
		else {
			switch (data_type_id.length)
			{
			case TypeLen::Tiny:
				return data[index];
			case TypeLen::Short:
				return ((uint16_t*)data)[index];
			case TypeLen::Default:
				return ((uint32_t*)data)[index];
			case TypeLen::Long:
				return ((uint64_t*)data)[index];
			default:
				break;
			}
		}
		return ENBT();
	}
	else if (is_array())
		return ((std::vector<ENBT>*)data)->operator[](index);
	else if (data_type_id.type == Type_ID::Type::structure)
		return ((ENBT*)data)[index];
    else
        throw std::invalid_argument("Invalid tid, cannot index array");
}

void ENBT::freeze() {
    if (data_type_id.type == Type::darray) {
        bool first = true;
        Type_ID data_type_id;
        for (ENBT val : *((std::vector<ENBT>*)data)) {
            if (first) {
                data_type_id = val.data_type_id;
                first = false;
            } else if (val.data_type_id != data_type_id) {
                throw std::invalid_argument("This array has different types");
            }
        }
        data_len = ((std::vector<ENBT>*)data)->size();
        data_type_id.type = Type::array;
        if (data_len <= UINT8_MAX)
            data_type_id.length = TypeLen::Tiny;
        else if (data_len <= UINT16_MAX)
            data_type_id.length = TypeLen::Short;
        else if (data_len <= UINT32_MAX)
            data_type_id.length = TypeLen::Default;
        else
            data_type_id.length = TypeLen::Long;
    } else
        throw std::invalid_argument("Cannot freeze non-dynamic array");
}

void ENBT::unfreeze() {
    if (data_type_id.type == Type::array) {
        data_type_id.type = Type::darray;
        data_len = 0;
        data_type_id.length = TypeLen::Tiny;
    } else
        throw std::invalid_argument("Cannot unfreeze non-array");
}

template<class Target>
Target simpleIntConvert(const ENBT::EnbtValue& val) {

	if (std::holds_alternative<nullptr_t>(val))
		return Target(0);

	else if (std::holds_alternative<bool>(val))
		return Target(std::get<bool>(val));

	else if (std::holds_alternative<uint8_t>(val))
		return (Target)std::get<uint8_t>(val);
	else if (std::holds_alternative<int8_t>(val))
		return (Target)std::get<int8_t>(val);

	else if (std::holds_alternative<uint16_t>(val))
		return (Target)std::get<uint16_t>(val);
	else if (std::holds_alternative<int16_t>(val))
		return (Target)std::get<int16_t>(val);

	else if (std::holds_alternative<uint32_t>(val))
		return (Target)std::get<uint32_t>(val);
	else if (std::holds_alternative<int32_t>(val))
		return (Target)std::get<int32_t>(val);

	else if (std::holds_alternative<uint64_t>(val))
		return (Target)std::get<uint64_t>(val);
	else if (std::holds_alternative<int64_t>(val))
		return (Target)std::get<int64_t>(val);

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
        throw EnbtException("Invalid type for convert");
}

ENBT::operator bool() const { return simpleIntConvert<bool>(content()); }
ENBT::operator int8_t() const { return simpleIntConvert<int8_t>(content()); }
ENBT::operator int16_t() const { return simpleIntConvert<int16_t>(content()); }
ENBT::operator int32_t() const { return simpleIntConvert<int32_t>(content()); }
ENBT::operator int64_t() const { return simpleIntConvert<int64_t>(content()); }
ENBT::operator uint8_t() const { return simpleIntConvert<uint8_t>(content()); }
ENBT::operator uint16_t() const { return simpleIntConvert<uint16_t>(content()); }
ENBT::operator uint32_t() const { return simpleIntConvert<uint32_t>(content()); }
ENBT::operator uint64_t() const { return simpleIntConvert<uint64_t>(content()); }
ENBT::operator float() const { return simpleIntConvert<float>(content()); }
ENBT::operator double() const { return simpleIntConvert<double>(content()); }

ENBT::operator std::string&() {
    return *std::get<std::string*>(content());
}

ENBT::operator std::string() const {
    return std::visit([](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>)
            return arg;
        else if constexpr( std::is_integral_v<T>)
            return std::to_string(arg);
        else if constexpr (std::is_same_v<T, uint8_t*>)
            return std::string((char*)arg);
        else
            return std::string();
    },
                      content());
}

ENBT::operator const uint8_t*() const {
    return std::get<uint8_t*>(content());
}

ENBT::operator const int8_t*() const {
    return (int8_t*)std::get<uint8_t*>(content());
}

ENBT::operator const char*() const {
    return (char*)std::get<uint8_t*>(content());
}

ENBT::operator const int16_t*() const {
    return (int16_t*)std::get<uint16_t*>(content());
}

ENBT::operator const uint16_t*() const {
    return std::get<uint16_t*>(content());
}

ENBT::operator const int32_t*() const {
    return (int32_t*)std::get<uint32_t*>(content());
}

ENBT::operator const uint32_t*() const {
    return std::get<uint32_t*>(content());
}

ENBT::operator const int64_t*() const {
    return (int64_t*)std::get<uint64_t*>(content());
}

ENBT::operator const uint64_t*() const {
    return std::get<uint64_t*>(content());
}

ENBT::operator std::unordered_map<std::string, ENBT>() const {
    if (is_compound())
        return *(std::unordered_map<std::string, ENBT>*)data;
    throw std::invalid_argument("Invalid tid, cannot convert compound");
}

ENBT::operator UUID() const {
    return std::get<UUID>(content());
}

ENBT::operator std::optional<ENBT>() const {
    if (data_type_id.type == Type::optional)
        if (data_type_id.is_signed)
            return std::optional<ENBT>(*(const ENBT*)data);
        else
            return std::nullopt;
    else
        return std::optional<ENBT>(*this);
}

std::pair<std::string, ENBT> ENBT::CopyInterator::operator*() const {
    switch (iterate_type.type) {
    case ENBT::Type::sarray:
        if (iterate_type.is_signed) {
            switch (iterate_type.length) {
            case ENBT::TypeLen::Tiny:
                return {"", ENBT(*(int8_t*)pointer)};
                break;
            case ENBT::TypeLen::Short:
                return {"", ENBT(*(int16_t*)pointer)};
                break;
            case ENBT::TypeLen::Default:
                return {"", ENBT(*(int32_t*)pointer)};
                break;
            case ENBT::TypeLen::Long:
                return {"", ENBT(*(int64_t*)pointer)};
                break;
            default:
                break;
            }
        } else {
            switch (iterate_type.length) {
            case ENBT::TypeLen::Tiny:
                return {"", ENBT(*(uint8_t*)pointer)};
                break;
            case ENBT::TypeLen::Short:
                return {"", ENBT(*(uint16_t*)pointer)};
                break;
            case ENBT::TypeLen::Default:
                return {"", ENBT(*(uint32_t*)pointer)};
                break;
            case ENBT::TypeLen::Long:
                return {"", ENBT(*(uint64_t*)pointer)};
                break;
            default:
                break;
            }
        }
        break;
    case ENBT::Type::array:
    case ENBT::Type::darray:
        return {"", ENBT(*(*(std::vector<ENBT>::iterator*)pointer))};
    case ENBT::Type::compound: {
        auto& tmp = (*(std::unordered_map<std::string, ENBT>::iterator*)pointer);
        return std::pair<std::string, ENBT>(
            tmp->first,
            ENBT(tmp->second)
        );
    }
    }
    throw EnbtException("Unreachable exception in non debug environment");
}

#pragma region enbt_custom_operators_copy

template <>
ENBT& ENBT::operator=(const enbt::compound& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::fixed_array& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::dynamic_array& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::simple_array_ui8& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::simple_array_ui16& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::simple_array_ui32& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::simple_array_ui64& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::simple_array_i8& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::simple_array_i16& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::simple_array_i32& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::simple_array_i64& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::bit& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::optional& tag) {
    return *this = (const ENBT&)tag;
}

template <>
ENBT& ENBT::operator=(const enbt::uuid& tag) {
    return *this = (const ENBT&)tag;
}

#pragma endregion
#pragma region enbt_custom_operators_move

template <>
ENBT& ENBT::operator=(enbt::compound&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::fixed_array&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::dynamic_array&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::simple_array_ui8&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::simple_array_ui16&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::simple_array_ui32&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::simple_array_ui64&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::simple_array_i8&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::simple_array_i16&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::simple_array_i32&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::simple_array_i64&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::bit&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::optional&& tag) {
    return *this = (ENBT&&)tag;
}

template <>
ENBT& ENBT::operator=(enbt::uuid&& tag) {
    return *this = (ENBT&&)tag;
}

#pragma endregion
