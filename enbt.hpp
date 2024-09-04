#pragma once
#ifndef SRC_LIBRARY_ENBT_ENBT
    #define SRC_LIBRARY_ENBT_ENBT
    #include <any>
    #include <bit>
    #include <boost/uuid/detail/md5.hpp>
    #include <exception>
    #include <initializer_list>
    #include <istream>
    #include <optional>
    #include <sstream>
    #include <string>
    #include <unordered_map>
    #include <variant>
    #define ENBT_VERSION_HEX 0x10
    #define ENBT_VERSION_STR 1.0

//enchanted named binary tag
class EnbtException : public std::exception {
public:
    EnbtException(std::string&& reason)
        : std::exception(reason.c_str()) {}

    EnbtException(const char* reason)
        : std::exception(reason) {}

    EnbtException()
        : std::exception("EnbtException") {}
};

namespace enbt {
    class compound;
    class fixed_array;
    class dynamic_array;
    template <class T>
    class simple_array;


    using simple_array_ui8 = simple_array<uint8_t>;
    using simple_array_ui16 = simple_array<uint16_t>;
    using simple_array_ui32 = simple_array<uint32_t>;
    using simple_array_ui64 = simple_array<uint64_t>;
    using simple_array_i8 = simple_array<int8_t>;
    using simple_array_i16 = simple_array<int16_t>;
    using simple_array_i32 = simple_array<int32_t>;
    using simple_array_i64 = simple_array<int64_t>;

    class bit;
    class optional;
    class uuid;
}

class ENBT {
    template <typename T, typename Enable = void>
    struct is_optional : std::false_type {};

    template <typename T>
    struct is_optional<std::optional<T>> : std::true_type {};

public:
    struct version {       //current 1.0, max 15.15
        uint8_t Major : 4; //0x01
        uint8_t Minor : 4; //0x00
                           //path not match as version cause file structure and types will always match with any path version,
                           //if added new type or changed structure minor will incremented, not increment if type removed or total new simple types count not reached 5(all types reserving, not added in new paths)
                           //if minor reached 16 or structure has significant changes major increment
    };

    typedef boost::uuids::uuid UUID;

    union Type_ID {
        enum class Endian : uint8_t {
            little,
            big,
            native = (unsigned char)std::endian::native
        };
        enum class Type : uint8_t {
            //[(len)] = [00XXXXXX], [01XXXXXX XXXXXXXX], etc... 00 - 1 byte,01 - 2 byte,02 - 4 byte,03 - 8 byte
            none, //[0byte]
            integer,
            floating,
            var_integer, //ony default and long length
            uuid,        //[16byte]
            sarray,      // [(len)]{nums} if signed, endian convert will be enabled(simple array)

            compound,
            //[len][... items]   ((named items list))
            //			item [(len)][chars] {type_id 1byte} (value_define_and_data)

            darray, //		 [len][... items]   ((unnamed items list))
            //				item {type_id 1byte} (value_define_and_data)

            array, //[len][type_id 1byte]{... items} /unnamed items array, if contain static value size reader can get one element without reading all elems[int,double,e.t.c..]
            //				item {value_define_and_data}

            structure, //[total types] [type_id ...] {type defies}
            //used to reduce same metadata for array
            //will be encoded as:
            //[len][typeid: structure][2][typeid1][typeid2]{[item1,item2], [item1,item2], [item1,item2]}

            optional, // 'any value'     (contain value if is_signed == true)  /example [optional,unsigned]
            //	 				 											  /example [optional,signed][string][(3)]{"Yay"}
            bit,      //[0byte] bit in is_signed flag from Type_id byte
            string,   //[(len)][chars]  /utf8 string
            log_item, //[(log entry size in bytes)] [log entry] used for faster log reading
        };
        enum class LenType : uint8_t { // array string, e.t.c. length always little endian
            Tiny,
            Short,
            Default,
            Long
        };

        struct {
            uint8_t is_signed : 1;
            Endian endian : 1;
            LenType length : 2;
            Type type : 4;
        };

        uint8_t raw;

        std::endian getEndian() const {
            if (endian == Endian::big)
                return std::endian::big;
            else
                return std::endian::little;
        }

        bool operator!=(Type_ID cmp) const {
            return !operator==(cmp);
        }

        bool operator==(Type_ID cmp) const {
            return type == cmp.type && length == cmp.length && endian == cmp.endian && is_signed == cmp.is_signed;
        }

        Type_ID(Type ty = Type::none, LenType lt = LenType::Tiny, Endian en = Endian::native, bool sign = false) {
            type = ty;
            length = lt;
            endian = en;
            is_signed = sign;
        }

        Type_ID(Type ty, LenType lt, std::endian en, bool sign = false) {
            type = ty;
            length = lt;
            endian = (Endian)en;
            is_signed = sign;
        }

        Type_ID(Type ty, LenType lt, bool sign) {
            type = ty;
            length = lt;
            endian = Endian::native;
            is_signed = sign;
        }
    };

    typedef Type_ID::Type Type;
    typedef Type_ID::LenType TypeLen;
    typedef Type_ID::Endian Endian;

    typedef std::variant<
        bool,
        uint8_t,
        int8_t,
        uint16_t,
        int16_t,
        uint32_t,
        int32_t,
        uint64_t,
        int64_t,
        float,
        double,
        uint8_t*,
        uint16_t*,
        uint32_t*,
        uint64_t*,
        std::string*,
        std::vector<ENBT>*,                     //source pointer
        std::unordered_map<std::string, ENBT>*, //source pointer,
        UUID,
        ENBT*,
        nullptr_t>
        EnbtValue;

    #pragma region EndianConvertHelper

    static void EndianSwap(void* value_ptr, size_t len) {
        char* tmp = new char[len];
        char* prox = (char*)value_ptr;
        int j = 0;
        for (int64_t i = len - 1; i >= 0; i--)
            tmp[i] = prox[j++];
        for (size_t i = 0; i < len; i++)
            prox[i] = tmp[i];
        delete[] tmp;
    }

    static void ConvertEndian(std::endian value_endian, void* value_ptr, size_t len) {
        if (std::endian::native != value_endian)
            EndianSwap(value_ptr, len);
    }

    template <class T>
    static T ConvertEndian(std::endian value_endian, T val) {
        if (std::endian::native != value_endian)
            EndianSwap(&val, sizeof(T));
        return val;
    }

    template <class T>
    static void ConvertEndianArr(std::endian value_endian, T* val, size_t size) {
        if (std::endian::native != value_endian)
            for (size_t i = 0; i < size; i++)
                EndianSwap(&val[i], sizeof(T));
    }

    template <class T>
    static void ConvertEndianArr(std::endian value_endian, std::vector<T>& val) {
        if (std::endian::native != value_endian)
            for (auto& it : val)
                EndianSwap(&it, sizeof(T));
    }

    #pragma endregion

    template <class T>
    static T fromVar(uint8_t* ch, size_t& len) {
        constexpr int max_offset = (sizeof(T) / 5 * 5 + ((sizeof(T) % 5) > 0)) * 8;
        T decodedInt = 0;
        T bitOffset = 0;
        char currentByte = 0;
        size_t i = 0;
        do {
            if (i >= len)
                throw EnbtException("VarInt is too big");
            if (bitOffset == max_offset)
                throw EnbtException("VarInt is too big");
            currentByte = ch[i++];
            decodedInt |= (currentByte & 0b01111111) << bitOffset;
            bitOffset += 7;
        } while ((currentByte & 0b10000000) != 0);
        len = i;
        return decodedInt;
    }

    template <class T>
    static size_t toVar(uint8_t* buf, size_t buf_len, T val) {
        size_t i = 0;
        do {
            if (i >= buf_len)
                throw EnbtException("VarInt is too big");
            buf[i] = (uint8_t)(val & 0b01111111);
            val >>= 7;
            if (val != 0)
                buf[i] |= 0b10000000;
            i++;
        } while (val != 0);
        return i;
    }

protected:
    static EnbtValue GetContent(uint8_t* data, size_t data_len, Type_ID data_type_id);
    static uint8_t* CloneData(uint8_t* data, Type_ID data_type_id, size_t data_len);

    uint8_t* CloneData() const {
        return CloneData(data, data_type_id, data_len);
    }

    static uint8_t* getData(uint8_t*& data, Type_ID data_type_id, size_t data_len) {
        if (NeedFree(data_type_id, data_len))
            return data;
        else
            return (uint8_t*)&data;
    }

    static bool NeedFree(Type_ID data_type_id, size_t data_len) {
        switch (data_type_id.type) {
        case Type::none:
        case Type::bit:
        case Type::integer:
        case Type::var_integer:
        case Type::floating:
            return false;
        default:
            return true;
        }
    }

    static void FreeData(uint8_t* data, Type_ID data_type_id, size_t data_len) {
        if (data == nullptr)
            return;
        switch (data_type_id.type) {
        case Type::none:
        case Type::bit:
        case Type::integer:
        case Type::var_integer:
        case Type::floating:
            break;
        case Type::array:
        case Type::darray:
            delete (std::vector<ENBT>*)data;
            break;
        case Type::compound:
            delete (std::unordered_map<std::string, ENBT>*)data;
            break;
        case Type::structure:
            delete[] (ENBT*)data;
            break;
        case Type::optional:
            if (data_type_id.is_signed)
                delete (ENBT*)data;
            break;
        case Type::uuid:
            delete (UUID*)data;
            break;
        case Type::string:
            delete (std::string*)data;
            break;
        case Type::log_item:
            delete (ENBT*)data;
            break;
        default:
            delete[] data;
        }
        data = nullptr;
    }

    //if data_len <= 8 contain value in ptr
    //if data_len > 8 contain ptr to bytes array
    //if typeid is darray contain ptr to std::vector<ENBT>
    //if typeid is array contain ptr to array_value struct
    //if typeid is structure contain ENBT array
    uint8_t* data = nullptr;
    size_t data_len;
    Type_ID data_type_id;

    template <class T>
    void SetData(T val) {
        data_len = sizeof(T);
        if (data_len <= 8 && data_type_id.type != Type::uuid) {
            data = nullptr;
            char* prox0 = (char*)&data;
            char* prox1 = (char*)&val;
            for (size_t i = 0; i < data_len; i++)
                prox0[i] = prox1[i];
        } else {
            FreeData(data, data_type_id, data_len);
            data = (uint8_t*)new T(val);
        }
    }

    template <class T>
    void SetData(T* val, size_t len) {
        data_len = len * sizeof(T);
        if (data_len <= 8) {
            char* prox0 = (char*)data;
            char* prox1 = (char*)val;
            for (size_t i = 0; i < data_len; i++)
                prox0[i] = prox1[i];
        } else {
            FreeData(data, data_type_id, data_len);
            T* tmp = new T[len / sizeof(T)];
            for (size_t i = 0; i < len; i++)
                tmp[i] = val[i];
            data = (uint8_t*)tmp;
        }
    }

    template <class T>
    static size_t len(T* val) {
        T* len_calc = val;
        size_t size = 1;
        while (*len_calc++)
            size++;
        return size;
    }

    void checkLen(Type_ID tid, size_t len) {
        switch (tid.length) {
        case TypeLen::Tiny:
            if (tid.is_signed) {
                if (len > INT8_MAX)
                    throw EnbtException("Invalid tid");
            } else {
                if (len > UINT8_MAX)
                    throw EnbtException("Invalid tid");
            }
            break;
        case TypeLen::Short:
            if (tid.is_signed) {
                if (len > INT16_MAX)
                    throw EnbtException("Invalid tid");
            } else {
                if (len > UINT16_MAX)
                    throw EnbtException("Invalid tid");
            }
            break;
        case TypeLen::Default:
            if (tid.is_signed) {
                if (len > INT32_MAX)
                    throw EnbtException("Invalid tid");
            } else {
                if (len > UINT32_MAX)
                    throw EnbtException("Invalid tid");
            }
            break;
        case TypeLen::Long:
            if (tid.is_signed) {
                if (len > INT64_MAX)
                    throw EnbtException("Invalid tid");
            } else {
                if (len > UINT64_MAX)
                    throw EnbtException("Invalid tid");
            }
            break;
        }
    }

    TypeLen calcLen(size_t len) {
        if (len > UINT32_MAX)
            return TypeLen::Long;
        else if (len > UINT16_MAX)
            return TypeLen::Default;
        else if (len > UINT8_MAX)
            return TypeLen::Short;
        else
            return TypeLen::Tiny;
    }

public:
    static ENBT optional() {
        return ENBT((ENBT*)nullptr, Type_ID(Type::optional, TypeLen::Tiny, false), 0, false);
    }

    static ENBT optional(const ENBT& val) {
        return ENBT(new ENBT(val), Type_ID(Type::optional, TypeLen::Tiny, true), 0, false);
    }

    static ENBT optional(ENBT&& val) {
        return ENBT(new ENBT(std::move(val)), Type_ID(Type::optional, TypeLen::Tiny, false), 0, false);
    }

    static ENBT compound() {
        return ENBT(std::unordered_map<std::string, ENBT>());
    }

    static ENBT compound(std::unordered_map<std::string, ENBT>&& val) {
        return ENBT(std::move(val));
    }


    static ENBT dynamic_array() {
        return ENBT(std::vector<ENBT>(), Type::darray);
    }

    static ENBT dynamic_array(const std::vector<ENBT>& arr) {
        return ENBT(arr, Type::darray);
    }

    static ENBT dynamic_array(std::vector<ENBT>&& arr) {
        return ENBT(std::move(arr), Type::darray);
    }

    static ENBT fixed_array(size_t size) {
        return ENBT(std::vector<ENBT>(size), Type::array);
    }

    static ENBT to_log_item(const ENBT& val) {
        return ENBT(new ENBT(val), Type_ID(Type::log_item, TypeLen::Tiny, true), 0, false);
    }

    static ENBT to_log_item(ENBT&& val) {
        return ENBT(new ENBT(std::move(val)), Type_ID(Type::log_item, TypeLen::Tiny, false), 0, false);
    }

    static ENBT from_log_item(const ENBT& val) {
        if (val.data_type_id.type != Type::log_item)
            throw EnbtException("Invalid type");
        return *(ENBT*)val.data;
    }

    static ENBT from_log_item(ENBT&& val) {
        if (val.data_type_id.type != Type::log_item)
            throw EnbtException("Invalid type");
        ENBT res = std::move(*(ENBT*)val.data);
        delete (ENBT*)val.data;
        val.data = nullptr;
        return res;
    }

    template <class T>
    ENBT(const std::vector<T>& array) {
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
        auto res = new std::vector<ENBT>();
        res->reserve(data_len);
        for (const auto& it : array)
            res->push_back(it);
        data = (uint8_t*)res;
    }

    template <class T = ENBT>
    ENBT(const std::vector<ENBT>& array) {
        bool as_array = true;
        Type_ID tid_check = array[0].type_id();
        for (auto& check : array)
            if (!check.type_equal(tid_check)) {
                as_array = false;
                break;
            }
        data_len = array.size();
        if (as_array)
            data_type_id.type = Type::array;
        else
            data_type_id.type = Type::darray;
        if (data_len <= UINT8_MAX)
            data_type_id.length = TypeLen::Tiny;
        else if (data_len <= UINT16_MAX)
            data_type_id.length = TypeLen::Short;
        else if (data_len <= UINT32_MAX)
            data_type_id.length = TypeLen::Default;
        else
            data_type_id.length = TypeLen::Long;
        data_type_id.is_signed = 0;
        data_type_id.endian = Endian::native;
        data = (uint8_t*)new std::vector<ENBT>(array);
    }

    ENBT();
    ENBT(const std::string& str);
    ENBT(std::string&& str);
    ENBT(const char* str);
    ENBT(const char* str, size_t len);
    ENBT(const std::initializer_list<ENBT>& arr);
    ENBT(const std::vector<ENBT>& array, Type_ID tid);
    ENBT(const std::unordered_map<std::string, ENBT>& compound, TypeLen len_type = TypeLen::Long);
    ENBT(std::vector<ENBT>&& array);
    ENBT(std::vector<ENBT>&& array, Type_ID tid);
    ENBT(std::unordered_map<std::string, ENBT>&& compound, TypeLen len_type = TypeLen::Long);
    ENBT(const uint8_t* utf8_str);
    ENBT(const uint16_t* utf16_str, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const uint32_t* utf32_str, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const uint64_t* utf64_str, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const uint8_t* utf8_str, size_t slen);
    ENBT(const uint16_t* utf16_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const uint32_t* utf32_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const uint64_t* utf64_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const int8_t* utf8_str);
    ENBT(const int16_t* utf16_str, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const int32_t* utf32_str, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const int64_t* utf64_str, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const int8_t* utf8_str, size_t slen);
    ENBT(const int16_t* utf16_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const int32_t* utf32_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(const int64_t* utf64_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true);
    ENBT(UUID uuid, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(bool byte);
    ENBT(int8_t byte);
    ENBT(uint8_t byte);
    ENBT(int16_t sh, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(int32_t in, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(int64_t lon, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(int32_t in, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(int64_t lon, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(uint16_t sh, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(uint32_t in, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(uint64_t lon, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(uint32_t in, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(uint64_t lon, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(float flo, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(double dou, std::endian endian = std::endian::native, bool convert_endian = false);
    ENBT(EnbtValue val, Type_ID tid, size_t length, bool convert_endian = true);
    ENBT(ENBT* structureValues, size_t elems, TypeLen len_type = TypeLen::Tiny);
    ENBT(std::nullopt_t);
    ENBT(Type_ID tid, size_t len = 0);
    ENBT(Type typ, size_t len = 0);
    ENBT(const ENBT& copy);
    ENBT(bool optional, ENBT&& value);
    ENBT(bool optional, const ENBT& value);
    ENBT(ENBT&& copy) noexcept;

    ~ENBT() {
        FreeData(data, data_type_id, data_len);
    }

    ENBT& operator=(const ENBT& copy) {
        data = copy.CloneData();
        data_len = copy.data_len;
        data_type_id = copy.data_type_id;
        return *this;
    }

    ENBT& operator=(ENBT&& move) noexcept {
        data = move.data;
        data_len = move.data_len;
        data_type_id = move.data_type_id;

        move.data_type_id = {};
        move.data = nullptr;
        return *this;
    }

    template <class Ty>
    ENBT& operator=(Ty&& set_value) {
        using T = std::remove_cvref_t<Ty>;
        if constexpr (std::is_same<T, uint8_t>().value || std::is_same<T, int8_t>().value || std::is_same<T, uint16_t>().value || std::is_same<T, int16_t>().value || std::is_same<T, uint32_t>().value || std::is_same<T, int32_t>().value || std::is_same<T, uint64_t>().value || std::is_same<T, int64_t>().value || std::is_same<T, float>().value || std::is_same<T, double>().value) {
            if (data_type_id.type == Type::integer || data_type_id.type == Type::var_integer) {
                switch (data_type_id.length) {
                case TypeLen::Tiny:
                    SetData((uint8_t)set_value);
                    break;
                case TypeLen::Short:
                    SetData((uint16_t)set_value);
                    break;
                case TypeLen::Default:
                    SetData((uint32_t)set_value);
                    break;
                case TypeLen::Long:
                    SetData((uint64_t)set_value);
                    break;
                default:
                    break;
                }
            } else if (data_type_id.type == Type::floating) {
                switch (data_type_id.length) {
                case TypeLen::Default:
                    SetData((float)set_value);
                    break;
                case TypeLen::Long:
                    SetData((double)set_value);
                    break;
                default:
                    break;
                }
            } else
                operator=(ENBT(set_value));
        } else if constexpr (std::is_same<T, UUID>().value)
            SetData(set_value);
        else if constexpr (std::is_same<T, EnbtValue>().value)
            operator=(ENBT(set_value, data_type_id, data_len, data_type_id.endian));
        else if constexpr (std::is_same<T, bool>().value) {
            if (data_type_id.type == Type::bit)
                data_type_id.is_signed = set_value;
            else
                operator=(ENBT(set_value));
        } else if constexpr (is_optional<std::remove_cvref_t<T>>::value) {
            if (set_value.has_value())
                operator=(optional(*set_value));
            else
                operator=(optional());
        } else
            operator=(ENBT(std::forward<Ty>(set_value)));

        return *this;
    }

    bool type_equal(Type_ID tid) const {
        return !(data_type_id != tid);
    }

    bool is_compound() const {
        return data_type_id.type == Type::compound;
    }

    bool is_tiny_compound() const {
        if (data_type_id.type != Type::compound)
            return false;
        return data_type_id.is_signed;
    }

    bool is_long_compound() const {
        if (data_type_id.type != Type::compound)
            return false;
        return !data_type_id.is_signed;
    }

    bool is_array() const {
        return data_type_id.type == Type::array || data_type_id.type == Type::darray;
    }

    bool is_sarray() const {
        return data_type_id.type == Type::sarray;
    }

    bool is_numeric() const {
        return data_type_id.type == Type::integer || data_type_id.type == Type::var_integer || data_type_id.type == Type::floating;
    }

    bool is_none() const {
        return data_type_id.type == Type::none;
    }

    ENBT& operator[](size_t index);

    ENBT& operator[](int index) {
        return operator[]((size_t)index);
    }

    ENBT& operator[](const char* index);
    ENBT getIndex(size_t simple_index) const;
    const ENBT& operator[](size_t index) const;

    const ENBT& operator[](int index) const {
        return operator[]((size_t)index);
    }

    const ENBT& operator[](const char* index) const;

    const ENBT& operator[](const std::string& index) const {
        return operator[](index.c_str());
    }

    ENBT& operator[](const std::string& index) {
        return operator[](index.c_str());
    }

    size_t size() const {
        switch (data_type_id.type) {
        case Type::sarray:
        case Type::structure:
            return data_len;
        case Type::compound:
            return (*(std::unordered_map<std::string, ENBT>*)data).size();
        case Type::darray:
        case Type::array:
            return (*(std::vector<ENBT>*)data).size();
        case Type::string:
            return (*(std::string*)data).size();
        default:
            throw EnbtException("This type can not be sized");
        }
    }

    Type_ID type_id() const {
        return data_type_id;
    }

    EnbtValue content() const {
        return GetContent(data, data_len, data_type_id);
    }

    void setOptional(const ENBT& value) {
        if (data_type_id.type == Type::optional) {
            data_type_id.is_signed = true;
            FreeData(data, data_type_id, data_len);
            data = (uint8_t*)new ENBT(value);
        }
    }

    void setOptional(ENBT&& value) {
        if (data_type_id.type == Type::optional) {
            data_type_id.is_signed = true;
            FreeData(data, data_type_id, data_len);
            data = (uint8_t*)new ENBT(std::move(value));
        }
    }

    void setOptional() {
        if (data_type_id.type == Type::optional) {
            FreeData(data, data_type_id, data_len);
            data_type_id.is_signed = false;
        }
    }

    const ENBT* getOptional() const {
        if (data_type_id.type == Type::optional)
            if (data_type_id.is_signed)
                return (ENBT*)data;
        return nullptr;
    }

    ENBT* getOptional() {
        if (data_type_id.type == Type::optional)
            if (data_type_id.is_signed)
                return (ENBT*)data;
        return nullptr;
    }

    bool contains() const {
        if (data_type_id.type == Type::optional)
            return data_type_id.is_signed;
        return data_type_id.type != Type::none;
    }

    bool contains(const char* index) const {
        if (is_compound())
            return ((std::unordered_map<std::string, ENBT>*)data)->contains(index);
        return false;
    }

    bool contains(const std::string& index) const {
        if (is_compound())
            return ((std::unordered_map<std::string, ENBT>*)data)->contains(index);
        return false;
    }

    Type getType() const {
        return data_type_id.type;
    }

    TypeLen getTypeLen() const {
        return data_type_id.length;
    }

    bool getTypeSign() const {
        return data_type_id.is_signed;
    }

    const uint8_t* getPtr() const {
        return data;
    }

    void remove(size_t index) {
        if (is_array())
            ((std::vector<ENBT>*)data)->erase(((std::vector<ENBT>*)data)->begin() + index);
        else
            throw EnbtException("Cannot remove item from non array type");
    }

    void remove(std::string name);

    size_t push(const ENBT& enbt) {
        if (is_array()) {
            if (data_type_id.type == Type::array) {
                if (data_len)
                    if (operator[](0).data_type_id != enbt.data_type_id)
                        throw EnbtException("Invalid type for pushing array");
            }
            ((std::vector<ENBT>*)data)->push_back(enbt);
            return data_len++;
        } else
            throw EnbtException("Cannot push to non array type");
    }

    size_t push(ENBT&& enbt) {
        if (is_array()) {
            if (data_type_id.type == Type::array) {
                if (data_len)
                    if (operator[](0).data_type_id != enbt.data_type_id)
                        throw EnbtException("Invalid type for pushing array");
            }
            ((std::vector<ENBT>*)data)->push_back(std::move(enbt));
            return data_len++;
        } else
            throw EnbtException("Cannot push to non array type");
    }

    ENBT& front() {
        if (is_array()) {
            if (!data_len)
                throw EnbtException("Array empty");
            return ((std::vector<ENBT>*)data)->front();
        } else
            throw EnbtException("Cannot get front item from non array type");
    }

    const ENBT& front() const {
        if (is_array()) {
            if (!data_len)
                throw EnbtException("Array empty");
            return ((std::vector<ENBT>*)data)->front();
        } else
            throw EnbtException("Cannot get front item from non array type");
    }

    void pop() {
        if (is_array()) {
            if (!data_len)
                throw EnbtException("Array empty");
            ((std::vector<ENBT>*)data)->pop_back();
        } else
            throw EnbtException("Cannot pop front item from non array type");
    }

    void resize(size_t siz) {
        if (is_array()) {
            ((std::vector<ENBT>*)data)->resize(siz);
            if (siz < UINT8_MAX)
                data_type_id.length = TypeLen::Tiny;
            else if (siz < UINT16_MAX)
                data_type_id.length = TypeLen::Short;
            else if (siz < UINT32_MAX)
                data_type_id.length = TypeLen::Default;
            else
                data_type_id.length = TypeLen::Long;
        } else if (is_sarray()) {
            switch (data_type_id.length) {
            case TypeLen::Tiny: {
                uint8_t* n = new uint8_t[siz];
                for (size_t i = 0; i < siz && i < data_len; i++)
                    n[i] = data[i];
                delete[] data;
                data_len = siz;
                data = n;
                break;
            }
            case TypeLen::Short: {
                uint16_t* n = new uint16_t[siz];
                uint16_t* prox = (uint16_t*)data;
                for (size_t i = 0; i < siz && i < data_len; i++)
                    n[i] = prox[i];
                delete[] data;
                data_len = siz;
                data = (uint8_t*)n;
                break;
            }
            case TypeLen::Default: {
                uint32_t* n = new uint32_t[siz];
                uint32_t* prox = (uint32_t*)data;
                for (size_t i = 0; i < siz && i < data_len; i++)
                    n[i] = prox[i];
                delete[] data;
                data_len = siz;
                data = (uint8_t*)n;
                break;
            }
            case TypeLen::Long: {
                uint64_t* n = new uint64_t[siz];
                uint64_t* prox = (uint64_t*)data;
                for (size_t i = 0; i < siz && i < data_len; i++)
                    n[i] = prox[i];
                delete[] data;
                data_len = siz;
                data = (uint8_t*)n;
                break;
            }
            default:
                break;
            }


        } else
            throw EnbtException("Cannot resize non array type");
    }

    void freeze();
    void unfreeze();

    bool operator==(const ENBT& enbt) const {
        if (enbt.data_type_id == data_type_id && data_len == enbt.data_len) {
            switch (data_type_id.type) {
            case Type::sarray:
                switch (data_type_id.length) {
                case TypeLen::Tiny: {
                    uint8_t* other = enbt.data;
                    for (size_t i = 0; i < data_len; i++)
                        if (data[i] != other[i])
                            return false;
                    break;
                }
                case TypeLen::Short: {
                    uint16_t* im = (uint16_t*)data;
                    uint16_t* other = (uint16_t*)enbt.data;
                    for (size_t i = 0; i < data_len; i++)
                        if (im[i] != other[i])
                            return false;
                    break;
                }
                case TypeLen::Default: {
                    uint32_t* im = (uint32_t*)data;
                    uint32_t* other = (uint32_t*)enbt.data;
                    for (size_t i = 0; i < data_len; i++)
                        if (im[i] != other[i])
                            return false;
                    break;
                }
                case TypeLen::Long: {
                    uint64_t* im = (uint64_t*)data;
                    uint64_t* other = (uint64_t*)enbt.data;
                    for (size_t i = 0; i < data_len; i++)
                        if (im[i] != other[i])
                            return false;
                    break;
                }
                }
                return true;
            case Type::structure:
                for (size_t i = 0; i < data_len; i++)
                    if (operator[](i) != enbt[i])
                        return false;
                return true;
            case Type::array:
            case Type::darray:
                return (*std::get<std::vector<ENBT>*>(content())) == (*std::get<std::vector<ENBT>*>(enbt.content()));
            case Type::compound:
                return (*std::get<std::unordered_map<std::string, ENBT>*>(content())) == (*std::get<std::unordered_map<std::string, ENBT>*>(enbt.content()));
            case Type::optional:
                if (data_type_id.is_signed)
                    return (*(ENBT*)data) == (*(ENBT*)enbt.data);
                return true;
            case Type::bit:
                return data_type_id.is_signed == enbt.data_type_id.is_signed;
            case Type::uuid:
                return std::get<UUID>(content()) == std::get<UUID>(enbt.content());
            case Type::none:
                return true;
            case Type::string:
                return *std::get<std::string*>(content()) == *std::get<std::string*>(enbt.content());
            default:
                return content() == enbt.content();
            }
        } else
            return false;
    }

    bool operator!=(const ENBT& enbt) const {
        return !operator==(enbt);
    }

    operator bool() const;
    operator int8_t() const;
    operator int16_t() const;
    operator int32_t() const;
    operator int64_t() const;
    operator uint8_t() const;
    operator uint16_t() const;
    operator uint32_t() const;
    operator uint64_t() const;
    operator float() const;
    operator double() const;

    operator std::string&();
    operator const std::string&() const;
    operator std::string() const;
    operator const uint8_t*() const;
    operator const int8_t*() const;
    operator const char*() const;
    operator const int16_t*() const;
    operator const uint16_t*() const;
    operator const int32_t*() const;
    operator const uint32_t*() const;
    operator const int64_t*() const;
    operator const uint64_t*() const;

    template <class T = ENBT>
    operator std::vector<ENBT>() const {
        return *std::get<std::vector<ENBT>*>(content());
    }

    template <class T>
    operator std::vector<T>() const {
        std::vector<T> res;
        res.reserve(size());
        std::vector<ENBT>& tmp = *std::get<std::vector<ENBT>*>(content());
        for (auto& temp : tmp)
            res.push_back(std::get<T>(temp.content()));
        return res;
    }

    operator std::unordered_map<std::string, ENBT>() const;
    operator UUID() const;
    operator std::optional<ENBT>() const;

    std::string convert_to_str() const {
        return operator std::string();
    }

    class ConstInterator {
    protected:
        ENBT::Type_ID iterate_type;
        void* pointer;

    public:
        ConstInterator(const ENBT& enbt, bool in_begin = true) {
            iterate_type = enbt.data_type_id;
            switch (enbt.data_type_id.type) {
            case ENBT::Type::none:
                pointer = nullptr;
                break;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                if (in_begin)
                    pointer = new std::vector<ENBT>::iterator(
                        (*(std::vector<ENBT>*)enbt.data).begin()
                    );
                else
                    pointer = new std::vector<ENBT>::iterator(
                        (*(std::vector<ENBT>*)enbt.data).end()
                    );
                break;
            case ENBT::Type::compound:
                if (in_begin)
                    pointer = new std::unordered_map<std::string, ENBT>::iterator(
                        (*(std::unordered_map<std::string, ENBT>*)enbt.data).begin()
                    );
                else
                    pointer = new std::unordered_map<std::string, ENBT>::iterator(
                        (*(std::unordered_map<std::string, ENBT>*)enbt.data).end()
                    );
                break;
            default:
                throw EnbtException("Invalid type");
            }
        }

        ConstInterator(ConstInterator&& interator) noexcept {
            iterate_type = interator.iterate_type;
            pointer = interator.pointer;
            interator.pointer = nullptr;
        }

        ConstInterator(const ConstInterator& interator) {
            iterate_type = interator.iterate_type;
            switch (iterate_type.type) {
            case ENBT::Type::none:
                pointer = nullptr;
                break;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                pointer = new std::vector<ENBT>::iterator(
                    (*(std::vector<ENBT>::iterator*)interator.pointer)
                );
                break;
            case ENBT::Type::compound:
                pointer = new std::unordered_map<std::string, ENBT>::iterator(
                    (*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer)
                );
                break;
            default:
                throw EnbtException("Unreachable exception in non debug environment");
            }
        }

        ConstInterator& operator++() {
            switch (iterate_type.type) {
            case ENBT::Type::none:
                break;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                (*(std::vector<ENBT>::iterator*)pointer)++;
                break;
            case ENBT::Type::compound:
                (*(std::unordered_map<std::string, ENBT>::iterator*)pointer)++;
                break;
            }
            return *this;
        }

        ConstInterator operator++(int) {
            ConstInterator temp = *this;
            operator++();
            return temp;
        }

        ConstInterator& operator--() {
            switch (iterate_type.type) {
            case ENBT::Type::none:
                break;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                (*(std::vector<ENBT>::iterator*)pointer)--;
                break;
            case ENBT::Type::compound:
                (*(std::unordered_map<std::string, ENBT>::iterator*)pointer)--;
                break;
            }
            return *this;
        }

        ConstInterator operator--(int) {
            ConstInterator temp = *this;
            operator--();
            return temp;
        }

        bool operator==(const ConstInterator& interator) const {
            switch (iterate_type.type) {
            case ENBT::Type::none:
                return false;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                return (*(std::vector<ENBT>::iterator*)pointer) ==
                       (*(std::vector<ENBT>::iterator*)pointer);
            case ENBT::Type::compound:
                return (*(std::unordered_map<std::string, ENBT>::iterator*)pointer) ==
                       (*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer);
            }
            return false;
        }

        bool operator!=(const ConstInterator& interator) const {
            switch (iterate_type.type) {
            case ENBT::Type::none:
                return true;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                return (*(std::vector<ENBT>::iterator*)pointer) !=
                       (*(std::vector<ENBT>::iterator*)pointer);
            case ENBT::Type::compound:
                return (*(std::unordered_map<std::string, ENBT>::iterator*)pointer) !=
                       (*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer);
                break;
            }
            return true;
        }

        std::pair<std::string, const ENBT&> operator*() const {
            switch (iterate_type.type) {
            case ENBT::Type::none:
                return {"", ENBT()};
            case ENBT::Type::array:
            case ENBT::Type::darray:
                return {"", *(*(std::vector<ENBT>::iterator*)pointer)};
            case ENBT::Type::compound: {
                auto& tmp = (*(std::unordered_map<std::string, ENBT>::iterator*)pointer);
                return std::pair<std::string, const ENBT&>(
                    tmp->first,
                    tmp->second
                );
            }
            }
            throw EnbtException("Unreachable exception in non debug environment");
        }
    };

    class Interator : public ConstInterator {

    public:
        Interator(ENBT& enbt, bool in_begin = true)
            : ConstInterator(enbt, in_begin){};
        Interator(Interator&& interator) noexcept
            : ConstInterator(interator){};

        Interator(const Interator& interator)
            : ConstInterator(interator) {}

        Interator& operator++() {
            ConstInterator::operator++();
            return *this;
        }

        Interator operator++(int) {
            Interator temp = *this;
            ConstInterator::operator++();
            return temp;
        }

        Interator& operator--() {
            ConstInterator::operator--();
            return *this;
        }

        Interator operator--(int) {
            Interator temp = *this;
            ConstInterator::operator--();
            return temp;
        }

        bool operator==(const Interator& interator) const {
            return ConstInterator::operator==(interator);
        }

        bool operator!=(const Interator& interator) const {
            return ConstInterator::operator!=(interator);
        }

        std::pair<std::string, ENBT&> operator*() {
            switch (iterate_type.type) {
            case ENBT::Type::none:
                throw EnbtException("Invalid type");
            case ENBT::Type::array:
            case ENBT::Type::darray:
                return {"", *(*(std::vector<ENBT>::iterator*)pointer)};
            case ENBT::Type::compound: {
                auto& tmp = (*(std::unordered_map<std::string, ENBT>::iterator*)pointer);
                return std::pair<std::string, ENBT&>(
                    tmp->first,
                    tmp->second
                );
            }
            }
            throw EnbtException("Unreachable exception in non debug environemnt");
        }
    };

    class CopyInterator {
    protected:
        ENBT::Type_ID iterate_type;
        void* pointer;

    public:
        CopyInterator(const ENBT& enbt, bool in_begin = true) {
            iterate_type = enbt.data_type_id;
            switch (enbt.data_type_id.type) {
            case ENBT::Type::sarray:
                if (in_begin)
                    pointer = const_cast<uint8_t*>(enbt.getPtr());
                else
                    pointer = const_cast<uint8_t*>(enbt.getPtr() + enbt.size());
                break;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                if (in_begin)
                    pointer = new std::vector<ENBT>::iterator(
                        (*(std::vector<ENBT>*)enbt.data).begin()
                    );
                else
                    pointer = new std::vector<ENBT>::iterator(
                        (*(std::vector<ENBT>*)enbt.data).end()
                    );
                break;
            case ENBT::Type::compound:
                if (in_begin)
                    pointer = new std::unordered_map<std::string, ENBT>::iterator(
                        (*(std::unordered_map<std::string, ENBT>*)enbt.data).begin()
                    );
                else
                    pointer = new std::unordered_map<std::string, ENBT>::iterator(
                        (*(std::unordered_map<std::string, ENBT>*)enbt.data).end()
                    );
                break;
            default:
                throw EnbtException("Invalid type");
            }
        }

        CopyInterator(CopyInterator&& interator) noexcept {
            iterate_type = interator.iterate_type;
            pointer = interator.pointer;
            interator.pointer = nullptr;
        }

        CopyInterator(const CopyInterator& interator) {
            iterate_type = interator.iterate_type;
            switch (iterate_type.type) {
            case ENBT::Type::sarray:
                pointer = interator.pointer;
                break;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                pointer = new std::vector<ENBT>::iterator(
                    (*(std::vector<ENBT>::iterator*)interator.pointer)
                );
                break;
            case ENBT::Type::compound:
                pointer = new std::unordered_map<std::string, ENBT>::iterator(
                    (*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer)
                );
                break;
            default:
                throw EnbtException("Unreachable exception in non debug environment");
            }
        }

        CopyInterator& operator++() {
            switch (iterate_type.type) {
            case ENBT::Type::sarray:
                switch (iterate_type.length) {
                case ENBT::TypeLen::Tiny:
                    pointer = ((uint8_t*)pointer) + 1;
                    break;
                case ENBT::TypeLen::Short:
                    pointer = ((uint16_t*)pointer) + 1;
                    break;
                case ENBT::TypeLen::Default:
                    pointer = ((uint32_t*)pointer) + 1;
                    break;
                case ENBT::TypeLen::Long:
                    pointer = ((uint64_t*)pointer) + 1;
                    break;
                default:
                    break;
                }
                break;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                (*(std::vector<ENBT>::iterator*)pointer)++;
                break;
            case ENBT::Type::compound:
                (*(std::unordered_map<std::string, ENBT>::iterator*)pointer)++;
                break;
            }
            return *this;
        }

        CopyInterator operator++(int) {
            CopyInterator temp = *this;
            operator++();
            return temp;
        }

        CopyInterator& operator--() {
            switch (iterate_type.type) {
            case ENBT::Type::sarray:
                switch (iterate_type.length) {
                case ENBT::TypeLen::Tiny:
                    pointer = ((uint8_t*)pointer) - 1;
                    break;
                case ENBT::TypeLen::Short:
                    pointer = ((uint16_t*)pointer) - 1;
                    break;
                case ENBT::TypeLen::Default:
                    pointer = ((uint32_t*)pointer) - 1;
                    break;
                case ENBT::TypeLen::Long:
                    pointer = ((uint64_t*)pointer) - 1;
                    break;
                default:
                    break;
                }
                break;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                (*(std::vector<ENBT>::iterator*)pointer)--;
                break;
            case ENBT::Type::compound:
                (*(std::unordered_map<std::string, ENBT>::iterator*)pointer)--;
                break;
            }
            return *this;
        }

        CopyInterator operator--(int) {
            CopyInterator temp = *this;
            operator--();
            return temp;
        }

        bool operator==(const CopyInterator& interator) const {
            if (interator.iterate_type != iterate_type)
                return false;
            switch (iterate_type.type) {
            case ENBT::Type::sarray:
                return pointer == interator.pointer;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                return (*(std::vector<ENBT>::iterator*)pointer) ==
                       (*(std::vector<ENBT>::iterator*)pointer);
            case ENBT::Type::compound:
                return (*(std::unordered_map<std::string, ENBT>::iterator*)pointer) ==
                       (*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer);
                break;
            }
            return false;
        }

        bool operator!=(const CopyInterator& interator) const {
            if (interator.iterate_type != iterate_type)
                return false;
            switch (iterate_type.type) {
            case ENBT::Type::sarray:
                return pointer == interator.pointer;
            case ENBT::Type::array:
            case ENBT::Type::darray:
                return (*(std::vector<ENBT>::iterator*)pointer) !=
                       (*(std::vector<ENBT>::iterator*)pointer);
            case ENBT::Type::compound:
                return (*(std::unordered_map<std::string, ENBT>::iterator*)pointer) !=
                       (*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer);
                break;
            }
            return false;
        }

        std::pair<std::string, ENBT> operator*() const;
    };

    ConstInterator begin() const {
        return ConstInterator(*this, true);
    }

    ConstInterator end() const {
        return ConstInterator(*this, false);
    }

    Interator begin() {
        Interator test(*this, true);
        return Interator(*this, true);
    }

    Interator end() {
        return Interator(*this, false);
    }

    CopyInterator cbegin() const {
        return CopyInterator(*this, true);
    }

    CopyInterator cend() const {
        return CopyInterator(*this, false);
    }
};

namespace enbt {
    class compound_ref {
    protected:
        std::unordered_map<std::string, ENBT>* proxy;

        compound_ref(ENBT& abstract) {
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(abstract.content());
        }

        compound_ref(const ENBT& abstract) {
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(abstract.content());
        }


        compound_ref() {
            proxy = nullptr;
        }

    public:
        static compound_ref make_ref(ENBT& enbt) {
            if (enbt.is_compound())
                return compound_ref(enbt);
            else
                throw EnbtException("ENBT is not a compound");
        }

        static compound_ref make_ref(const ENBT& enbt) {
            if (enbt.is_compound())
                return compound_ref(enbt);
            else
                throw EnbtException("ENBT is not a compound");
        }

        using hasher = std::unordered_map<std::string, ENBT>::hasher;
        using key_type = std::unordered_map<std::string, ENBT>::key_type;
        using mapped_type = std::unordered_map<std::string, ENBT>::mapped_type;
        using key_equal = std::unordered_map<std::string, ENBT>::key_equal;

        using value_type = std::unordered_map<std::string, ENBT>::value_type;
        using allocator_type = std::unordered_map<std::string, ENBT>::allocator_type;
        using size_type = std::unordered_map<std::string, ENBT>::size_type;
        using difference_type = std::unordered_map<std::string, ENBT>::difference_type;
        using pointer = std::unordered_map<std::string, ENBT>::pointer;
        using const_pointer = std::unordered_map<std::string, ENBT>::const_pointer;
        using reference = std::unordered_map<std::string, ENBT>::reference;
        using const_reference = std::unordered_map<std::string, ENBT>::const_reference;
        using iterator = std::unordered_map<std::string, ENBT>::iterator;
        using const_iterator = std::unordered_map<std::string, ENBT>::const_iterator;

        using local_iterator = std::unordered_map<std::string, ENBT>::local_iterator;
        using const_local_iterator = std::unordered_map<std::string, ENBT>::const_local_iterator;

        using insert_return_type = std::unordered_map<std::string, ENBT>::insert_return_type;

        using node_type = std::unordered_map<std::string, ENBT>::node_type;

        compound_ref(const compound_ref& tag) {
            proxy = tag.proxy;
        }

        compound_ref(compound_ref&& tag) {
            proxy = tag.proxy;
        }

        mapped_type& operator[](key_type&& key_val) {
            return proxy->operator[](std::move(key_val));
        }

        const mapped_type& operator[](key_type&& key_val) const {
            return proxy->operator[](std::move(key_val));
        }

        void swap(std::unordered_map<std::string, ENBT>& swap_value) {
            proxy->swap(swap_value);
        }

        template <class T>
        std::pair<iterator, bool> insert(T&& value) {
            return proxy->insert(std::forward<T>(value));
        }

        template <class T>
        iterator insert(const_iterator where, T&& value) {
            return proxy->insert(where, std::forward<T>(value));
        }

        std::pair<iterator, bool> insert(const value_type& value) {
            return proxy->insert(value);
        }

        std::pair<iterator, bool> insert(value_type&& value) {
            return proxy->emplace(_STD move(value));
        }

        decltype(auto) insert(const_iterator hint, const value_type& value) {
            return proxy->insert(hint, value);
        }

        decltype(auto) insert(const_iterator hint, value_type&& value) {
            return proxy->insert(hint, _STD move(value));
        }

        template <class Iterator>
        void insert(Iterator first, Iterator last) {
            proxy->insert(first, last);
        }

        void insert(std::initializer_list<value_type> list) {
            proxy->insert(list);
        }

        decltype(auto) insert(const_iterator hint, node_type&& node) {
            return proxy->insert(hint, std::move(node));
        }

        decltype(auto) insert(node_type&& node) {
            return proxy->insert(std::move(node));
        }

        template <class... _Ts>
        std::pair<iterator, bool> try_emplace(const key_type& key_value, _Ts&&... values) {
            return proxy->try_emplace(key_value, std::forward<_Ts>(values)...);
        }

        template <class... _Ts>
        std::pair<iterator, bool> try_emplace(key_type&& key_value, _Ts&&... values) {
            return proxy->try_emplace(std::move(key_value), std::forward<_Ts>(values)...);
        }

        template <class... _Ts>
        iterator try_emplace(const const_iterator hint, const key_type& key_value, _Ts&&... values) {
            return proxy->try_emplace(hint, key_value, std::forward<_Ts>(values)...);
        }

        template <class... _Ts>
        iterator try_emplace(const const_iterator hint, key_type&& key_value, _Ts&&... values) {
            return proxy->try_emplace(hint, std::move(key_value), std::forward<_Ts>(values)...);
        }

        template <class T>
        std::pair<iterator, bool> insert_or_assign(const key_type& key_value, T&& values) {
            return proxy->insert_or_assign(key_value, std::forward<T>(values));
        }

        template <class T>
        std::pair<iterator, bool> insert_or_assign(key_type&& key_value, T&& values) {
            return proxy->insert_or_assign(std::move(key_value), std::forward<T>(values));
        }

        template <class T>
        iterator insert_or_assign(const_iterator hint, const key_type& key_value, T&& values) {
            return proxy->insert_or_assign(hint, key_value, std::forward<T>(values));
        }

        template <class T>
        iterator insert_or_assign(const_iterator hint, key_type&& key_value, T&& values) {
            return proxy->insert_or_assign(hint, std::move(key_value), std::forward<T>(values));
        }

        hasher hash_function() const {
            return proxy->hash_function();
        }

        key_equal key_eq() const {
            return proxy->key_eq();
        }

        mapped_type& operator[](const key_type& key_value) {
            return proxy->operator[](key_value);
        }

        [[nodiscard]] mapped_type& at(const key_type& key_value) {
            return proxy->at(key_value);
        }

        [[nodiscard]] const mapped_type& at(const key_type& key_value) const {
            return proxy->at(key_value);
        }

        compound_ref& operator=(const compound_ref& tag) {
            proxy = tag.proxy;
            return *this;
        }

        compound_ref& operator=(compound_ref&& tag) {
            proxy = tag.proxy;
            return *this;
        }

        size_type bucket(const std::string& index) const noexcept {
            return proxy->bucket(index);
        }

        size_type bucket_count() const noexcept {
            return proxy->bucket_count();
        }

        size_type bucket_size(size_type bucket) const noexcept {
            return proxy->bucket_size(bucket);
        }

        void clear() noexcept {
            proxy->clear();
        }

        bool contains(const key_type& key_value) const {
            return proxy->contains(key_value);
        }

        size_type count(const key_type& key_value) const {
            return proxy->count(key_value);
        }

        template <class... Values>
        std::pair<iterator, bool> emplace(Values&&... vals) {
            return proxy->emplace(std::forward<Values>(vals)...);
        }

        template <class... Values>
        decltype(auto) emplace(const_iterator where, Values&&... vals) {
            return proxy->emplace_hint(where, std::forward<Values>(vals)...);
        }

        bool empty(const key_type& key_value) const noexcept {
            return proxy->empty();
        }

        std::pair<iterator, iterator> equal_range(const key_type& key_value) {
            return proxy->equal_range(key_value);
        }

        std::pair<const_iterator, const_iterator> equal_range(const key_type& key_value) const {
            return proxy->equal_range(key_value);
        }

        size_type erase(const key_type& key_value) {
            return proxy->erase(key_value);
        }

        node_type extract(const key_type& key_value) {
            return proxy->extract(key_value);
        }

        decltype(auto) find(const key_type& key_value) {
            return proxy->find(key_value);
        }

        decltype(auto) find(const key_type& key_value) const {
            return proxy->find(key_value);
        }

        float load_factor() const noexcept {
            return proxy->load_factor();
        }

        decltype(auto) lower_bound(const key_type& key_value) {
            return proxy->lower_bound(key_value);
        }

        size_type max_bucket_count() const noexcept {
            return proxy->max_bucket_count();
        }

        size_type max_load_factor() const noexcept {
            return proxy->max_load_factor();
        }

        size_type max_size() const noexcept {
            return proxy->max_size();
        }

        size_type size() const noexcept {
            return proxy->size();
        }

        iterator begin() {
            return proxy->begin();
        }

        iterator end() {
            return proxy->end();
        }

        const_iterator begin() const {
            return proxy->begin();
        }

        const_iterator end() const {
            return proxy->end();
        }

        const_iterator cbegin() const {
            return proxy->cbegin();
        }

        const_iterator cend() const {
            return proxy->cend();
        }
    };

    class fixed_array_ref {
    protected:
        ENBT::Type_ID fixed_type = ENBT::Type::none;
        std::vector<ENBT>* proxy;
        bool as_const = false;

        fixed_array_ref(ENBT& abstract) {
            proxy = std::get<std::vector<ENBT>*>(abstract.content());
            as_const = false;
        }

        fixed_array_ref(const ENBT& abstract) {
            proxy = std::get<std::vector<ENBT>*>(abstract.content());
            as_const = true;
        }

        fixed_array_ref() {
            proxy = nullptr;
        }

    public:
        static fixed_array_ref make_ref(ENBT& enbt) {
            if (enbt.getType() == ENBT::Type::array)
                return fixed_array_ref(enbt);
            else
                throw EnbtException("ENBT is not a fixed array");
        }

        static fixed_array_ref make_ref(const ENBT& enbt) {
            if (enbt.getType() == ENBT::Type::array)
                return fixed_array_ref(enbt);
            else
                throw EnbtException("ENBT is not a fixed array");
        }

        using value_type = ENBT;
        using allocator_type = std::vector<ENBT>::allocator_type;
        using pointer = std::vector<ENBT>::pointer;
        using const_pointer = std::vector<ENBT>::const_pointer;
        using reference = ENBT&;
        using const_reference = const ENBT&;
        using size_type = std::vector<ENBT>::size_type;
        using difference_type = std::vector<ENBT>::difference_type;
        using iterator = std::vector<ENBT>::iterator;
        using const_iterator = std::vector<ENBT>::const_iterator;
        using reverse_iterator = std::vector<ENBT>::reverse_iterator;
        using const_reverse_iterator = std::vector<ENBT>::const_reverse_iterator;

        fixed_array_ref(const fixed_array_ref& tag) {
            fixed_type = tag.fixed_type;
            proxy = tag.proxy;
        }

        fixed_array_ref(fixed_array_ref&& tag) {
            fixed_type = tag.fixed_type;
            proxy = tag.proxy;
        }

        fixed_array_ref& operator=(const fixed_array_ref& tag) {
            fixed_type = tag.fixed_type;
            proxy = tag.proxy;
            return *this;
        }

        fixed_array_ref& operator=(fixed_array_ref&& tag) {
            fixed_type = tag.fixed_type;
            proxy = tag.proxy;
            return *this;
        }

        template <class T>
        void set(size_t index, T&& value) {
            if (as_const)
                throw EnbtException("This array is constant");
            ENBT to_set(std::forward<T>(value));


            if (index != 0 || fixed_type.type != ENBT::Type::none) {
                if (to_set.type_id() != fixed_type)
                    throw EnbtException("Invalid set value, set value must be same as every item in fixed array");
            }
            fixed_type = to_set.type_id();
            (*proxy)[index] = std::move(to_set);
        }

        const ENBT& operator[](size_t index) const {
            return (*proxy)[index];
        }

        void remove(size_t index) {
            proxy->erase(proxy->begin() + index);
        }

        size_t size() const {
            return proxy->size();
        }

        const_iterator begin() const {
            return proxy->cbegin();
        }

        const_iterator end() const {
            return proxy->cend();
        }

        const_iterator cbegin() const {
            return proxy->cbegin();
        }

        const_iterator cend() const {
            return proxy->cend();
        }

        const_reverse_iterator rbegin() const {
            return proxy->crbegin();
        }

        const_reverse_iterator rend() const {
            return proxy->crend();
        }

        const_reverse_iterator crbegin() const {
            return proxy->crbegin();
        }

        const_reverse_iterator crend() const {
            return proxy->crend();
        }
    };

    class dynamic_array_ref {
    protected:
        std::vector<ENBT>* proxy;

        dynamic_array_ref(const ENBT& abstract) {
            proxy = std::get<std::vector<ENBT>*>(abstract.content());
        }

        dynamic_array_ref() {
            proxy = nullptr;
        }

    public:
        static dynamic_array_ref make_ref(ENBT& enbt) {
            if (enbt.getType() == ENBT::Type::array)
                return dynamic_array_ref(enbt);
            else
                throw EnbtException("ENBT is not a dynamic array");
        }

        static const dynamic_array_ref make_ref(const ENBT& enbt) {
            if (enbt.getType() == ENBT::Type::array)
                return dynamic_array_ref(enbt);
            else
                throw EnbtException("ENBT is not a dynamic array");
        }

        using value_type = ENBT;
        using allocator_type = std::vector<ENBT>::allocator_type;
        using pointer = std::vector<ENBT>::pointer;
        using const_pointer = std::vector<ENBT>::const_pointer;
        using reference = ENBT&;
        using const_reference = const ENBT&;
        using size_type = std::vector<ENBT>::size_type;
        using difference_type = std::vector<ENBT>::difference_type;
        using iterator = std::vector<ENBT>::iterator;
        using const_iterator = std::vector<ENBT>::const_iterator;
        using reverse_iterator = std::vector<ENBT>::reverse_iterator;
        using const_reverse_iterator = std::vector<ENBT>::const_reverse_iterator;

        dynamic_array_ref(const dynamic_array_ref& tag) {
            proxy = tag.proxy;
        }

        dynamic_array_ref(dynamic_array_ref&& tag) {
            proxy = tag.proxy;
        }

        dynamic_array_ref& operator=(const dynamic_array_ref& tag) {
            proxy = tag.proxy;
            return *this;
        }

        dynamic_array_ref& operator=(dynamic_array_ref&& tag) {
            proxy = tag.proxy;
            return *this;
        }

        template <class T>
        void push_back(T&& value) {
            ENBT to_set(std::forward<T>(value));
            proxy->push_back(std::move(to_set));
        }

        template <class... _Values>
        decltype(auto) emplace_back(_Values&&... values) {
            return proxy->emplace_back(std::forward<_Values>(values)...);
        }

        template <class... _Values>
        iterator emplace(const_iterator where, _Values&&... values) {
            return proxy->emplace(where, std::forward<_Values>(values)...);
        }

        iterator insert(const_iterator where, const ENBT& value) {
            return proxy->insert(where, value);
        }

        iterator insert(const_iterator where, ENBT&& value) {
            return proxy->insert(where, std::move(value));
        }

        iterator insert(const_iterator where, size_type count, const ENBT& value) {
            return proxy->insert(where, count, value);
        }

        void pop_back() {
            proxy->pop_back();
        }

        void resize(size_type siz) {
            proxy->resize(siz);
        }

        void resize(size_type siz, const ENBT& def_init) {
            proxy->resize(siz, def_init);
        }

        size_type size() const {
            return proxy->size();
        }

        bool empty() const {
            return proxy->empty();
        }

        void assign(size_type new_size, const ENBT& val) {
            proxy->assign(new_size, val);
        }

        void assign(std::initializer_list<ENBT> list) {
            proxy->assign(list);
        }

        void reserve(size_type new_capacity) {
            proxy->reserve(new_capacity);
        }

        void shrink_to_fit() {
            proxy->shrink_to_fit();
        }

        iterator erase(const_iterator where) {
            return proxy->erase(where);
        }

        iterator erase(const_iterator where_beg, const_iterator where_end) {
            return proxy->erase(where_beg, where_end);
        }

        void clear() {
            proxy->clear();
        }

        void swap(std::vector<ENBT>& another) {
            proxy->swap(another);
        }

        void swap(dynamic_array_ref& another) {
            proxy->swap(*another.proxy);
        }

        ENBT* data() {
            return proxy->data();
        }

        const ENBT* data() const {
            return proxy->data();
        }

        iterator begin() {
            return proxy->begin();
        }

        iterator end() {
            return proxy->end();
        }

        const_iterator begin() const {
            return proxy->cbegin();
        }

        const_iterator end() const {
            return proxy->cend();
        }

        const_iterator cbegin() const {
            return proxy->cbegin();
        }

        const_iterator cend() const {
            return proxy->cend();
        }

        reverse_iterator rbegin() {
            return proxy->rbegin();
        }

        reverse_iterator rend() {
            return proxy->rend();
        }

        const_reverse_iterator rbegin() const {
            return proxy->crbegin();
        }

        const_reverse_iterator rend() const {
            return proxy->crend();
        }

        const_reverse_iterator crbegin() const {
            return proxy->crbegin();
        }

        const_reverse_iterator crend() const {
            return proxy->crend();
        }

        size_t max_size() const noexcept {
            return proxy->max_size();
        }

        [[nodiscard]] ENBT& operator[](size_type index) noexcept {
            return proxy->operator[](index);
        }

        [[nodiscard]] const ENBT& operator[](size_type index) const noexcept {
            return proxy->operator[](index);
        }

        [[nodiscard]] ENBT& at(size_type index) {
            return proxy->at(index);
        }

        [[nodiscard]] const ENBT& at(size_type index) const {
            return proxy->at(index);
        }

        const ENBT& front() const {
            return proxy->front();
        }

        ENBT& front() {
            return proxy->front();
        }

        const ENBT& back() const {
            return proxy->back();
        }

        ENBT& back() {
            return proxy->back();
        }
    };

    template <class T>
    class simple_array_const_ref {
    protected:
        T* proxy;
        size_t size_;

        static constexpr ENBT::TypeLen len_type = []() constexpr {
            if (sizeof(T) < UINT8_MAX)
                return ENBT::TypeLen::Tiny;
            else if (sizeof(T) < UINT16_MAX)
                return ENBT::TypeLen::Short;
            else if (sizeof(T) < UINT32_MAX)
                return ENBT::TypeLen::Default;
            else
                return ENBT::TypeLen::Long;
        }();

        simple_array_const_ref(const ENBT& abstract) {
            proxy = std::get<T*>(abstract.content());
            size_ = abstract.size();
        }

    public:
        static simple_array_const_ref make_ref(const ENBT& enbt) {
            auto ty = enbt.type_id();
            if (ty.type == ENBT::Type::sarray && ty.length == len_type && ty.is_signed == std::is_signed_v<T>)
                return simple_array_const_ref(enbt);
            else
                throw EnbtException("ENBT is not a simple array or not same");
        }

        using pointer = ENBT*;
        using const_pointer = const ENBT*;
        using reference = ENBT&;
        using const_reference = const ENBT&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using iterator = ENBT*;
        using const_iterator = const ENBT*;
        using reverse_iterator = std::vector<ENBT>::reverse_iterator;
        using const_reverse_iterator = std::vector<ENBT>::const_reverse_iterator;

        simple_array_const_ref(const simple_array_const_ref& tag) {
            proxy = tag.proxy;
            size_ = tag.size_;
        }

        simple_array_const_ref(simple_array_const_ref&& tag) {
            proxy = tag.proxy;
            size_ = tag.size_;
        }

        simple_array_const_ref& operator=(const simple_array_const_ref& tag) {
            proxy = tag.proxy;
            size_ = tag.size_;
            return *this;
        }

        simple_array_const_ref& operator=(simple_array_const_ref&& tag) {
            proxy = tag.proxy;
            size_ = tag.size_;
            return *this;
        }

        T operator[](size_t index) const {
            return proxy[index];
        }

        size_t size() const {
            return size_;
        }

        const T* begin() const {
            return proxy;
        }

        const T* end() const {
            return proxy + size_;
        }

        const T* cbegin() const {
            return proxy;
        }

        const T* cend() const {
            return proxy + size_;
        }

        const T* rbegin() const {
            return proxy + size_;
        }

        const T* rend() const {
            return proxy;
        }

        const T* crbegin() const {
            return proxy + size_;
        }

        const T* crend() const {
            return proxy;
        }
    };

    template <class T>
    class simple_array_ref {
    protected:
        T* proxy;
        size_t size_;

        static constexpr ENBT::TypeLen len_type = []() constexpr {
            if (sizeof(T) < UINT8_MAX)
                return ENBT::TypeLen::Tiny;
            else if (sizeof(T) < UINT16_MAX)
                return ENBT::TypeLen::Short;
            else if (sizeof(T) < UINT32_MAX)
                return ENBT::TypeLen::Default;
            else
                return ENBT::TypeLen::Long;
        }();

        simple_array_ref(ENBT& abstract) {
            proxy = std::get<T*>(abstract.content());
            size_ = abstract.size();
        }

    public:
        static simple_array_ref make_ref(ENBT& enbt) {
            auto ty = enbt.type_id();
            if (ty.type == ENBT::Type::sarray && ty.length == len_type && ty.is_signed == std::is_signed_v<T>)
                return simple_array_ref(enbt);
            else
                throw EnbtException("ENBT is not a simple array or not same");
        }

        static simple_array_const_ref<T> make_ref(const ENBT& enbt) {
            return simple_array_const_ref<T>::make_ref(enbt);
        }

        using pointer = ENBT*;
        using const_pointer = const ENBT*;
        using reference = ENBT&;
        using const_reference = const ENBT&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using iterator = ENBT*;
        using const_iterator = const ENBT*;
        using reverse_iterator = std::vector<ENBT>::reverse_iterator;
        using const_reverse_iterator = std::vector<ENBT>::const_reverse_iterator;

        simple_array_ref(const simple_array_ref& tag) {
            proxy = tag.proxy;
            size_ = tag.size_;
        }

        simple_array_ref(simple_array_ref&& tag) {
            proxy = tag.proxy;
            size_ = tag.size_;
        }

        simple_array_ref& operator=(const simple_array_ref& tag) {
            proxy = tag.proxy;
            size_ = tag.size_;
            return *this;
        }

        simple_array_ref& operator=(simple_array_ref&& tag) {
            proxy = tag.proxy;
            size_ = tag.size_;
            return *this;
        }

        T& operator[](size_t index) {
            return proxy[index];
        }

        T operator[](size_t index) const {
            return proxy[index];
        }

        size_t size() const {
            return size_;
        }

        T* begin() {
            return proxy;
        }

        T* end() {
            return proxy + size_;
        }

        const T* begin() const {
            return proxy;
        }

        const T* end() const {
            return proxy + size_;
        }

        const T* cbegin() const {
            return proxy;
        }

        const T* cend() const {
            return proxy + size_;
        }

        T* rbegin() {
            return proxy + size_;
        }

        T* rend() {
            return proxy;
        }

        const T* rbegin() const {
            return proxy + size_;
        }

        const T* rend() const {
            return proxy;
        }

        const T* crbegin() const {
            return proxy + size_;
        }

        const T* crend() const {
            return proxy;
        }
    };

    class compound : public compound_ref {
        ENBT holder;

    public:
        compound()
            : holder(ENBT::compound()) {
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
        }

        compound(const compound& copy) {
            holder = copy.holder;
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
        }

        compound(compound&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
        }

        compound(std::unordered_map<std::string, ENBT>&& init)
            : holder(init) {
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
        }

        compound(const std::unordered_map<std::string, ENBT>& init)
            : holder(init) {
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
        }

        compound(std::initializer_list<std::pair<const std::string, ENBT>> init)
            : holder(std::unordered_map<std::string, ENBT>(init)) {
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
        }


        compound& operator=(const ENBT& copy) {
            if (!copy.is_compound())
                throw EnbtException("ENBT is not a compound");
            holder = copy;
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
            return *this;
        }

        compound& operator=(ENBT&& copy) {
            if (!copy.is_compound())
                throw EnbtException("ENBT is not a compound");
            holder = std::move(copy);
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
            return *this;
        }

        compound& operator=(const compound& copy) {
            holder = copy.holder;
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
            return *this;
        }

        compound& operator=(compound&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::unordered_map<std::string, ENBT>*>(holder.content());
            return *this;
        }

        bool operator==(const compound& tag) const {
            return holder == tag.holder;
        }

        bool operator!=(const compound& tag) const {
            return holder != tag.holder;
        }

        operator ENBT&() & {
            return holder;
        }

        operator const ENBT&() const& {
            return holder;
        }

        operator ENBT&&() && {
            return std::move(holder);
        }
    };

    class fixed_array : public fixed_array_ref {
        ENBT holder;

    public:
        template <class T>
        fixed_array(std::initializer_list<T> list)
            : fixed_array(list.size()) {
            size_t i = 0;
            for (auto& item : list)
                (*proxy)[i++] = item;
        }

        fixed_array(size_t array_size)
            : holder(ENBT::fixed_array(array_size)) {
            proxy = std::get<std::vector<ENBT>*>(holder.content());
        }

        fixed_array(const fixed_array& copy) {
            holder = copy.holder;
            proxy = std::get<std::vector<ENBT>*>(holder.content());
        }

        fixed_array(fixed_array&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::vector<ENBT>*>(holder.content());
        }

        fixed_array& operator=(const fixed_array& copy) {
            holder = copy.holder;
            proxy = std::get<std::vector<ENBT>*>(holder.content());
            return *this;
        }

        fixed_array& operator=(fixed_array&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::vector<ENBT>*>(holder.content());
            return *this;
        }

        bool operator==(const fixed_array& tag) const {
            return holder == tag.holder;
        }

        bool operator!=(const fixed_array& tag) const {
            return holder != tag.holder;
        }

        operator ENBT&() & {
            return holder;
        }

        operator const ENBT&() const& {
            return holder;
        }

        operator ENBT&&() && {
            return std::move(holder);
        }
    };

    class dynamic_array : public dynamic_array_ref {
        ENBT holder;

    public:
        template <class T>
        dynamic_array(std::initializer_list<T> list)
            : dynamic_array() {
            size_t i = 0;
            for (auto& item : list)
                (*proxy)[i++] = item;
        }

        template <size_t arr_size>
        dynamic_array(const ENBT arr[arr_size])
            : dynamic_array(arr, arr + arr_size) {}

        dynamic_array(const ENBT* begin, const ENBT* end)
            : dynamic_array() {
            *proxy = std::vector<ENBT>(begin, end);
        }

        dynamic_array()
            : holder(ENBT::dynamic_array()) {
            proxy = std::get<std::vector<ENBT>*>(holder.content());
        }

        dynamic_array(const dynamic_array& copy) {
            holder = copy.holder;
            proxy = std::get<std::vector<ENBT>*>(holder.content());
        }

        dynamic_array(dynamic_array&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::vector<ENBT>*>(holder.content());
        }

        dynamic_array& operator=(const ENBT& copy) {
            if (copy.getType() != ENBT::Type::darray)
                throw EnbtException("ENBT is not a dynamic array");
            holder = copy;
            proxy = std::get<std::vector<ENBT>*>(holder.content());
            return *this;
        }

        dynamic_array& operator=(ENBT&& copy) {
            if (copy.getType() != ENBT::Type::darray)
                throw EnbtException("ENBT is not a dynamic array");
            holder = std::move(copy);
            proxy = std::get<std::vector<ENBT>*>(holder.content());
            return *this;
        }

        dynamic_array& operator=(const dynamic_array& copy) {
            holder = copy.holder;
            proxy = std::get<std::vector<ENBT>*>(holder.content());
            return *this;
        }

        dynamic_array& operator=(dynamic_array&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::vector<ENBT>*>(holder.content());
            return *this;
        }

        bool operator==(const dynamic_array& tag) const {
            return holder == tag.holder;
        }

        bool operator!=(const dynamic_array& tag) const {
            return holder != tag.holder;
        }

        operator ENBT&() & {
            return holder;
        }

        operator const ENBT&() const& {
            return holder;
        }

        operator ENBT&&() && {
            return std::move(holder);
        }
    };

    template <class T>
    class simple_array : public simple_array_ref<T> {
        ENBT holder;

    public:
        simple_array(std::initializer_list<T> list)
            : simple_array(list.size()) {
            size_t i = 0;
            for (auto& item : list)
                simple_array_ref<T>::proxy[i++] = item;
        }

        template <size_t arr_size>
        simple_array(const T (&arr)[arr_size])
            : simple_array(arr, arr + arr_size) {}

        simple_array(const T* begin, const T* end)
            : simple_array(end - begin) {
            size_t i = 0;
            for (auto it = begin; it != end; ++it)
                simple_array_ref<T>::proxy[i++] = *it;
        }

        simple_array(size_t array_size)
            : simple_array_ref<T>(holder), holder(ENBT::Type_ID(ENBT::Type::sarray, simple_array_ref<T>::len_type, std::is_signed_v<T>), array_size) {}

        simple_array(const simple_array& copy)
            : simple_array_ref<T>(holder), holder(copy.holder) {}

        simple_array(simple_array&& move)
            : simple_array_ref<T>(holder), holder(std::move(move.holder)) {}

        simple_array& operator=(const simple_array& copy) {
            holder = copy.holder;
            simple_array_ref<T>::proxy = std::get<T*>(holder.content());
            return *this;
        }

        simple_array& operator=(simple_array&& move) {
            holder = std::move(move.holder);
            simple_array_ref<T>::proxy = std::get<T*>(holder.content());
            return *this;
        }

        bool operator==(const simple_array& tag) const {
            return holder == tag.holder;
        }

        bool operator!=(const simple_array& tag) const {
            return holder != tag.holder;
        }

        operator ENBT&() & {
            return holder;
        }

        operator const ENBT&() const& {
            return holder;
        }

        operator ENBT&&() && {
            return std::move(holder);
        }
    };

    using simple_array_ref_ui8 = simple_array_ref<uint8_t>;
    using simple_array_ref_ui16 = simple_array_ref<uint16_t>;
    using simple_array_ref_ui32 = simple_array_ref<uint32_t>;
    using simple_array_ref_ui64 = simple_array_ref<uint64_t>;
    using simple_array_ref_i8 = simple_array_ref<int8_t>;
    using simple_array_ref_i16 = simple_array_ref<int16_t>;
    using simple_array_ref_i32 = simple_array_ref<int32_t>;
    using simple_array_ref_i64 = simple_array_ref<int64_t>;

    using simple_array_ui8 = simple_array<uint8_t>;
    using simple_array_ui16 = simple_array<uint16_t>;
    using simple_array_ui32 = simple_array<uint32_t>;
    using simple_array_ui64 = simple_array<uint64_t>;
    using simple_array_i8 = simple_array<int8_t>;
    using simple_array_i16 = simple_array<int16_t>;
    using simple_array_i32 = simple_array<int32_t>;
    using simple_array_i64 = simple_array<int64_t>;

    class bit {
        ENBT holder = ENBT(false);

    public:
        bit() = default;

        bit(const ENBT& abstract) {
            if (abstract.getType() == ENBT::Type::bit)
                holder = abstract;
        }

        bit(ENBT&& abstract) {
            if (abstract.getType() == ENBT::Type::bit)
                holder = std::move(abstract);
        }

        bit(const bit& tag) {
            holder = tag.holder;
        }

        bit(bit&& tag) {
            holder = std::move(tag.holder);
        }

        bit& operator=(const bit& tag) {
            holder = tag.holder;
            return *this;
        }

        bit& operator=(bit&& tag) {
            holder = std::move(tag.holder);
            return *this;
        }

        bit& operator=(bool value) {
            holder = ENBT(value);
            return *this;
        }

        operator bool() const {
            return holder;
        }

        operator ENBT&() & {
            return holder;
        }

        operator const ENBT&() const& {
            return holder;
        }

        operator ENBT&&() && {
            return std::move(holder);
        }
    };

    class optional {
        ENBT holder = ENBT::optional();

    public:
        optional() = default;

        optional(const ENBT& abstract) {
            if (abstract.getType() == ENBT::Type::optional)
                holder = abstract;
        }

        optional(ENBT&& abstract) {
            if (abstract.getType() == ENBT::Type::optional)
                holder = std::move(abstract);
        }

        optional(const optional& tag) {
            holder = tag.holder;
        }

        optional(optional&& tag) {
            holder = std::move(tag.holder);
        }

        optional& operator=(const optional& tag) {
            holder = tag.holder;
            return *this;
        }

        optional& operator=(optional&& tag) {
            holder = std::move(tag.holder);
            return *this;
        }

        bool has_value() const {
            return holder.contains();
        }

        void reset() {
            holder = ENBT::optional();
        }

        ENBT& get() {
            if (auto res = holder.getOptional(); res != nullptr)
                return *res;
            else
                throw EnbtException("Optional is empty");
        }

        const ENBT& get() const {
            if (auto res = holder.getOptional(); res != nullptr)
                return *res;
            else
                throw EnbtException("Optional is empty");
        }

        template <class _Fn>
        optional& if_has_value(const _Fn& _Func) {
            if (has_value())
                _Func(get());
            return *this;
        }

        template <class _Fn>
        optional& or_else(const _Fn& _Func) {
            if (!has_value())
                _Func();
            return *this;
        }

        operator ENBT&() & {
            return holder;
        }

        operator const ENBT&() const& {
            return holder;
        }

        operator ENBT&&() && {
            return std::move(holder);
        }
    };

    class uuid {
        ENBT holder = ENBT(ENBT::UUID());

    public:
        uuid() = default;

        uuid(const ENBT& abstract) {
            if (abstract.getType() == ENBT::Type::uuid)
                holder = abstract;
        }

        uuid(ENBT&& abstract) {
            if (abstract.getType() == ENBT::Type::uuid)
                holder = std::move(abstract);
        }

        uuid(const uuid& tag) {
            holder = tag.holder;
        }

        uuid(uuid&& tag) {
            holder = std::move(tag.holder);
        }

        uuid& operator=(const uuid& tag) {
            holder = tag.holder;
            return *this;
        }

        uuid& operator=(uuid&& tag) {
            holder = std::move(tag.holder);
            return *this;
        }

        uuid& operator=(ENBT::UUID value) {
            holder = value;
            return *this;
        }

        operator ENBT::UUID() {
            return std::get<ENBT::UUID>(holder.content());
        }

        operator const ENBT::UUID&() const {
            return holder;
        }

        operator ENBT&() & {
            return holder;
        }

        operator const ENBT&() const& {
            return holder;
        }

        operator ENBT&&() && {
            return std::move(holder);
        }
    };
}

namespace std {
    template <>
    struct hash<ENBT::UUID> {
        size_t operator()(const ENBT::UUID& uuid) const {
            return hash<uint64_t>()(uuid.data[0]) ^ hash<uint64_t>()(uuid.data[1]);
        }
    };
}

class ENBTHelper {
    template <class T>
    static T _read_as_(std::istream& input_stream) {
        T res;
        input_stream.read((char*)&res, sizeof(T));
        return res;
    }

public:
    static void WriteCompressLen(std::ostream& write_stream, uint64_t len) {
        union {
            uint64_t full = 0;
            uint8_t part[8];

        } b;

        b.full = ENBT::ConvertEndian(std::endian::big, len);

        constexpr struct {
            uint64_t b64 : 62 = -1;
            uint64_t b32 : 30 = -1;
            uint64_t b16 : 14 = -1;
            uint64_t b8 : 6 = -1;
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
            throw std::overflow_error("uint64_t Cannot put in to uint60_t");
    }

    template <class T>
    static void WriteVar(std::ostream& write_stream, T value, std::endian endian = std::endian::native) {
        value = ENBT::ConvertEndian(endian, value);
        do {
            char currentByte = (char)(value & 0b01111111);

            value >>= 7;
            if (value != 0)
                currentByte |= 0b10000000;

            write_stream << currentByte;
        } while (value != 0);
    }

    template <class T>
    static void WriteVar(std::ostream& write_stream, ENBT::EnbtValue value, std::endian endian = std::endian::native) {
        WriteVar(write_stream, std::get<T>(value), endian);
    }

    static void WriteTypeID(std::ostream& write_stream, ENBT::Type_ID tid) {
        write_stream << tid.raw;
    }

    template <class T>
    static void WriteValue(std::ostream& write_stream, T value, std::endian endian = std::endian::native) {
        if constexpr (std::is_same<T, ENBT::UUID>())
            ENBT::ConvertEndian(endian, value.data, 16);
        else
            value = ENBT::ConvertEndian(endian, value);
        uint8_t* proxy = (uint8_t*)&value;
        for (size_t i = 0; i < sizeof(T); i++)
            write_stream << proxy[i];
    }

    template <class T>
    static void WriteValue(std::ostream& write_stream, ENBT::EnbtValue value, std::endian endian = std::endian::native) {
        return WriteValue(write_stream, std::get<T>(value), endian);
    }

    template <class T>
    static void WriteArray(std::ostream& write_stream, T* values, size_t len, std::endian endian = std::endian::native) {
        if constexpr (sizeof(T) == 1) {
            for (size_t i = 0; i < len; i++)
                write_stream << values[i];
        } else {
            for (size_t i = 0; i < len; i++)
                WriteValue(write_stream, values[i], endian);
        }
    }

    template <class T>
    static void WriteArray(std::ostream& write_stream, ENBT::EnbtValue* values, size_t len, std::endian endian = std::endian::native) {
        T* arr = new T[len];
        for (size_t i = 0; i < len; i++)
            arr[i] = std::get<T>(values[i]);
        WriteArray(write_stream, arr, len, endian);
        delete[] arr;
    }

    static void WriteString(std::ostream& write_stream, const ENBT& val) {
        const std::string& str_ref = (const std::string&)val;
        size_t real_size = str_ref.size();
        size_t size_without_null = real_size ? (str_ref[real_size - 1] != 0 ? real_size : real_size - 1) : 0;
        WriteCompressLen(write_stream, size_without_null);
        WriteArray(write_stream, str_ref.data(), size_without_null);
    }


    template <class T>
    static void WriteDefineLen(std::ostream& write_stream, T value) {
        return WriteValue(write_stream, value, std::endian::little);
    }

    static void WriteDefineLen(std::ostream& write_stream, uint64_t len, ENBT::Type_ID tid) {
        switch (tid.length) {
        case ENBT::TypeLen::Tiny:
            if (len != ((uint8_t)len))
                throw EnbtException("Cannot convert value to uint8_t");
            WriteDefineLen(write_stream, (uint8_t)len);
            break;
        case ENBT::TypeLen::Short:
            if (len != ((uint16_t)len))
                throw EnbtException("Cannot convert value to uint16_t");
            WriteDefineLen(write_stream, (uint16_t)len);
            break;
        case ENBT::TypeLen::Default:
            if (len != ((uint32_t)len))
                throw EnbtException("Cannot convert value to uint32_t");
            WriteDefineLen(write_stream, (uint32_t)len);
            break;
        case ENBT::TypeLen::Long:
            return WriteDefineLen(write_stream, (uint64_t)len);
            break;
        }
    }

    static void InitializeVersion(std::ostream& write_stream) {
        write_stream << (char)ENBT_VERSION_HEX;
    }

    static void WriteCompound(std::ostream& write_stream, const ENBT& val) {
        auto result = std::get<std::unordered_map<std::string, ENBT>*>(val.content());
        WriteDefineLen(write_stream, result->size(), val.type_id());
        for (auto& it : *result) {
            WriteString(write_stream, it.first);
            WriteToken(write_stream, it.second);
        }
    }

    static void WriteArray(std::ostream& write_stream, const ENBT& val) {
        if (!val.is_array())
            throw EnbtException("This is not array for serialize it");
        auto result = (std::vector<ENBT>*)val.getPtr();
        size_t len = result->size();
        WriteDefineLen(write_stream, len, val.type_id());
        if (len) {
            ENBT::Type_ID tid = (*result)[0].type_id();
            WriteTypeID(write_stream, tid);
            if (tid.type != ENBT::Type::bit) {
                for (const auto& it : *result)
                    WriteValue(write_stream, it);
            } else {
                tid.is_signed = false;
                int8_t i = 0;
                uint8_t value = 0;
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

    static void WriteDArray(std::ostream& write_stream, const ENBT& val) {
        if (!val.is_array())
            throw EnbtException("This is not array for serialize it");
        auto result = (std::vector<ENBT>*)val.getPtr();
        WriteDefineLen(write_stream, result->size(), val.type_id());
        for (auto& it : *result)
            WriteToken(write_stream, it);
    }

    static void WriteSimpleArray(std::ostream& write_stream, const ENBT& val) {
        WriteCompressLen(write_stream, val.size());
        switch (val.type_id().length) {
        case ENBT::TypeLen::Tiny:
            if (val.type_id().is_signed)
                WriteArray(write_stream, val.getPtr(), val.size(), val.type_id().getEndian());
            else
                WriteArray(write_stream, val.getPtr(), val.size());
            break;
        case ENBT::TypeLen::Short:
            if (val.type_id().is_signed)
                WriteArray(write_stream, (uint16_t*)val.getPtr(), val.size(), val.type_id().getEndian());
            else
                WriteArray(write_stream, (uint16_t*)val.getPtr(), val.size());
            break;
        case ENBT::TypeLen::Default:
            if (val.type_id().is_signed)
                WriteArray(write_stream, (uint32_t*)val.getPtr(), val.size(), val.type_id().getEndian());
            else
                WriteArray(write_stream, (uint32_t*)val.getPtr(), val.size());
            break;
        case ENBT::TypeLen::Long:
            if (val.type_id().is_signed)
                WriteArray(write_stream, (uint64_t*)val.getPtr(), val.size(), val.type_id().getEndian());
            else
                WriteArray(write_stream, (uint64_t*)val.getPtr(), val.size());
            break;
        default:
            break;
        }
    }

    static void WriteValue(std::ostream& write_stream, const ENBT& val) {
        ENBT::Type_ID tid = val.type_id();
        switch (tid.type) {
        case ENBT::Type::integer:
            switch (tid.length) {
            case ENBT::TypeLen::Tiny:
                if (tid.is_signed)
                    return WriteValue<int8_t>(write_stream, val.content(), tid.getEndian());
                else
                    return WriteValue<uint8_t>(write_stream, val.content(), tid.getEndian());
            case ENBT::TypeLen::Short:
                if (tid.is_signed)
                    return WriteValue<int16_t>(write_stream, val.content(), tid.getEndian());
                else
                    return WriteValue<uint16_t>(write_stream, val.content(), tid.getEndian());
            case ENBT::TypeLen::Default:
                if (tid.is_signed)
                    return WriteValue<int32_t>(write_stream, val.content(), tid.getEndian());
                else
                    return WriteValue<uint32_t>(write_stream, val.content(), tid.getEndian());
            case ENBT::TypeLen::Long:
                if (tid.is_signed)
                    return WriteValue<int64_t>(write_stream, val.content(), tid.getEndian());
                else
                    return WriteValue<uint64_t>(write_stream, val.content(), tid.getEndian());
            }
            return;
        case ENBT::Type::floating:
            switch (tid.length) {
            case ENBT::TypeLen::Default:
                return WriteValue<float>(write_stream, val.content(), tid.getEndian());
            case ENBT::TypeLen::Long:
                return WriteValue<double>(write_stream, val.content(), tid.getEndian());
            }
            return;
        case ENBT::Type::var_integer:
            switch (tid.length) {
            case ENBT::TypeLen::Default:
                if (tid.is_signed)
                    return WriteVar<int32_t>(write_stream, val.content(), tid.getEndian());
                else
                    return WriteVar<uint32_t>(write_stream, val.content(), tid.getEndian());
            case ENBT::TypeLen::Long:
                if (tid.is_signed)
                    return WriteVar<int64_t>(write_stream, val.content(), tid.getEndian());
                else
                    return WriteVar<uint64_t>(write_stream, val.content(), tid.getEndian());
            }
            return;
        case ENBT::Type::uuid:
            return WriteValue<ENBT::UUID>(write_stream, val.content(), tid.getEndian());
        case ENBT::Type::sarray:
            return WriteSimpleArray(write_stream, val);
        case ENBT::Type::darray:
            return WriteDArray(write_stream, val);
        case ENBT::Type::compound:
            return WriteCompound(write_stream, val);
        case ENBT::Type::array:
            return WriteArray(write_stream, val);
        case ENBT::Type::optional:
            if (val.contains())
                WriteToken(write_stream, *val.getOptional());
            break;
        case ENBT::Type::string:
            return WriteString(write_stream, val);
        }
    }

    static void WriteToken(std::ostream& write_stream, const ENBT& val) {
        write_stream << val.type_id().raw;
        WriteValue(write_stream, val);
    }

    template <class T>
    static T ReadVar(std::istream& read_stream, std::endian endian) {
        constexpr int max_offset = (sizeof(T) / 5 * 5 + ((sizeof(T) % 5) > 0)) * 8;
        T decodedInt = 0;
        T bitOffset = 0;
        char currentByte = 0;
        do {
            if (bitOffset == max_offset)
                throw EnbtException("Var value too big");
            currentByte = _read_as_<char>(read_stream);
            decodedInt |= T(currentByte & 0b01111111) << bitOffset;
            bitOffset += 7;
        } while ((currentByte & 0b10000000) != 0);
        return ENBT::ConvertEndian(endian, decodedInt);
    }

    static ENBT::Type_ID ReadTypeID(std::istream& read_stream) {
        ENBT::Type_ID result;
        result.raw = _read_as_<uint8_t>(read_stream);
        return result;
    }

    template <class T>
    static T ReadValue(std::istream& read_stream, std::endian endian = std::endian::native) {
        T tmp;
        if constexpr (std::is_same<T, ENBT::UUID>()) {
            for (size_t i = 0; i < 16; i++)
                tmp.data[i] = _read_as_<uint8_t>(read_stream);
            ENBT::ConvertEndian(endian, tmp.data, 16);
        } else {
            uint8_t* proxy = (uint8_t*)&tmp;
            for (size_t i = 0; i < sizeof(T); i++)
                proxy[i] = _read_as_<uint8_t>(read_stream);
            ENBT::ConvertEndian(endian, tmp);
        }
        return tmp;
    }

    template <class T>
    static T* ReadArray(std::istream& read_stream, size_t len, std::endian endian = std::endian::native) {
        T* tmp = new T[len];
        if constexpr (sizeof(T) == 1) {
            for (size_t i = 0; i < len; i++)
                tmp[i] = _read_as_<uint8_t>(read_stream);
        } else {
            for (size_t i = 0; i < len; i++)
                tmp[i] = ReadValue<T>(read_stream, endian);
        }
        return tmp;
    }

    template <class T>
    static T ReadDefineLen(std::istream& read_stream) {
        return ReadValue<T>(read_stream, std::endian::little);
    }

    static size_t ReadDefineLen(std::istream& read_stream, ENBT::Type_ID tid) {
        switch (tid.length) {
        case ENBT::TypeLen::Tiny:
            return ReadDefineLen<uint8_t>(read_stream);
        case ENBT::TypeLen::Short:
            return ReadDefineLen<uint16_t>(read_stream);
        case ENBT::TypeLen::Default:
            return ReadDefineLen<uint32_t>(read_stream);
        case ENBT::TypeLen::Long: {
            uint64_t val = ReadDefineLen<uint64_t>(read_stream);
            if ((size_t)val != val)
                throw std::overflow_error("Array length too big for this platform");
            return val;
        }
        default:
            return 0;
        }
    }

    static uint64_t ReadDefineLen64(std::istream& read_stream, ENBT::Type_ID tid) {
        switch (tid.length) {
        case ENBT::TypeLen::Tiny:
            return ReadDefineLen<uint8_t>(read_stream);
        case ENBT::TypeLen::Short:
            return ReadDefineLen<uint16_t>(read_stream);
        case ENBT::TypeLen::Default:
            return ReadDefineLen<uint32_t>(read_stream);
        case ENBT::TypeLen::Long:
            return ReadDefineLen<uint64_t>(read_stream);
        default:
            return 0;
        }
    }

    static uint64_t ReadCompressLen(std::istream& read_stream) {
        union {
            uint8_t complete = 0;

            struct {
                uint8_t len : 6;
                uint8_t len_flag : 2;
            } partial;
        } b;

        read_stream.read((char*)&b.complete, 1);
        switch (b.partial.len_flag) {
        case 0:
            return b.partial.len;
        case 1: {
            uint16_t full = b.partial.len;
            full <<= 8;
            full |= _read_as_<uint8_t>(read_stream);
            return ENBT::ConvertEndian(std::endian::little, full);
        }
        case 2: {
            uint32_t full = b.partial.len;
            full <<= 24;
            full |= _read_as_<uint8_t>(read_stream);
            full <<= 16;
            full |= _read_as_<uint8_t>(read_stream);
            full <<= 8;
            full |= _read_as_<uint8_t>(read_stream);
            return ENBT::ConvertEndian(std::endian::little, full);
        }
        case 3: {
            uint64_t full = b.partial.len;
            full <<= 56;
            full |= _read_as_<uint8_t>(read_stream);
            full <<= 48;
            full |= _read_as_<uint8_t>(read_stream);
            full <<= 40;
            full |= _read_as_<uint8_t>(read_stream);
            full <<= 24;
            full |= _read_as_<uint8_t>(read_stream);
            full <<= 24;
            full |= _read_as_<uint8_t>(read_stream);
            full <<= 16;
            full |= _read_as_<uint8_t>(read_stream);
            full <<= 8;
            full |= _read_as_<uint8_t>(read_stream);
            return ENBT::ConvertEndian(std::endian::little, full);
        }
        default:
            return 0;
        }
    }

    static std::string ReadString(std::istream& read_stream) {
        uint64_t read = ReadCompressLen(read_stream);
        std::string res;
        res.reserve(read);
        for (uint64_t i = 0; i < read; i++)
            res.push_back(ReadValue<char>(read_stream));
        return res;
    }

    static ENBT ReadCompound(std::istream& read_stream, ENBT::Type_ID tid) {
        size_t len = ReadDefineLen(read_stream, tid);
        std::unordered_map<std::string, ENBT> result;
        for (size_t i = 0; i < len; i++) {
            std::string key = ReadString(read_stream);
            result[key] = ReadToken(read_stream);
        }
        return result;
    }

    static std::vector<ENBT> ReadArray(std::istream& read_stream, ENBT::Type_ID tid) {
        size_t len = ReadDefineLen(read_stream, tid);
        if (!len)
            return {};
        ENBT::Type_ID a_tid = ReadTypeID(read_stream);
        std::vector<ENBT> result(len);
        if (a_tid == ENBT::Type::bit) {
            int8_t i = 0;
            uint8_t value = _read_as_<uint8_t>(read_stream);
            for (auto& it : result) {
                if (i >= 8) {
                    i = 0;
                    value = _read_as_<uint8_t>(read_stream);
                }
                bool set = (bool)(value << i);
                it = set;
            }
        } else {
            for (size_t i = 0; i < len; i++)
                result[i] = ReadValue(read_stream, a_tid);
        }
        return result;
    }

    static std::vector<ENBT> ReadDArray(std::istream& read_stream, ENBT::Type_ID tid) {
        size_t len = ReadDefineLen(read_stream, tid);
        std::vector<ENBT> result(len);
        for (size_t i = 0; i < len; i++)
            result[i] = ReadToken(read_stream);
        return result;
    }

    static ENBT ReadSArray(std::istream& read_stream, ENBT::Type_ID tid) {
        uint64_t len = ReadCompressLen(read_stream);
        auto endian = tid.getEndian();
        ENBT res;
        switch (tid.length) {
        case ENBT::TypeLen::Tiny: {
            uint8_t* arr = ReadArray<uint8_t>(read_stream, len, endian);
            res = {arr, len};
            delete[] arr;
            break;
        }
        case ENBT::TypeLen::Short: {
            uint16_t* arr = ReadArray<uint16_t>(read_stream, len, endian);
            res = {arr, len};
            delete[] arr;
            break;
        }
        case ENBT::TypeLen::Default: {
            uint32_t* arr = ReadArray<uint32_t>(read_stream, len, endian);
            res = {arr, len};
            delete[] arr;
            break;
        }
        case ENBT::TypeLen::Long: {
            uint64_t* arr = ReadArray<uint64_t>(read_stream, len, endian);
            res = {arr, len};
            delete[] arr;
            break;
        }
        default:
            throw EnbtException();
        }
        return res;
    }

    static ENBT ReadValue(std::istream& read_stream, ENBT::Type_ID tid) {
        switch (tid.type) {
        case ENBT::Type::integer:
            switch (tid.length) {
            case ENBT::TypeLen::Tiny:
                if (tid.is_signed)
                    return ReadValue<int8_t>(read_stream, tid.getEndian());
                else
                    return ReadValue<uint8_t>(read_stream, tid.getEndian());
            case ENBT::TypeLen::Short:
                if (tid.is_signed)
                    return ReadValue<int16_t>(read_stream, tid.getEndian());
                else
                    return ReadValue<uint16_t>(read_stream, tid.getEndian());
            case ENBT::TypeLen::Default:
                if (tid.is_signed)
                    return ReadValue<int32_t>(read_stream, tid.getEndian());
                else
                    return ReadValue<uint32_t>(read_stream, tid.getEndian());
            case ENBT::TypeLen::Long:
                if (tid.is_signed)
                    return ReadValue<int64_t>(read_stream, tid.getEndian());
                else
                    return ReadValue<uint64_t>(read_stream, tid.getEndian());
            default:
                return ENBT();
            }
        case ENBT::Type::floating:
            switch (tid.length) {
            case ENBT::TypeLen::Default:
                return ReadValue<float>(read_stream, tid.getEndian());
            case ENBT::TypeLen::Long:
                return ReadValue<double>(read_stream, tid.getEndian());
            default:
                return ENBT();
            }
        case ENBT::Type::var_integer:
            switch (tid.length) {
            case ENBT::TypeLen::Default:
                if (tid.is_signed)
                    return ReadVar<int32_t>(read_stream, tid.getEndian());
                else
                    return ReadVar<uint32_t>(read_stream, tid.getEndian());
            case ENBT::TypeLen::Long:
                if (tid.is_signed)
                    return ReadVar<int64_t>(read_stream, tid.getEndian());
                else
                    return ReadVar<uint64_t>(read_stream, tid.getEndian());
            default:
                return ENBT();
            }
        case ENBT::Type::uuid:
            return ReadValue<ENBT::UUID>(read_stream, tid.getEndian());
        case ENBT::Type::sarray:
            return ReadSArray(read_stream, tid);
        case ENBT::Type::darray:
            return ENBT(ReadDArray(read_stream, tid), tid);
        case ENBT::Type::compound:
            return ReadCompound(read_stream, tid);
        case ENBT::Type::array:
            return ENBT(ReadArray(read_stream, tid), tid);
        case ENBT::Type::optional:
            return tid.is_signed ? ENBT(true, ReadToken(read_stream)) : ENBT(false, ENBT());
        case ENBT::Type::bit:
            return ENBT((bool)tid.is_signed);
        case ENBT::Type::string:
            return ReadString(read_stream);
        default:
            return ENBT();
        }
    }

    static ENBT ReadToken(std::istream& read_stream) {
        return ReadValue(read_stream, ReadTypeID(read_stream));
    }

    static void CheckVersion(std::istream& read_stream) {
        if (ReadValue<uint8_t>(read_stream) != ENBT_VERSION_HEX)
            throw EnbtException("Unsupported version");
    }

    static void SkipSignedCompound(std::istream& read_stream, uint64_t len, bool wide) {
        uint8_t add = 1 + wide;
        for (uint64_t i = 0; i < len; i++) {
            read_stream.seekg(read_stream.tellg() += add);
            SkipToken(read_stream);
        }
    }

    static void SkipUnsignedCompoundString(std::istream& read_stream) {
        uint64_t skip = ReadCompressLen(read_stream);
        read_stream.seekg(read_stream.tellg() += skip);
    }

    static void SkipUnsignedCompound(std::istream& read_stream, uint64_t len, bool wide) {
        uint8_t add = 4;
        if (wide)
            add = 8;
        for (uint64_t i = 0; i < len; i++) {
            SkipUnsignedCompoundString(read_stream);
            SkipToken(read_stream);
        }
    }

    static void SkipCompound(std::istream& read_stream, ENBT::Type_ID tid) {
        uint64_t len = ReadDefineLen64(read_stream, tid);
        if (tid.is_signed)
            SkipSignedCompound(read_stream, len, false);
        else
            SkipUnsignedCompound(read_stream, len, true);
    }

    //return zero if cannot, else return type size
    static uint8_t CanFastIndex(ENBT::Type_ID tid) {
        switch (tid.type) {
        case ENBT::Type::integer:
        case ENBT::Type::floating:
            switch (tid.length) {
            case ENBT::TypeLen::Tiny:
                return 1;
            case ENBT::TypeLen::Short:
                return 2;
            case ENBT::TypeLen::Default:
                return 4;
            case ENBT::TypeLen::Long:
                return 8;
            }
            return 0;
        case ENBT::Type::uuid:
            return 16;
        case ENBT::Type::bit:
            return 1;
        default:
            return 0;
        }
    }

    static void SkipArray(std::istream& read_stream, ENBT::Type_ID tid) {
        uint64_t len = ReadDefineLen64(read_stream, tid);
        if (!len)
            return;
        auto items_tid = ReadTypeID(read_stream);
        if (int index_multiplier = CanFastIndex(items_tid); !index_multiplier)
            for (uint64_t i = 0; i < len; i++)
                SkipValue(read_stream, items_tid);
        else {
            if (tid == ENBT::Type::bit) {
                uint64_t actual_len = len / 8;
                if (len % 8)
                    ++actual_len;
                read_stream.seekg(read_stream.tellg() += actual_len);
            } else
                read_stream.seekg(read_stream.tellg() += len * index_multiplier);
        }
    }

    static void SkipDArray(std::istream& read_stream, ENBT::Type_ID tid) {
        uint64_t len = ReadDefineLen64(read_stream, tid);
        for (uint64_t i = 0; i < len; i++)
            SkipToken(read_stream);
    }

    static void SkipSArray(std::istream& read_stream, ENBT::Type_ID tid) {
        uint64_t index = ReadCompressLen(read_stream);
        switch (tid.length) {
        case ENBT::TypeLen::Tiny:
            read_stream.seekg(read_stream.tellg() += index);
            break;
        case ENBT::TypeLen::Short:
            read_stream.seekg(read_stream.tellg() += index * 2);
            break;
        case ENBT::TypeLen::Default:
            read_stream.seekg(read_stream.tellg() += index * 4);
            break;
        case ENBT::TypeLen::Long:
            read_stream.seekg(read_stream.tellg() += index * 8);
            break;
        default:
            break;
        }
    }

    static void SkipString(std::istream& read_stream) {
        uint64_t len = ReadCompressLen(read_stream);
        read_stream.seekg(read_stream.tellg() += len);
    }

    static void SkipValue(std::istream& read_stream, ENBT::Type_ID tid) {
        switch (tid.type) {
        case ENBT::Type::floating:
        case ENBT::Type::integer:
            switch (tid.length) {
            case ENBT::TypeLen::Tiny:
                read_stream.seekg(read_stream.tellg() += 1);
                break;
            case ENBT::TypeLen::Short:
                read_stream.seekg(read_stream.tellg() += 2);
                break;
            case ENBT::TypeLen::Default:
                read_stream.seekg(read_stream.tellg() += 4);
                break;
            case ENBT::TypeLen::Long:
                read_stream.seekg(read_stream.tellg() += 8);
                break;
            }
            break;
        case ENBT::Type::var_integer:
            switch (tid.length) {
            case ENBT::TypeLen::Default:
                if (tid.is_signed)
                    ReadVar<int32_t>(read_stream, std::endian::native);
                else
                    ReadVar<uint32_t>(read_stream, std::endian::native);
                break;
            case ENBT::TypeLen::Long:
                if (tid.is_signed)
                    ReadVar<int64_t>(read_stream, std::endian::native);
                else
                    ReadVar<uint64_t>(read_stream, std::endian::native);
            }
            break;
        case ENBT::Type::uuid:
            read_stream.seekg(read_stream.tellg() += 16);
            break;
        case ENBT::Type::sarray:
            SkipSArray(read_stream, tid);
            break;
        case ENBT::Type::darray:
            SkipDArray(read_stream, tid);
            break;
        case ENBT::Type::compound:
            SkipCompound(read_stream, tid);
            break;
        case ENBT::Type::array:
            SkipArray(read_stream, tid);
            break;
        case ENBT::Type::optional:
            if (tid.is_signed)
                SkipToken(read_stream);
            break;
        case ENBT::Type::bit:
            break;
        case ENBT::Type::string:
            SkipString(read_stream);
            break;
        }
    }

    static void SkipToken(std::istream& read_stream) {
        return SkipValue(read_stream, ReadTypeID(read_stream));
    }

    //move read stream cursor to value in compound, return true if value found
    static bool FindValueCompound(std::istream& read_stream, ENBT::Type_ID tid, const std::string& key) {
        size_t len = ReadDefineLen(read_stream, tid);
        for (size_t i = 0; i < len; i++) {
            if (ReadString(read_stream) != key)
                SkipValue(read_stream, ReadTypeID(read_stream));
            else
                return true;
        }
        return false;
    }

    static void IndexStaticArray(std::istream& read_stream, uint64_t index, uint64_t len, ENBT::Type_ID targetId) {
        if (index >= len)
            throw EnbtException('[' + std::to_string(index) + "] out of range " + std::to_string(len));
        if (uint8_t skipper = CanFastIndex(targetId)) {
            if (targetId != ENBT::Type::bit)
                read_stream.seekg(read_stream.tellg() += index * skipper);
            else
                read_stream.seekg(read_stream.tellg() += index / 8);
        } else
            for (uint64_t i = 0; i < index; i++)
                SkipValue(read_stream, targetId);
    }

    static void IndexDynArray(std::istream& read_stream, uint64_t index, uint64_t len) {
        if (index >= len)
            throw EnbtException('[' + std::to_string(index) + "] out of range " + std::to_string(len));
        for (uint64_t i = 0; i < index; i++)
            SkipToken(read_stream);
    }

    static void IndexArray(std::istream& read_stream, uint64_t index, ENBT::Type_ID arr_tid) {
        switch (arr_tid.type) {
        case ENBT::Type::array: {
            uint64_t len = ReadDefineLen64(read_stream, arr_tid);
            if (!len)
                throw EnbtException("This array is empty");
            auto targetId = ReadTypeID(read_stream);
            IndexStaticArray(read_stream, index, len, targetId);
            break;
        }
        case ENBT::Type::darray:
            IndexDynArray(read_stream, index, ReadDefineLen64(read_stream, arr_tid));
            break;
        default:
            throw EnbtException("Invalid type id");
        }
    }

    static void IndexArray(std::istream& read_stream, uint64_t index) {
        IndexArray(read_stream, index, ReadTypeID(read_stream));
    }

    static std::vector<std::string> SplitS(std::string str, std::string delimiter) {
        std::vector<std::string> res;
        size_t pos = 0;
        std::string token;
        while ((pos = str.find(delimiter)) != std::string::npos) {
            token = str.substr(0, pos);
            res.push_back(token);
            str.erase(0, pos + delimiter.length());
        }
        return res;
    }

    //move read_stream cursor to value,
    //value_path similar: "0/the test/4/54",
    //return success status
    //can throw EnbtException
    static bool MoveToValuePath(std::istream& read_stream, const std::string& value_path) {
        try {
            for (auto&& tmp : SplitS(value_path, "/")) {
                auto tid = ReadTypeID(read_stream);
                switch (tid.type) {
                case ENBT::Type::array:
                case ENBT::Type::darray:
                    IndexArray(read_stream, std::stoull(tmp), tid);
                    continue;
                case ENBT::Type::compound:
                    if (!FindValueCompound(read_stream, tid, tmp))
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

    static ENBT GetValuePath(std::istream& read_stream, const std::string& value_path) {
        auto old_pos = read_stream.tellg();
        bool is_bit_value = false;
        bool bit_value;
        try {
            for (auto&& tmp : SplitS(value_path, "/")) {
                auto tid = ReadTypeID(read_stream);
                switch (tid.type) {
                case ENBT::Type::array: {
                    uint64_t len = ReadDefineLen64(read_stream, tid);
                    auto targetId = ReadTypeID(read_stream);
                    uint64_t index = std::stoull(tmp);
                    IndexStaticArray(read_stream, index, len, targetId);
                    if (targetId.type == ENBT::Type::bit) {
                        is_bit_value = true;
                        bit_value = _read_as_<uint8_t>(read_stream) << index % 8;
                    }
                    continue;
                }
                case ENBT::Type::darray:
                    IndexArray(read_stream, std::stoull(tmp), tid);
                    continue;
                case ENBT::Type::compound:
                    if (!FindValueCompound(read_stream, tid, tmp))
                        return false;
                    continue;
                default:
                    throw std::invalid_argument("Invalid Path to Value");
                }
            }
        } catch (...) {
            read_stream.seekg(old_pos);
            throw;
        }
        read_stream.seekg(old_pos);
        if (is_bit_value)
            return bit_value;
        return ReadToken(read_stream);
    }
};

    #undef ENBT_VERSION_HEX
    #undef ENBT_VERSION_STR

#endif /* SRC_LIBRARY_ENBT_ENBT */


// ENBT Example file
///////                       type (1 byte)                         compound len
//	[ENBT::Type_ID{ type=compound, length=tiny }][5 (1byte unsigned)]
//		[(6)]["sarray"][{ type=string }][(7)]["Мир!"]
//		[(7)]["integer"][{ type=integer, length=long, is_signed=true }][-9223372036854775808]
//		[(4)]["uuid"][{ type=uuid }][0xD55F0C3556DA165E6F512203C78B57FF]
//		[(6)]["darray"][{ type=darray, length=tiny, is_signed=false }] [3]
//			[{ type=string }][(307)]["\"Pijamalı hasta yağız şoföre çabucak güvendi\"\"Victor jagt zwölf Boxkämpfer quer über den großen Sylter Deich\"\"съешь ещё этих мягких французских булок, да выпей чаю.\"\"Pranzo d'acqua fa volti sghembi\"\"Stróż pchnął kość w quiz gędźb vel fax myjń\"\"]
// 			[{ type=integer, length=long, is_signed=true }][9223372036854775807]
//			[{ type=uuid}][0xD55F0C3556DA165E6F512203C78B57FF]
//
