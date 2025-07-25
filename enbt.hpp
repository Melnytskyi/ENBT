#pragma once
#ifndef ENBT
    #define ENBT 1.1
    #include <any>
    #include <bit>
    #include <cstdint>
    #include <stdexcept>
    #include <initializer_list>
    #include <optional>
    #include <string>
    #include <unordered_map>
    #include <variant>
    #define ENBT_VERSION_HEX 0x11
    #define ENBT_VERSION_STR 1.1

//enchanted named binary tag

namespace enbt {
    namespace endian_helpers {
        void endian_swap(void* value_ptr, std::size_t len);

        inline void convert_endian(std::endian value_endian, void* value_ptr, std::size_t len) {
            if (std::endian::native != value_endian)
                endian_swap(value_ptr, len);
        }

        template <class T>
        T convert_endian(std::endian value_endian, T val) {
            if (std::endian::native != value_endian)
                endian_swap(&val, sizeof(T));
            return val;
        }

        template <class T>
        void convert_endian_arr(std::endian value_endian, T* val, std::size_t size) {
            if (std::endian::native != value_endian)
                for (std::size_t i = 0; i < size; i++)
                    endian_swap(&val[i], sizeof(T));
        }

        template <class T>
        void convert_endian_arr(std::endian value_endian, std::vector<T>& val) {
            if (std::endian::native != value_endian)
                for (auto& it : val)
                    endian_swap(&it, sizeof(T));
        }
    }

    class exception : public std::exception {
    public:
        exception(std::string&& reason)
            : std::exception(reason.c_str()) {}

        exception(const char* reason)
            : std::exception(reason) {}

        exception()
            : std::exception("Undefined enbt exception") {}
    };

    class compound_ref;
    class compound_const_ref;
    class fixed_array_ref;
    class dynamic_array_ref;
    template <class T>
    class simple_array_const_ref;
    using simple_array_const_ref_ui8 = simple_array_const_ref<std::uint8_t>;
    using simple_array_const_ref_ui16 = simple_array_const_ref<std::uint16_t>;
    using simple_array_const_ref_ui32 = simple_array_const_ref<std::uint32_t>;
    using simple_array_const_ref_ui64 = simple_array_const_ref<std::uint64_t>;
    using simple_array_const_ref_i8 = simple_array_const_ref<std::int8_t>;
    using simple_array_const_ref_i16 = simple_array_const_ref<std::int16_t>;
    using simple_array_const_ref_i32 = simple_array_const_ref<std::int32_t>;
    using simple_array_const_ref_i64 = simple_array_const_ref<std::int64_t>;

    template <class T>
    class simple_array_ref;
    using simple_array_ref_ui8 = simple_array_ref<std::uint8_t>;
    using simple_array_ref_ui16 = simple_array_ref<std::uint16_t>;
    using simple_array_ref_ui32 = simple_array_ref<std::uint32_t>;
    using simple_array_ref_ui64 = simple_array_ref<std::uint64_t>;
    using simple_array_ref_i8 = simple_array_ref<std::int8_t>;
    using simple_array_ref_i16 = simple_array_ref<std::int16_t>;
    using simple_array_ref_i32 = simple_array_ref<std::int32_t>;
    using simple_array_ref_i64 = simple_array_ref<std::int64_t>;

    class compound;
    class fixed_array;
    class dynamic_array;
    template <class T>
    class simple_array;


    using simple_array_ui8 = simple_array<std::uint8_t>;
    using simple_array_ui16 = simple_array<std::uint16_t>;
    using simple_array_ui32 = simple_array<std::uint32_t>;
    using simple_array_ui64 = simple_array<std::uint64_t>;
    using simple_array_i8 = simple_array<std::int8_t>;
    using simple_array_i16 = simple_array<std::int16_t>;
    using simple_array_i32 = simple_array<std::int32_t>;
    using simple_array_i64 = simple_array<std::int64_t>;

    class bit;
    class optional;
    class uuid;

    enum class endian : std::uint8_t {
        little = 0,
        big = 1,
        native = (std::uint8_t)(std::endian::native == std::endian::little ? enbt::endian::little : enbt::endian::big)
    };

    enum class type : std::uint8_t {
        //[(len)] = [00XXXXXX], [01XXXXXX XXXXXXXX], etc... 00 - 1 byte,01 - 2 byte,02 - 4 byte,03 - 8 byte
        none, //[0byte]
        integer,
        floating,
        var_integer,  //ony default and long length
        comp_integer, //[(value)]//same as var_integer but uses another encoding format as `[(len)]`
        uuid,         //[16byte]
        sarray,       // [(len)]{nums} contains array of integers

        compound,
        //[len][... items]   ((named items list))
        //			item [(len)][chars] {type_id 1byte} (value_define_and_data)

        darray, //		 [len][... items]   ((unnamed items list))
        //				item {type_id 1byte} (value_define_and_data)

        array, //[len][type_id 1byte]{... items} /unnamed items array, if contain static value size reader can get one element without reading all elems[int,double,e.t.c..]
        //				item {value_define_and_data}

        optional, // 'any value'     (contain value if is_signed == true)  /example [optional,unsigned]
        //	 				 											  /example [optional,signed][string][(3)]{"Yay"}
        bit,      //[0byte] bit in is_signed flag from Type_id byte
        string,   //[(len)][chars]  /utf8 string
        log_item, //[(log entry size in bytes)] [log entry] used for faster log reading
    };
    enum class type_len : std::uint8_t { // array string, e.t.c. length's always little endian
        Tiny,
        Short,
        Default,
        Long
    };

    enbt::type_len calc_type_len(std::size_t len);

    struct type_id {
        std::uint8_t is_signed : 1;
        enbt::endian endian : 1;
        enbt::type_len length : 2;
        enbt::type type : 4;

        constexpr std::endian get_endian() const {
            if (endian == enbt::endian::big)
                return std::endian::big;
            else
                return std::endian::little;
        }

        constexpr bool operator!=(type_id cmp) const {
            return !operator==(cmp);
        }

        constexpr bool operator==(type_id cmp) const {
            return type == cmp.type && length == cmp.length && endian == cmp.endian && is_signed == cmp.is_signed;
        }

        constexpr type_id(enbt::type ty = enbt::type::none, enbt::type_len lt = enbt::type_len::Tiny, enbt::endian en = enbt::endian::native, bool sign = false) {
            type = ty;
            length = lt;
            endian = en;
            is_signed = sign;
        }

        constexpr type_id(enbt::type ty, enbt::type_len lt, std::endian en, bool sign = false) {
            type = ty;
            length = lt;
            endian = (enbt::endian)en;
            is_signed = sign;
        }

        constexpr type_id(enbt::type ty, enbt::type_len lt, bool sign) {
            type = ty;
            length = lt;
            endian = enbt::endian::native;
            is_signed = sign;
        }
    };

    struct version {            //current 1.1, max 15.15
        std::uint8_t major : 4; //0x01
        std::uint8_t minor : 4; //0x01
    };

    struct raw_uuid {
        using value_type = std::uint8_t;
        using reference = std::uint8_t&;
        using pointer = std::uint8_t*;
        using const_reference = std::uint8_t const&;
        using iterator = std::uint8_t*;
        using const_iterator = std::uint8_t const*;
        using difference_type = std::ptrdiff_t;
        using size_type = std::size_t;

        constexpr iterator begin() noexcept {
            return data;
        }

        constexpr const_iterator begin() const noexcept {
            return data;
        }

        constexpr iterator end() noexcept {
            return data + size();
        }

        constexpr const_iterator end() const noexcept {
            return data + size();
        }

        constexpr size_type size() const noexcept {
            return 16;
        }

        enum class family_t {
            unknown = -1,
            ncs = 0,
            rfc_4122 = 1,
            microsoft = 2,
            future = 3,
        };

        constexpr bool is_null() const noexcept {
            return data[0] | data[1] | data[2] | data[3] | data[4] | data[5] | data[6] | data[7]
                   | data[8] | data[9] | data[10] | data[11] | data[12] | data[13] | data[14] | data[15];
        }

        constexpr family_t family() const noexcept {
            auto octet7 = data[8];
            if ((octet7 & 0x80) == 0x00)
                return family_t::ncs;
            else if ((octet7 & 0xC0) == 0x80)
                return family_t::rfc_4122;
            else if ((octet7 & 0xE0) == 0xC0)
                return family_t::microsoft;
            else if ((octet7 & 0xE0) == 0xE0)
                return family_t::future;
            else
                return family_t::unknown;
        }

        enum class version_t {
            unknown = -1,
            time_based = 1,
            dce_security = 2,
            name_based_md5 = 3,
            random_number_based = 4,
            name_based_sha1 = 5,
            sortable_time_based = 6,
            timestamp_and_random = 7,
            custom = 8,
            v1 = time_based,
            v2 = dce_security,
            v3 = name_based_md5,
            v4 = random_number_based,
            v5 = name_based_sha1,
            v6 = sortable_time_based,
            v7 = timestamp_and_random,
            v8 = custom,
        };

        constexpr version_t version() const noexcept {
            auto octet9 = data[6];
            if ((octet9 & 0xF0) == 0x10)
                return version_t::time_based;
            else if ((octet9 & 0xF0) == 0x20)
                return version_t::dce_security;
            else if ((octet9 & 0xF0) == 0x30)
                return version_t::name_based_md5;
            else if ((octet9 & 0xF0) == 0x40)
                return version_t::random_number_based;
            else if ((octet9 & 0xF0) == 0x50)
                return version_t::name_based_sha1;
            else if ((octet9 & 0xF0) == 0x60)
                return version_t::sortable_time_based;
            else if ((octet9 & 0xF0) == 0x70)
                return version_t::timestamp_and_random;
            else if ((octet9 & 0xF0) == 0x80)
                return version_t::custom;
            else
                return version_t::unknown;
        }

        constexpr void swap(raw_uuid& rhs) noexcept {
            std::swap(data, rhs.data);
        }

        constexpr auto operator<=>(const raw_uuid&) const = default;

        // Format: 8-4-4-4-12 (standard UUID format)
        constexpr std::string to_string() const {
            std::string res;
            res.reserve(37);
            for (std::size_t i = 0; i < 16; i++) {
                res += "0123456789abcdef"[data[i] >> 4];
                res += "0123456789abcdef"[data[i] & 0xF];
                if (i == 3 || i == 5 || i == 7 || i == 9)
                    res += '-';
            }
            return res;
        }

        static raw_uuid from_string(std::string_view view) {
            uint64_t parts[2] = {std::hash<std::string_view>()(view), std::hash<size_t>()(view.size())};
            uint64_t p = parts[0];
            parts[0] ^= parts[1] << 5;
            parts[1] ^= p << 2 ^ parts[1];

            enbt::raw_uuid r;
            for (int8_t i = 0; i < 16; i++)
                r.data[i] = reinterpret_cast<uint8_t*>(parts)[i];
            r.data[8] = (r.data[8] & 0x1) & 0xC0;  //microsoft family
            r.data[6] = (r.data[8] & 0x0F) & 0x30; //name_based_md5
            return r;
        }

        static std::string_view from_uuid_string(raw_uuid& res, std::string_view view, bool skip_hyphens = true) {
            //parse standard UUID format that for from to_string function, skip -
            size_t j = 0, i = 0;
            for (; i < 36; i++) {
                if (view[i] == '-' && skip_hyphens)
                    continue;
                if (j == 16)
                    return view.substr(i);
                std::string byteString = std::string(view.substr(i, 2));
                res.data[j++] = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
            }
            if (j != 16)
                throw std::invalid_argument("Invalid UUID string format");
            else
                return view.substr(i);
        }

        static raw_uuid generate_v4();

        static raw_uuid generate_v7();

        static raw_uuid as_null();


        value_type data[16];
    };

    class value {
        friend inline value from_log_item(const value& val);
        friend inline value from_log_item(value&& val);
        friend class fixed_array_ref;
        friend class dynamic_array_ref;

        template <class T, class Enable = void>
        struct __is_optional : std::false_type {};

        template <class T>
        struct __is_optional<std::optional<T>> : std::true_type {};

        template <>
        struct __is_optional<optional> : std::true_type {};

    public:
        typedef std::variant<
            bool,
            std::uint8_t,
            std::int8_t,
            std::uint16_t,
            std::int16_t,
            std::uint32_t,
            std::int32_t,
            std::uint64_t,
            std::int64_t,
            float,
            double,
            std::uint8_t*,
            std::uint16_t*,
            std::uint32_t*,
            std::uint64_t*,
            std::int8_t*,
            std::int16_t*,
            std::int32_t*,
            std::int64_t*,
            std::string*,
            std::vector<value>*,                     //source pointer
            std::unordered_map<std::string, value>*, //source pointer,
            enbt::raw_uuid,
            value*,
            std::nullptr_t>
            value_variants;

        template <class T>
        static T fromVar(std::uint8_t* ch, std::size_t& len) {
            constexpr size_t max_offset = (sizeof(T) / 5 * 5 + ((sizeof(T) % 5) > 0)) * 8;
            std::make_unsigned_t<T> decodedInt = 0;
            size_t bitOffset = 0;
            char currentByte = 0;
            std::size_t i = 0;
            do {
                if (i >= len)
                    throw enbt::exception("VarInt is too big");
                if (bitOffset == max_offset)
                    throw enbt::exception("VarInt is too big");
                currentByte = ch[i++];
                decodedInt |= (currentByte & 0b01111111) << bitOffset;
                bitOffset += 7;
            } while ((currentByte & 0b10000000) != 0);
            len = i;

            if constexpr (std::is_signed_v<T>)
                return (T)((decodedInt >> 1) ^ -(decodedInt & 1));
            else
                return decodedInt;
        }

        template <class T>
        static std::size_t toVar(std::uint8_t* buf, std::size_t buf_len, T val) {
            static constexpr size_t sign_bit_offset = sizeof(T) * 8 - 1;
            static constexpr T least_bits = ~T(0x7F);

            std::size_t i = 0;
            if constexpr (std::is_signed_v<T>)
                val = ((val) << 1) ^ ((val) >> sign_bit_offset);
            while (val & least_bits) {
                if (i >= buf_len)
                    throw enbt::exception("VarInt is too big");
                buf[i++] = (uint8_t(val) | 0x80);
                val >>= 7;
            }
            if (i >= buf_len)
                throw enbt::exception("VarInt is too big");
            buf[i++] = uint8_t(val);
            return i;
        }

    private:
        static value_variants get_content(std::uint8_t* data, std::size_t data_len, type_id data_type_id);
        static std::uint8_t* clone_data(std::uint8_t* data, type_id data_type_id, std::size_t data_len);

        std::uint8_t* clone_data() const {
            return clone_data(data, data_type_id, data_len);
        }

        static std::uint8_t* get_data(std::uint8_t*& data, type_id data_type_id) {
            if (need_to_free(data_type_id))
                return data;
            else
                return (std::uint8_t*)&data;
        }

        static bool need_to_free(type_id data_type_id) {
            switch (data_type_id.type) {
            case enbt::type::none:
            case enbt::type::bit:
            case enbt::type::integer:
            case enbt::type::var_integer:
            case enbt::type::comp_integer:
            case enbt::type::floating:
                return false;
            default:
                return true;
            }
        }

        static void free_data(std::uint8_t* data, type_id data_type_id) {
            if (data == nullptr)
                return;
            switch (data_type_id.type) {
            case enbt::type::none:
            case enbt::type::bit:
            case enbt::type::integer:
            case enbt::type::var_integer:
            case enbt::type::comp_integer:
            case enbt::type::floating:
                break;
            case enbt::type::array:
            case enbt::type::darray:
                delete (std::vector<value>*)data;
                break;
            case enbt::type::compound:
                delete (std::unordered_map<std::string, value>*)data;
                break;
            case enbt::type::optional:
                if (data_type_id.is_signed)
                    delete (value*)data;
                break;
            case enbt::type::uuid:
                delete (enbt::raw_uuid*)data;
                break;
            case enbt::type::string:
                delete (std::string*)data;
                break;
            case enbt::type::log_item:
                delete (value*)data;
                break;
            default:
                delete[] data;
            }
            data = nullptr;
        }

        //if data_len <= 8 contain value in ptr
        //if data_len > 8 contain ptr to bytes array
        //if typeid is darray contain ptr to std::vector<value>
        //if typeid is array contain ptr to array_value struct
        std::uint8_t* data = nullptr;
        std::size_t data_len;
        type_id data_type_id;

        template <class T>
        void set_data(T val) {
            data_len = sizeof(T);
            if (data_len <= 8 && data_type_id.type != enbt::type::uuid) {
                data = nullptr;
                std::memcpy((char*)&data, (char*)&val, data_len);
            } else {
                free_data(data, data_type_id);
                data = (std::uint8_t*)new T(val);
            }
        }

        template <class T>
        void set_data(T* val, std::size_t len) {
            data_len = len * sizeof(T);
            if (data_len <= 8) {
                std::memcpy((char*)&data, (char*)&val, data_len);
            } else {
                free_data(data, data_type_id);
                T* tmp = new T[len / sizeof(T)];
                for (std::size_t i = 0; i < len; i++)
                    tmp[i] = val[i];
                data = (std::uint8_t*)tmp;
            }
        }

        template <class T>
        static std::size_t len(T* val) {
            T* len_calc = val;
            std::size_t size = 1;
            while (*len_calc++)
                size++;
            return size;
        }

        value(std::uint8_t* data, std::size_t data_len, type_id data_type_id)
            : data(data), data_len(data_len), data_type_id(data_type_id) {}

    public:
        static inline value build_inline(std::uint8_t* data, std::size_t data_len, type_id data_type_id) {
            return value(data, data_len, data_type_id);
        }

        template <class T>
        value(const std::vector<T>& array) {
            data_len = array.size();
            data_type_id.type = enbt::type::array;
            data_type_id.is_signed = 0;
            data_type_id.endian = enbt::endian::native;
            if (data_len <= UINT8_MAX)
                data_type_id.length = enbt::type_len::Tiny;
            else if (data_len <= UINT16_MAX)
                data_type_id.length = enbt::type_len::Short;
            else if (data_len <= UINT32_MAX)
                data_type_id.length = enbt::type_len::Default;
            else
                data_type_id.length = enbt::type_len::Long;
            auto res = new std::vector<value>();
            res->reserve(data_len);
            for (const auto& it : array)
                res->push_back(it);
            data = (std::uint8_t*)res;
        }

        template <class T = value>
        value(const std::vector<value>& array) {
            bool as_array = true;
            enbt::type_id tid_check = array[0].type_id();
            for (auto& check : array)
                if (!check.type_equal(tid_check)) {
                    as_array = false;
                    break;
                }
            data_len = array.size();
            if (as_array)
                data_type_id.type = enbt::type::array;
            else
                data_type_id.type = enbt::type::darray;
            if (data_len <= UINT8_MAX)
                data_type_id.length = enbt::type_len::Tiny;
            else if (data_len <= UINT16_MAX)
                data_type_id.length = enbt::type_len::Short;
            else if (data_len <= UINT32_MAX)
                data_type_id.length = enbt::type_len::Default;
            else
                data_type_id.length = enbt::type_len::Long;
            data_type_id.is_signed = 0;
            data_type_id.endian = enbt::endian::native;
            data = (std::uint8_t*)new std::vector<value>(array);
        }

        value();
        value(std::string_view str);
        value(std::string&& str);
        value(const std::string& str);
        value(const char* str);
        value(const char* str, size_t len);

        value(const std::unordered_map<std::string, value>& compound);
        value(std::unordered_map<std::string, value>&& compound);

        value(std::initializer_list<value> arr);
        value(const std::vector<value>& array, type_id tid); //array

        value(std::vector<value>&& array);              //darray
        value(std::vector<value>&& array, type_id tid); //array

        value(const std::uint8_t* arr, std::size_t len);
        value(const std::uint16_t* arr, std::size_t len, std::endian endian = std::endian::native, bool convert_endian = false);
        value(const std::uint32_t* arr, std::size_t len, std::endian endian = std::endian::native, bool convert_endian = false);
        value(const std::uint64_t* arr, std::size_t len, std::endian endian = std::endian::native, bool convert_endian = false);
        value(const std::int8_t* arr, std::size_t len);
        value(const std::int16_t* arr, std::size_t len, std::endian endian = std::endian::native, bool convert_endian = false);
        value(const std::int32_t* arr, std::size_t len, std::endian endian = std::endian::native, bool convert_endian = false);
        value(const std::int64_t* arr, std::size_t len, std::endian endian = std::endian::native, bool convert_endian = false);


        value(std::nullopt_t);
        value(bool bit);
        value(std::int8_t byte);
        value(std::uint8_t byte);
        value(std::int16_t sh, std::endian endian = std::endian::native, bool convert_endian = false);
        value(std::int32_t in, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false);
        value(std::int64_t lon, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false);
        value(std::int32_t in, std::endian endian = std::endian::native, bool convert_endian = false);
        value(std::int64_t lon, std::endian endian = std::endian::native, bool convert_endian = false);
        value(std::uint16_t sh, std::endian endian = std::endian::native, bool convert_endian = false);
        value(std::uint32_t in, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false);
        value(std::uint64_t lon, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false);
        value(std::uint32_t in, std::endian endian = std::endian::native, bool convert_endian = false);
        value(std::uint64_t lon, std::endian endian = std::endian::native, bool convert_endian = false);
        value(float flo, std::endian endian = std::endian::native, bool convert_endian = false);
        value(double dou, std::endian endian = std::endian::native, bool convert_endian = false);
        value(enbt::raw_uuid uuid, std::endian endian = std::endian::native, bool convert_endian = false);

        value(value_variants val, type_id tid, std::size_t length, bool convert_endian = true);
        value(type_id tid, std::size_t len = 0);
        value(enbt::type typ, std::size_t len = 0);
        value(const value& copy);
        value(bool optional, value&& value);
        value(bool optional, const value& value);
        value(value&& copy) noexcept;


        value(const compound& copy);
        value(compound&& copy) noexcept;

        value(const fixed_array& copy);
        value(fixed_array&& copy) noexcept;

        value(const dynamic_array& copy);
        value(dynamic_array&& copy) noexcept;

        value(const simple_array_ui8& copy);
        value(simple_array_ui8&& copy) noexcept;

        value(const simple_array_ui16& copy);
        value(simple_array_ui16&& copy) noexcept;

        value(const simple_array_ui32& copy);
        value(simple_array_ui32&& copy) noexcept;

        value(const simple_array_ui64& copy);
        value(simple_array_ui64&& copy) noexcept;

        value(const simple_array_i8& copy);
        value(simple_array_i8&& copy) noexcept;

        value(const simple_array_i16& copy);
        value(simple_array_i16&& copy) noexcept;

        value(const simple_array_i32& copy);
        value(simple_array_i32&& copy) noexcept;

        value(const simple_array_i64& copy);
        value(simple_array_i64&& copy) noexcept;

        value(const bit& copy);
        value(bit&& copy) noexcept;

        value(const optional& copy);
        value(optional&& copy) noexcept;

        value(const uuid& copy);
        value(uuid&& copy) noexcept;

        ~value() {
            free_data(data, data_type_id);
        }

        value& operator=(const value& copy) {
            data = copy.clone_data();
            data_len = copy.data_len;
            data_type_id = copy.data_type_id;
            return *this;
        }

        value& operator=(value&& move) noexcept {
            data = move.data;
            data_len = move.data_len;
            data_type_id = move.data_type_id;

            move.data_type_id = {};
            move.data = nullptr;
            return *this;
        }

        value& operator=(uint8_t set_value);
        value& operator=(int8_t set_value);
        value& operator=(uint16_t set_value);
        value& operator=(int16_t set_value);
        value& operator=(uint32_t set_value);
        value& operator=(int32_t set_value);
        value& operator=(uint64_t set_value);
        value& operator=(int64_t set_value);
        value& operator=(float set_value);
        value& operator=(double set_value);
        value& operator=(bool set_value);
        value& operator=(raw_uuid set_value);

        template <class Ty>
        value& operator=(const Ty& set_value) {
            using T = std::decay_t<Ty>;
            if constexpr (std::is_same<T, value_variants>().value)
                operator=(value(set_value, data_type_id, data_len, data_type_id.endian));
            else if constexpr (__is_optional<T>::value) {
                if (set_value.has_value())
                    operator=(value(true, *set_value));
                else
                    operator=(value(false, value{}));
            } else
                operator=(value(set_value));
            return *this;
        }

        template <class Ty>
        value& operator=(Ty&& set_value) {
            using T = std::decay_t<Ty>;
            if constexpr (std::is_same<T, value_variants>().value)
                operator=(value(set_value, data_type_id, data_len, data_type_id.endian));
            else if constexpr (__is_optional<T>::value) {
                if (set_value.has_value())
                    operator=(value(true, *set_value));
                else
                    operator=(value(false, value{}));
            } else
                operator=(value(std::forward<Ty>(set_value)));
            return *this;
        }

        bool type_equal(type_id tid) const {
            return !(data_type_id != tid);
        }

        bool is_log_item() const {
            return data_type_id.type == enbt::type::log_item;
        }

        bool is_string() const {
            return data_type_id.type == enbt::type::string;
        }

        bool is_uuid() const {
            return data_type_id.type == enbt::type::uuid;
        }

        bool is_compound() const {
            return data_type_id.type == enbt::type::compound;
        }

        bool is_array() const {
            return data_type_id.type == enbt::type::array || data_type_id.type == enbt::type::darray;
        }

        bool is_dyn_array() const {
            return data_type_id.type == enbt::type::darray;
        }

        bool is_fixed_array() const {
            return data_type_id.type == enbt::type::array;
        }

        bool is_sarray() const {
            return data_type_id.type == enbt::type::sarray;
        }

        bool is_numeric() const {
            return data_type_id.type == enbt::type::integer || data_type_id.type == enbt::type::var_integer || data_type_id.type == enbt::type::comp_integer || data_type_id.type == enbt::type::floating;
        }

        bool is_bit() const {
            return data_type_id.type == enbt::type::bit;
        }

        bool is_none() const {
            return data_type_id.type == enbt::type::none;
        }

        bool is_optional() const {
            return data_type_id.type == enbt::type::optional;
        }

        value& operator[](std::size_t index);

        value& operator[](int index) {
            return operator[]((std::size_t)index);
        }

        value& operator[](const char* index);
        value get_index(std::size_t simple_index) const;
        const value& operator[](std::size_t index) const;

        const value& operator[](int index) const {
            return operator[]((std::size_t)index);
        }

        const value& operator[](const char* index) const;

        const value& operator[](const std::string& index) const {
            return operator[](index.c_str());
        }

        value& operator[](const std::string& index) {
            return operator[](index.c_str());
        }

        value& at(const std::string& index);
        const value& at(const std::string& index) const;
        value& at(std::size_t index);
        const value& at(std::size_t index) const;

        std::size_t size() const {
            switch (data_type_id.type) {
            case enbt::type::sarray:
                return data_len;
            case enbt::type::compound:
                return (*(std::unordered_map<std::string, value>*)data).size();
            case enbt::type::darray:
            case enbt::type::array:
                return (*(std::vector<value>*)data).size();
            case enbt::type::string:
                return (*(std::string*)data).size();
            default:
                throw enbt::exception("This type can not be sized");
            }
        }

        enbt::type_id type_id() {
            switch (data_type_id.type) {
            case enbt::type::compound:
            case enbt::type::darray:
            case enbt::type::array:
            case enbt::type::string: {
                auto siz = size();
                if (siz < UINT8_MAX)
                    data_type_id.length = enbt::type_len::Tiny;
                else if (siz < UINT16_MAX)
                    data_type_id.length = enbt::type_len::Short;
                else if (siz < UINT32_MAX)
                    data_type_id.length = enbt::type_len::Default;
                else
                    data_type_id.length = enbt::type_len::Long;
                break;
            }
            default:
                break;
            }
            return data_type_id;
        }

        enbt::type_id type_id() const {
            switch (data_type_id.type) {
            case enbt::type::compound:
            case enbt::type::darray:
            case enbt::type::array:
            case enbt::type::string: {
                enbt::type_id res = data_type_id;
                auto siz = size();
                if (siz < UINT8_MAX)
                    res.length = enbt::type_len::Tiny;
                else if (siz < UINT16_MAX)
                    res.length = enbt::type_len::Short;
                else if (siz < UINT32_MAX)
                    res.length = enbt::type_len::Default;
                else
                    res.length = enbt::type_len::Long;
                return res;
            }
            default:
                return data_type_id;
            }
        }

        value_variants content() const {
            return get_content(data, data_len, data_type_id);
        }

        void set_optional(const value& _value) {
            if (data_type_id.type == enbt::type::optional) {
                data_type_id.is_signed = true;
                free_data(data, data_type_id);
                data = (std::uint8_t*)new value(_value);
            }
        }

        void set_optional(value&& _value) {
            if (data_type_id.type == enbt::type::optional) {
                data_type_id.is_signed = true;
                free_data(data, data_type_id);
                data = (std::uint8_t*)new value(std::move(_value));
            }
        }

        void set_optional() {
            if (data_type_id.type == enbt::type::optional) {
                free_data(data, data_type_id);
                data_type_id.is_signed = false;
            }
        }

        const value* get_optional() const {
            if (data_type_id.type == enbt::type::optional)
                if (data_type_id.is_signed)
                    return (value*)data;
            return nullptr;
        }

        value* get_optional() {
            if (data_type_id.type == enbt::type::optional)
                if (data_type_id.is_signed)
                    return (value*)data;
            return nullptr;
        }

        const value& get_log_value() const {
            if (data_type_id.type == enbt::type::log_item)
                return *(value*)data;
            return *this;
        }

        value& get_log_value() {
            if (data_type_id.type == enbt::type::log_item)
                return *(value*)data;
            return *this;
        }

        bool is_log_value() {
            return data_type_id.type == enbt::type::log_item;
        }

        bool contains() const {
            if (data_type_id.type == enbt::type::optional)
                return data_type_id.is_signed;
            return data_type_id.type != enbt::type::none;
        }

        bool contains(const char* index) const {
            if (is_compound())
                return ((std::unordered_map<std::string, value>*)data)->contains(index);
            return false;
        }

        bool contains(const std::string& index) const {
            if (is_compound())
                return ((std::unordered_map<std::string, value>*)data)->contains(index);
            return false;
        }

        value& merge(const value& copy) &;
        value& merge(value&& move) &;

        value merge(const value& copy) &&;
        value merge(value&& move) &&;

        enbt::type get_type() const {
            return data_type_id.type;
        }

        enbt::type_len get_type_len() const {
            return data_type_id.length;
        }

        bool get_type_sign() const {
            return data_type_id.is_signed;
        }

        const std::uint8_t* get_internal_ptr() const {
            return data;
        }

        void remove(std::size_t index) {
            if (is_array())
                ((std::vector<value>*)data)->erase(((std::vector<value>*)data)->begin() + index);
            else
                throw enbt::exception("Cannot remove item from non array type");
        }

        void remove(std::string name);

        std::size_t push(const value& enbt) {
            if (is_array()) {
                if (data_type_id.type == enbt::type::array) {
                    if (data_len)
                        if (operator[](0).data_type_id != enbt.data_type_id)
                            throw enbt::exception("Invalid type for pushing array");
                }
                ((std::vector<value>*)data)->push_back(enbt);
                return data_len++;
            } else
                throw enbt::exception("Cannot push to non array type");
        }

        std::size_t push(value&& enbt) {
            if (is_array()) {
                if (data_type_id.type == enbt::type::array) {
                    if (data_len)
                        if (operator[](0).data_type_id != enbt.data_type_id)
                            throw enbt::exception("Invalid type for pushing array");
                }
                ((std::vector<value>*)data)->push_back(std::move(enbt));
                return data_len++;
            } else
                throw enbt::exception("Cannot push to non array type");
        }

        value& front() {
            if (is_array()) {
                if (!data_len)
                    throw enbt::exception("Array empty");
                return ((std::vector<value>*)data)->front();
            } else
                throw enbt::exception("Cannot get front item from non array type");
        }

        const value& front() const {
            if (is_array()) {
                if (!data_len)
                    throw enbt::exception("Array empty");
                return ((std::vector<value>*)data)->front();
            } else
                throw enbt::exception("Cannot get front item from non array type");
        }

        void pop() {
            if (is_array()) {
                if (!data_len)
                    throw enbt::exception("Array empty");
                ((std::vector<value>*)data)->pop_back();
            } else
                throw enbt::exception("Cannot pop front item from non array type");
        }

        void resize(std::size_t siz) {
            if (is_array()) {
                ((std::vector<value>*)data)->resize(siz);
                if (siz < UINT8_MAX)
                    data_type_id.length = enbt::type_len::Tiny;
                else if (siz < UINT16_MAX)
                    data_type_id.length = enbt::type_len::Short;
                else if (siz < UINT32_MAX)
                    data_type_id.length = enbt::type_len::Default;
                else
                    data_type_id.length = enbt::type_len::Long;
            } else if (is_sarray()) {
                switch (data_type_id.length) {
                case enbt::type_len::Tiny: {
                    std::uint8_t* n = new std::uint8_t[siz];
                    for (std::size_t i = 0; i < siz && i < data_len; i++)
                        n[i] = data[i];
                    delete[] data;
                    data_len = siz;
                    data = n;
                    break;
                }
                case enbt::type_len::Short: {
                    std::uint16_t* n = new std::uint16_t[siz];
                    std::uint16_t* prox = (std::uint16_t*)data;
                    for (std::size_t i = 0; i < siz && i < data_len; i++)
                        n[i] = prox[i];
                    delete[] data;
                    data_len = siz;
                    data = (std::uint8_t*)n;
                    break;
                }
                case enbt::type_len::Default: {
                    std::uint32_t* n = new std::uint32_t[siz];
                    std::uint32_t* prox = (std::uint32_t*)data;
                    for (std::size_t i = 0; i < siz && i < data_len; i++)
                        n[i] = prox[i];
                    delete[] data;
                    data_len = siz;
                    data = (std::uint8_t*)n;
                    break;
                }
                case enbt::type_len::Long: {
                    std::uint64_t* n = new std::uint64_t[siz];
                    std::uint64_t* prox = (std::uint64_t*)data;
                    for (std::size_t i = 0; i < siz && i < data_len; i++)
                        n[i] = prox[i];
                    delete[] data;
                    data_len = siz;
                    data = (std::uint8_t*)n;
                    break;
                }
                default:
                    break;
                }


            } else
                throw enbt::exception("Cannot resize non array type");
        }

        void freeze();
        void unfreeze();

        bool operator==(const value& enbt) const {
            if (enbt.data_type_id == data_type_id && data_len == enbt.data_len) {
                switch (data_type_id.type) {
                case enbt::type::sarray:
                    switch (data_type_id.length) {
                    case enbt::type_len::Tiny: {
                        std::uint8_t* other = enbt.data;
                        for (std::size_t i = 0; i < data_len; i++)
                            if (data[i] != other[i])
                                return false;
                        break;
                    }
                    case enbt::type_len::Short: {
                        std::uint16_t* im = (std::uint16_t*)data;
                        std::uint16_t* other = (std::uint16_t*)enbt.data;
                        for (std::size_t i = 0; i < data_len; i++)
                            if (im[i] != other[i])
                                return false;
                        break;
                    }
                    case enbt::type_len::Default: {
                        std::uint32_t* im = (std::uint32_t*)data;
                        std::uint32_t* other = (std::uint32_t*)enbt.data;
                        for (std::size_t i = 0; i < data_len; i++)
                            if (im[i] != other[i])
                                return false;
                        break;
                    }
                    case enbt::type_len::Long: {
                        std::uint64_t* im = (std::uint64_t*)data;
                        std::uint64_t* other = (std::uint64_t*)enbt.data;
                        for (std::size_t i = 0; i < data_len; i++)
                            if (im[i] != other[i])
                                return false;
                        break;
                    }
                    }
                    return true;
                case enbt::type::array:
                case enbt::type::darray:
                    return (*std::get<std::vector<value>*>(content())) == (*std::get<std::vector<value>*>(enbt.content()));
                case enbt::type::compound:
                    return (*std::get<std::unordered_map<std::string, value>*>(content())) == (*std::get<std::unordered_map<std::string, value>*>(enbt.content()));
                case enbt::type::optional:
                    if (data_type_id.is_signed)
                        return (*(value*)data) == (*(value*)enbt.data);
                    return true;
                case enbt::type::bit:
                    return data_type_id.is_signed == enbt.data_type_id.is_signed;
                case enbt::type::uuid:
                    return std::get<enbt::raw_uuid>(content()) == std::get<enbt::raw_uuid>(enbt.content());
                case enbt::type::none:
                    return true;
                case enbt::type::string:
                    return *std::get<std::string*>(content()) == *std::get<std::string*>(enbt.content());
                default:
                    return content() == enbt.content();
                }
            } else
                return false;
        }

        bool operator!=(const value& enbt) const {
            return !operator==(enbt);
        }

        operator bool() const;
        operator std::int8_t() const;
        operator std::int16_t() const;
        operator std::int32_t() const;
        operator std::int64_t() const;
        operator std::uint8_t() const;
        operator std::uint16_t() const;
        operator std::uint32_t() const;
        operator std::uint64_t() const;
        operator float() const;
        operator double() const;

        explicit operator std::string&();
        explicit operator const std::string&() const;

        enbt::value cast_to(enbt::type_id id) const;

        operator std::string() const;
        operator const std::uint8_t*() const;
        operator const std::int8_t*() const;
        operator const char*() const;
        operator const std::int16_t*() const;
        operator const std::uint16_t*() const;
        operator const std::int32_t*() const;
        operator const std::uint32_t*() const;
        operator const std::int64_t*() const;
        operator const std::uint64_t*() const;

        template <class T = value>
        operator std::vector<value>() const {
            return *std::get<std::vector<value>*>(content());
        }

        template <class T>
        operator std::vector<T>() const {
            std::vector<T> res;
            res.reserve(size());
            std::vector<value>& tmp = *std::get<std::vector<value>*>(content());
            for (auto& temp : tmp)
                res.push_back((T)temp);
            return res;
        }

        operator std::unordered_map<std::string, value>() const;
        operator enbt::raw_uuid() const;
        operator std::optional<value>() const;

        std::string convert_to_str() const {
            return operator std::string();
        }

        class const_interator {
        protected:
            enbt::type_id iterate_type;
            void* pointer;

        public:
            const_interator(const value& enbt, bool in_begin = true) {
                iterate_type = enbt.data_type_id;
                switch (enbt.data_type_id.type) {
                case enbt::type::none:
                    pointer = nullptr;
                    break;
                case enbt::type::array:
                case enbt::type::darray:
                    if (in_begin)
                        pointer = new std::vector<value>::iterator(
                            (*(std::vector<value>*)enbt.data).begin()
                        );
                    else
                        pointer = new std::vector<value>::iterator(
                            (*(std::vector<value>*)enbt.data).end()
                        );
                    break;
                case enbt::type::compound:
                    if (in_begin)
                        pointer = new std::unordered_map<std::string, value>::iterator(
                            (*(std::unordered_map<std::string, value>*)enbt.data).begin()
                        );
                    else
                        pointer = new std::unordered_map<std::string, value>::iterator(
                            (*(std::unordered_map<std::string, value>*)enbt.data).end()
                        );
                    break;
                default:
                    throw enbt::exception("Invalid type");
                }
            }

            const_interator(const_interator&& interator) noexcept {
                iterate_type = interator.iterate_type;
                pointer = interator.pointer;
                interator.pointer = nullptr;
            }

            const_interator(const const_interator& interator) {
                iterate_type = interator.iterate_type;
                switch (iterate_type.type) {
                case enbt::type::none:
                    pointer = nullptr;
                    break;
                case enbt::type::array:
                case enbt::type::darray:
                    pointer = new std::vector<value>::iterator(
                        (*(std::vector<value>::iterator*)interator.pointer)
                    );
                    break;
                case enbt::type::compound:
                    pointer = new std::unordered_map<std::string, value>::iterator(
                        (*(std::unordered_map<std::string, value>::iterator*)interator.pointer)
                    );
                    break;
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
            }

            const_interator& operator++() {
                switch (iterate_type.type) {
                case enbt::type::none:
                    break;
                case enbt::type::array:
                case enbt::type::darray:
                    (*(std::vector<value>::iterator*)pointer)++;
                    break;
                case enbt::type::compound:
                    (*(std::unordered_map<std::string, value>::iterator*)pointer)++;
                    break;
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
                return *this;
            }

            const_interator operator++(int) {
                const_interator temp = *this;
                operator++();
                return temp;
            }

            const_interator& operator--() {
                switch (iterate_type.type) {
                case enbt::type::none:
                    break;
                case enbt::type::array:
                case enbt::type::darray:
                    (*(std::vector<value>::iterator*)pointer)--;
                    break;
                case enbt::type::compound:
                    (*(std::unordered_map<std::string, value>::iterator*)pointer)--;
                    break;
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
                return *this;
            }

            const_interator operator--(int) {
                const_interator temp = *this;
                operator--();
                return temp;
            }

            bool operator==(const const_interator& interator) const {
                switch (iterate_type.type) {
                case enbt::type::none:
                    return false;
                case enbt::type::array:
                case enbt::type::darray:
                    return (*(std::vector<value>::iterator*)pointer) == (*(std::vector<value>::iterator*)pointer);
                case enbt::type::compound:
                    return (*(std::unordered_map<std::string, value>::iterator*)pointer) == (*(std::unordered_map<std::string, value>::iterator*)interator.pointer);
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
            }

            bool operator!=(const const_interator& interator) const {
                switch (iterate_type.type) {
                case enbt::type::none:
                    return true;
                case enbt::type::array:
                case enbt::type::darray:
                    return (*(std::vector<value>::iterator*)pointer) != (*(std::vector<value>::iterator*)pointer);
                case enbt::type::compound:
                    return (*(std::unordered_map<std::string, value>::iterator*)pointer) != (*(std::unordered_map<std::string, value>::iterator*)interator.pointer);
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
            }

            std::pair<std::string, const value&> operator*() const {
                switch (iterate_type.type) {
                case enbt::type::none:
                    return {"", value()};
                case enbt::type::array:
                case enbt::type::darray:
                    return {"", *(*(std::vector<value>::iterator*)pointer)};
                case enbt::type::compound: {
                    auto& tmp = (*(std::unordered_map<std::string, value>::iterator*)pointer);
                    return std::pair<std::string, const value&>(
                        tmp->first,
                        tmp->second
                    );
                }
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
            }
        };

        class interator : public const_interator {

        public:
            interator(value& enbt, bool in_begin = true)
                : const_interator(enbt, in_begin) {};
            interator(interator&& interator) noexcept
                : const_interator(interator) {};

            interator(const interator& interator)
                : const_interator(interator) {}

            interator& operator++() {
                const_interator::operator++();
                return *this;
            }

            interator operator++(int) {
                interator temp = *this;
                const_interator::operator++();
                return temp;
            }

            interator& operator--() {
                const_interator::operator--();
                return *this;
            }

            interator operator--(int) {
                interator temp = *this;
                const_interator::operator--();
                return temp;
            }

            bool operator==(const interator& interator) const {
                return const_interator::operator==(interator);
            }

            bool operator!=(const interator& interator) const {
                return const_interator::operator!=(interator);
            }

            std::pair<std::string, value&> operator*() {
                switch (iterate_type.type) {
                case enbt::type::none:
                    throw enbt::exception("Invalid type");
                case enbt::type::array:
                case enbt::type::darray:
                    return {"", *(*(std::vector<value>::iterator*)pointer)};
                case enbt::type::compound: {
                    auto& tmp = (*(std::unordered_map<std::string, value>::iterator*)pointer);
                    return std::pair<std::string, value&>(
                        tmp->first,
                        tmp->second
                    );
                }
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
            }
        };

        class copy_interator {
        protected:
            enbt::type_id iterate_type;
            void* pointer;

        public:
            copy_interator(const value& enbt, bool in_begin = true) {
                iterate_type = enbt.data_type_id;
                switch (enbt.data_type_id.type) {
                case enbt::type::sarray:
                    pointer = const_cast<std::uint8_t*>(enbt.get_internal_ptr());
                    if (!in_begin) {
                        switch (enbt.data_type_id.length) {
                        case type_len::Tiny:
                            pointer = reinterpret_cast<std::uint8_t*>(pointer) + enbt.size();
                            break;
                        case type_len::Short:
                            pointer = reinterpret_cast<std::uint16_t*>(pointer) + enbt.size();
                            break;
                        case type_len::Default:
                            pointer = reinterpret_cast<std::uint32_t*>(pointer) + enbt.size();
                            break;
                        case type_len::Long:
                            pointer = reinterpret_cast<std::uint64_t*>(pointer) + enbt.size();
                            break;
                        }
                    }
                    break;
                case enbt::type::array:
                case enbt::type::darray:
                    if (in_begin)
                        pointer = new std::vector<value>::iterator(
                            (*(std::vector<value>*)enbt.data).begin()
                        );
                    else
                        pointer = new std::vector<value>::iterator(
                            (*(std::vector<value>*)enbt.data).end()
                        );
                    break;
                case enbt::type::compound:
                    if (in_begin)
                        pointer = new std::unordered_map<std::string, value>::iterator(
                            (*(std::unordered_map<std::string, value>*)enbt.data).begin()
                        );
                    else
                        pointer = new std::unordered_map<std::string, value>::iterator(
                            (*(std::unordered_map<std::string, value>*)enbt.data).end()
                        );
                    break;
                default:
                    throw enbt::exception("Invalid type");
                }
            }

            copy_interator(copy_interator&& interator) noexcept {
                iterate_type = interator.iterate_type;
                pointer = interator.pointer;
                interator.pointer = nullptr;
            }

            copy_interator(const copy_interator& interator) {
                iterate_type = interator.iterate_type;
                switch (iterate_type.type) {
                case enbt::type::sarray:
                    pointer = interator.pointer;
                    break;
                case enbt::type::array:
                case enbt::type::darray:
                    pointer = new std::vector<value>::iterator(
                        (*(std::vector<value>::iterator*)interator.pointer)
                    );
                    break;
                case enbt::type::compound:
                    pointer = new std::unordered_map<std::string, value>::iterator(
                        (*(std::unordered_map<std::string, value>::iterator*)interator.pointer)
                    );
                    break;
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
            }

            copy_interator& operator++() {
                switch (iterate_type.type) {
                case enbt::type::sarray:
                    switch (iterate_type.length) {
                    case enbt::type_len::Tiny:
                        pointer = ((std::uint8_t*)pointer) + 1;
                        break;
                    case enbt::type_len::Short:
                        pointer = ((std::uint16_t*)pointer) + 1;
                        break;
                    case enbt::type_len::Default:
                        pointer = ((std::uint32_t*)pointer) + 1;
                        break;
                    case enbt::type_len::Long:
                        pointer = ((std::uint64_t*)pointer) + 1;
                        break;
                    default:
                        break;
                    }
                    break;
                case enbt::type::array:
                case enbt::type::darray:
                    (*(std::vector<value>::iterator*)pointer)++;
                    break;
                case enbt::type::compound:
                    (*(std::unordered_map<std::string, value>::iterator*)pointer)++;
                    break;
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
                return *this;
            }

            copy_interator operator++(int) {
                copy_interator temp = *this;
                operator++();
                return temp;
            }

            copy_interator& operator--() {
                switch (iterate_type.type) {
                case enbt::type::sarray:
                    switch (iterate_type.length) {
                    case enbt::type_len::Tiny:
                        pointer = ((std::uint8_t*)pointer) - 1;
                        break;
                    case enbt::type_len::Short:
                        pointer = ((std::uint16_t*)pointer) - 1;
                        break;
                    case enbt::type_len::Default:
                        pointer = ((std::uint32_t*)pointer) - 1;
                        break;
                    case enbt::type_len::Long:
                        pointer = ((std::uint64_t*)pointer) - 1;
                        break;
                    default:
                        break;
                    }
                    break;
                case enbt::type::array:
                case enbt::type::darray:
                    (*(std::vector<value>::iterator*)pointer)--;
                    break;
                case enbt::type::compound:
                    (*(std::unordered_map<std::string, value>::iterator*)pointer)--;
                    break;
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
                return *this;
            }

            copy_interator operator--(int) {
                copy_interator temp = *this;
                operator--();
                return temp;
            }

            bool operator==(const copy_interator& interator) const {
                if (interator.iterate_type != iterate_type)
                    return false;
                switch (iterate_type.type) {
                case enbt::type::sarray:
                    return pointer == interator.pointer;
                case enbt::type::array:
                case enbt::type::darray:
                    return (*(std::vector<value>::iterator*)pointer) == (*(std::vector<value>::iterator*)pointer);
                case enbt::type::compound:
                    return (*(std::unordered_map<std::string, value>::iterator*)pointer) == (*(std::unordered_map<std::string, value>::iterator*)interator.pointer);
                    break;
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
                return false;
            }

            bool operator!=(const copy_interator& interator) const {
                if (interator.iterate_type != iterate_type)
                    return false;
                switch (iterate_type.type) {
                case enbt::type::sarray:
                    return pointer == interator.pointer;
                case enbt::type::array:
                case enbt::type::darray:
                    return (*(std::vector<value>::iterator*)pointer) != (*(std::vector<value>::iterator*)pointer);
                case enbt::type::compound:
                    return (*(std::unordered_map<std::string, value>::iterator*)pointer) != (*(std::unordered_map<std::string, value>::iterator*)interator.pointer);
                    break;
                default:
                    throw enbt::exception("Unreachable exception in non debug environment");
                }
                return false;
            }

            std::pair<std::string, value> operator*() const;
        };

        const_interator begin() const {
            return const_interator(*this, true);
        }

        const_interator end() const {
            return const_interator(*this, false);
        }

        interator begin() {
            interator test(*this, true);
            return interator(*this, true);
        }

        interator end() {
            return interator(*this, false);
        }

        const_interator cbegin() const {
            return const_interator(*this, true);
        }

        const_interator cend() const {
            return const_interator(*this, false);
        }

        copy_interator copy_begin() const {
            return copy_interator(*this, true);
        }

        copy_interator copy_end() const {
            return copy_interator(*this, false);
        }

        raw_uuid as_uuid() const;
        const std::string& as_string() const;
        std::string& as_string();
        compound_ref as_compound();
        compound_const_ref as_compound() const;
        dynamic_array_ref as_array();
        dynamic_array_ref as_array() const;
        dynamic_array_ref as_dyn_array();
        dynamic_array_ref as_dyn_array() const;
        fixed_array_ref as_fixed_array();
        fixed_array_ref as_fixed_array() const;
        simple_array_ref_ui8 as_ui8_array();
        simple_array_const_ref_ui8 as_ui8_array() const;
        simple_array_ref_ui16 as_ui16_array();
        simple_array_const_ref_ui16 as_ui16_array() const;
        simple_array_ref_ui32 as_ui32_array();
        simple_array_const_ref_ui32 as_ui32_array() const;
        simple_array_ref_ui64 as_ui64_array();
        simple_array_const_ref_ui64 as_ui64_array() const;
        simple_array_ref_i8 as_i8_array();
        simple_array_const_ref_i8 as_i8_array() const;
        simple_array_ref_i16 as_i16_array();
        simple_array_const_ref_i16 as_i16_array() const;
        simple_array_ref_i32 as_i32_array();
        simple_array_const_ref_i32 as_i32_array() const;
        simple_array_ref_i64 as_i64_array();
        simple_array_const_ref_i64 as_i64_array() const;
    };

    inline value to_log_item(const value& val) {
        return value(new value(val), enbt::type_id(enbt::type::log_item, enbt::type_len::Tiny, true), 0, false);
    }

    inline value to_log_item(value&& val) {
        return value(new value(std::move(val)), enbt::type_id(enbt::type::log_item, enbt::type_len::Tiny, false), 0, false);
    }

    inline value from_log_item(const value& val) {
        if (val.get_type() != enbt::type::log_item)
            throw enbt::exception("Invalid type");
        return *(value*)val.data;
    }

    inline value from_log_item(value&& val) {
        if (val.data_type_id.type != enbt::type::log_item)
            throw enbt::exception("Invalid type");
        value res = std::move(*(value*)val.data);
        delete (value*)val.data;
        val.data = nullptr;
        return res;
    }
}

namespace enbt {
    class compound_const_ref {
        friend class compound_ref;
        friend class compound;

    protected:
        std::unordered_map<std::string, value>* proxy;

        compound_const_ref(value& abstract) {
            proxy = std::get<std::unordered_map<std::string, value>*>(abstract.content());
        }

        compound_const_ref(const value& abstract) {
            proxy = std::get<std::unordered_map<std::string, value>*>(abstract.content());
        }

        compound_const_ref() {
            proxy = nullptr;
        }

    public:
        using hasher = std::unordered_map<std::string, value>::hasher;
        using key_type = std::unordered_map<std::string, value>::key_type;
        using mapped_type = std::unordered_map<std::string, value>::mapped_type;
        using key_equal = std::unordered_map<std::string, value>::key_equal;

        using value_type = std::unordered_map<std::string, value>::value_type;
        using allocator_type = std::unordered_map<std::string, value>::allocator_type;
        using size_type = std::unordered_map<std::string, value>::size_type;
        using difference_type = std::unordered_map<std::string, value>::difference_type;
        using pointer = std::unordered_map<std::string, value>::pointer;
        using const_pointer = std::unordered_map<std::string, value>::const_pointer;
        using reference = std::unordered_map<std::string, value>::reference;
        using const_reference = std::unordered_map<std::string, value>::const_reference;
        using iterator = std::unordered_map<std::string, value>::iterator;
        using const_iterator = std::unordered_map<std::string, value>::const_iterator;

        using local_iterator = std::unordered_map<std::string, value>::local_iterator;
        using const_local_iterator = std::unordered_map<std::string, value>::const_local_iterator;

        using insert_return_type = std::unordered_map<std::string, value>::insert_return_type;

        using node_type = std::unordered_map<std::string, value>::node_type;

        compound_const_ref(const compound_const_ref& tag) {
            proxy = tag.proxy;
        }

        compound_const_ref(compound_const_ref&& tag) {
            proxy = tag.proxy;
        }

        const mapped_type& operator[](key_type&& key_val) const {
            return proxy->operator[](std::move(key_val));
        }

        hasher hash_function() const {
            return proxy->hash_function();
        }

        key_equal key_eq() const {
            return proxy->key_eq();
        }

        [[nodiscard]] const mapped_type& at(const key_type& key_value) const {
            return proxy->at(key_value);
        }

        size_type bucket(const std::string& index) const noexcept {
            return proxy->bucket(index);
        }

        [[nodiscard]] size_type bucket_count() const noexcept {
            return proxy->bucket_count();
        }

        size_type bucket_size(size_type bucket) const noexcept {
            return proxy->bucket_size(bucket);
        }

        [[nodiscard]] bool contains(const key_type& key_value) const {
            return proxy->contains(key_value);
        }

        [[nodiscard]] size_type count(const key_type& key_value) const {
            return proxy->count(key_value);
        }

        [[nodiscard]] bool empty() const noexcept {
            return proxy->empty();
        }

        std::pair<const_iterator, const_iterator> equal_range(const key_type& key_value) const {
            return proxy->equal_range(key_value);
        }

        [[nodiscard]] decltype(auto) find(const key_type& key_value) const {
            return proxy->find(key_value);
        }

        [[nodiscard]] float load_factor() const noexcept {
            return proxy->load_factor();
        }

        [[nodiscard]] size_type max_bucket_count() const noexcept {
            return proxy->max_bucket_count();
        }

        [[nodiscard]] auto max_load_factor() const noexcept {
            return proxy->max_load_factor();
        }

        [[nodiscard]] size_type max_size() const noexcept {
            return proxy->max_size();
        }

        [[nodiscard]] size_type size() const noexcept {
            return proxy->size();
        }

        [[nodiscard]] const_iterator begin() const {
            return proxy->begin();
        }

        [[nodiscard]] const_iterator end() const {
            return proxy->end();
        }

        [[nodiscard]] const_iterator cbegin() const {
            return proxy->cbegin();
        }

        [[nodiscard]] const_iterator cend() const {
            return proxy->cend();
        }
    };

    class compound_ref : public compound_const_ref {
    protected:
        compound_ref(value& abstract) {
            proxy = std::get<std::unordered_map<std::string, value>*>(abstract.content());
        }

        compound_ref(const value& abstract) {
            proxy = std::get<std::unordered_map<std::string, value>*>(abstract.content());
        }

        compound_ref() {
            proxy = nullptr;
        }

    public:
        [[nodiscard]] static compound_ref make_ref(value& enbt) {
            if (enbt.is_compound())
                return compound_ref(enbt);
            else
                throw enbt::exception("value is not a compound");
        }

        [[nodiscard]] static compound_const_ref make_ref(const value& enbt) {
            if (enbt.is_compound())
                return compound_const_ref(enbt);
            else
                throw enbt::exception("value is not a compound");
        }

        compound_ref(const compound_ref& tag) {
            proxy = tag.proxy;
        }

        compound_ref(compound_ref&& tag) {
            proxy = tag.proxy;
        }

        mapped_type& operator[](key_type&& key_val) {
            return proxy->operator[](std::move(key_val));
        }

        void swap(std::unordered_map<std::string, value>& swap_value) {
            proxy->swap(swap_value);
        }

        [[nodiscard]] compound_ref merge(const value& copy) &;
        [[nodiscard]] compound_ref merge(value&& move) &;
        [[nodiscard]] compound_ref merge(const std::unordered_map<std::string, value>& copy) &;
        [[nodiscard]] compound_ref merge(std::unordered_map<std::string, value>&& move) &;

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

        void reserve(size_type max_count) {
            proxy->reserve(max_count);
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

        [[nodiscard]] mapped_type& operator[](const key_type& key_value) {
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

        void clear() noexcept {
            proxy->clear();
        }

        template <class... Values>
        std::pair<iterator, bool> emplace(Values&&... vals) {
            return proxy->emplace(std::forward<Values>(vals)...);
        }

        template <class... Values>
        decltype(auto) emplace(const_iterator where, Values&&... vals) {
            return proxy->emplace_hint(where, std::forward<Values>(vals)...);
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

        [[nodiscard]] node_type extract(const key_type& key_value) {
            return proxy->extract(key_value);
        }

        [[nodiscard]] decltype(auto) find(const key_type& key_value) {
            return proxy->find(key_value);
        }

        [[nodiscard]] decltype(auto) find(const key_type& key_value) const {
            return proxy->find(key_value);
        }

        [[nodiscard]] iterator begin() {
            return proxy->begin();
        }

        [[nodiscard]] iterator end() {
            return proxy->end();
        }

        [[nodiscard]] const_iterator begin() const {
            return proxy->begin();
        }

        [[nodiscard]] const_iterator end() const {
            return proxy->end();
        }
    };

    class fixed_array_ref {
    protected:
        enbt::type_id fixed_type = enbt::type::none;
        std::vector<value>* proxy;
        bool as_const = false;

        friend class fixed_array;

        fixed_array_ref(value& abstract) {
            proxy = std::get<std::vector<value>*>(abstract.content());
            as_const = false;
        }

        fixed_array_ref(const value& abstract) {
            proxy = std::get<std::vector<value>*>(abstract.content());
            as_const = true;
        }

        fixed_array_ref() {
            proxy = nullptr;
        }

    public:
        [[nodiscard]] static fixed_array_ref make_ref(value& enbt) {
            if (enbt.get_type() == enbt::type::array)
                return fixed_array_ref(enbt);
            else if (enbt.get_type() == enbt::type::darray) {
                if (enbt.size()) {
                    enbt::type_id tid_check = enbt[0].type_id();
                    for (auto& check : *std::get<std::vector<value>*>(enbt.content()))
                        if (!check.type_equal(tid_check))
                            throw enbt::exception("value is not a fixed array");
                }
                enbt.data_type_id.type = enbt::type::array;
                return fixed_array_ref(enbt);
            } else
                throw enbt::exception("value is not a fixed array");
        }

        [[nodiscard]] static fixed_array_ref make_ref(const value& enbt) {
            if (enbt.get_type() == enbt::type::array)
                return fixed_array_ref(enbt);
            else if (enbt.get_type() == enbt::type::darray) {
                if (enbt.size()) {
                    enbt::type_id tid_check = enbt[0].type_id();
                    for (auto& check : *std::get<std::vector<value>*>(enbt.content()))
                        if (!check.type_equal(tid_check))
                            throw enbt::exception("value is not a fixed array");
                }
                return fixed_array_ref(enbt);
            } else
                throw enbt::exception("value is not a fixed array");
        }

        using value_type = value;
        using allocator_type = std::vector<value>::allocator_type;
        using pointer = std::vector<value>::pointer;
        using const_pointer = std::vector<value>::const_pointer;
        using reference = value&;
        using const_reference = const value&;
        using size_type = std::vector<value>::size_type;
        using difference_type = std::vector<value>::difference_type;
        using iterator = std::vector<value>::iterator;
        using const_iterator = std::vector<value>::const_iterator;
        using reverse_iterator = std::vector<value>::reverse_iterator;
        using const_reverse_iterator = std::vector<value>::const_reverse_iterator;

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
        void set(std::size_t index, T&& _value) {
            if (as_const)
                throw enbt::exception("This array is constant");
            value to_set(std::forward<T>(_value));

            if (index != 0 && fixed_type.type != enbt::type::none) {
                if (to_set.type_id() != fixed_type) {
                    if (to_set.get_type() == fixed_type.type) {
                        switch (fixed_type.type) {
                        case enbt::type::integer:
                        case enbt::type::var_integer:
                        case enbt::type::comp_integer:
                        case enbt::type::floating:
                        case enbt::type::string:
                            (*proxy)[index] = std::move(to_set.cast_to(fixed_type));
                            return;
                        default:
                            break;
                        }
                    }
                    throw enbt::exception("Invalid set value, set value must be same as every item in fixed array");
                }
            }
            fixed_type = to_set.type_id();
            (*proxy)[index] = std::move(to_set);
        }

        template <class T>
        void push_back(T&& _value) {
            if (as_const)
                throw enbt::exception("This array is constant");
            value to_set(std::forward<T>(_value));


            if ((*proxy).size() != 0 && fixed_type.type != enbt::type::none) {
                if (to_set.type_id() != fixed_type)
                    throw enbt::exception("Invalid set value, set value must be same as every item in fixed array");
            }
            fixed_type = to_set.type_id();
            (*proxy).push_back(std::move(to_set));
        }

        void pop_back() {
            if (as_const)
                throw enbt::exception("This array is constant");
            (*proxy).pop_back();
        }

        [[nodiscard]] const value& operator[](std::size_t index) const noexcept {
            return (*proxy)[index];
        }

        [[nodiscard]] const value& at(size_type index) const {
            return proxy->at(index);
        }

        void remove(std::size_t index) {
            proxy->erase(proxy->begin() + index);
        }

        void reserve(std::size_t count) {
            proxy->reserve(count);
        }

        [[nodiscard]] std::size_t size() const {
            return proxy->size();
        }

        [[nodiscard]] bool empty() const {
            return proxy->empty();
        }

        [[nodiscard]] const_iterator begin() const {
            return proxy->cbegin();
        }

        [[nodiscard]] const_iterator end() const {
            return proxy->cend();
        }

        [[nodiscard]] const_iterator cbegin() const {
            return proxy->cbegin();
        }

        [[nodiscard]] const_iterator cend() const {
            return proxy->cend();
        }

        [[nodiscard]] const_reverse_iterator rbegin() const {
            return proxy->crbegin();
        }

        [[nodiscard]] const_reverse_iterator rend() const {
            return proxy->crend();
        }

        [[nodiscard]] const_reverse_iterator crbegin() const {
            return proxy->crbegin();
        }

        [[nodiscard]] const_reverse_iterator crend() const {
            return proxy->crend();
        }

        [[nodiscard]] size_t max_size() const noexcept {
            return proxy->max_size();
        }

        [[nodiscard]] const value& front() const {
            return proxy->front();
        }

        [[nodiscard]] const value& back() const {
            return proxy->back();
        }
    };

    class dynamic_array_ref {
    protected:
        std::vector<value>* proxy;
        friend class dynamic_array;

        dynamic_array_ref(const value& abstract) {
            proxy = std::get<std::vector<value>*>(abstract.content());
        }

        dynamic_array_ref() {
            proxy = nullptr;
        }

    public:
        [[nodiscard]] static dynamic_array_ref make_ref(value& enbt) {
            if (enbt.get_type() == enbt::type::darray)
                return dynamic_array_ref(enbt);
            else if (enbt.get_type() == enbt::type::array) {
                enbt.data_type_id.type = enbt::type::darray;
                return dynamic_array_ref(enbt);
            } else
                throw enbt::exception("value is not a dynamic array");
        }

        [[nodiscard]] static const dynamic_array_ref make_ref(const value& enbt) {
            if (enbt.get_type() == enbt::type::darray)
                return dynamic_array_ref(enbt);
            else if (enbt.get_type() == enbt::type::array)
                return dynamic_array_ref(enbt);
            else
                throw enbt::exception("value is not a dynamic array");
        }

        using value_type = value;
        using allocator_type = std::vector<value>::allocator_type;
        using pointer = std::vector<value>::pointer;
        using const_pointer = std::vector<value>::const_pointer;
        using reference = value&;
        using const_reference = const value&;
        using size_type = std::vector<value>::size_type;
        using difference_type = std::vector<value>::difference_type;
        using iterator = std::vector<value>::iterator;
        using const_iterator = std::vector<value>::const_iterator;
        using reverse_iterator = std::vector<value>::reverse_iterator;
        using const_reverse_iterator = std::vector<value>::const_reverse_iterator;

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
        void push_back(T&& _value) {
            value to_set(std::forward<T>(_value));
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

        iterator insert(const_iterator where, const value& value) {
            return proxy->insert(where, value);
        }

        iterator insert(const_iterator where, value&& value) {
            return proxy->insert(where, std::move(value));
        }

        iterator insert(const_iterator where, size_type count, const value& value) {
            return proxy->insert(where, count, value);
        }

        void pop_back() {
            proxy->pop_back();
        }

        void resize(size_type siz) {
            proxy->resize(siz);
        }

        void resize(size_type siz, const value& def_init) {
            proxy->resize(siz, def_init);
        }

        [[nodiscard]] size_type size() const {
            return proxy->size();
        }

        [[nodiscard]] bool empty() const {
            return proxy->empty();
        }

        void assign(size_type new_size, const value& val) {
            proxy->assign(new_size, val);
        }

        void assign(std::initializer_list<value> list) {
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

        void swap(std::vector<value>& another) {
            proxy->swap(another);
        }

        void swap(dynamic_array_ref& another) {
            proxy->swap(*another.proxy);
        }

        [[nodiscard]] value* data() {
            return proxy->data();
        }

        [[nodiscard]] const value* data() const {
            return proxy->data();
        }

        [[nodiscard]] iterator begin() {
            return proxy->begin();
        }

        [[nodiscard]] iterator end() {
            return proxy->end();
        }

        [[nodiscard]] const_iterator begin() const {
            return proxy->cbegin();
        }

        [[nodiscard]] const_iterator end() const {
            return proxy->cend();
        }

        [[nodiscard]] const_iterator cbegin() const {
            return proxy->cbegin();
        }

        [[nodiscard]] const_iterator cend() const {
            return proxy->cend();
        }

        [[nodiscard]] reverse_iterator rbegin() {
            return proxy->rbegin();
        }

        [[nodiscard]] reverse_iterator rend() {
            return proxy->rend();
        }

        [[nodiscard]] const_reverse_iterator rbegin() const {
            return proxy->crbegin();
        }

        [[nodiscard]] const_reverse_iterator rend() const {
            return proxy->crend();
        }

        [[nodiscard]] const_reverse_iterator crbegin() const {
            return proxy->crbegin();
        }

        [[nodiscard]] const_reverse_iterator crend() const {
            return proxy->crend();
        }

        [[nodiscard]] std::size_t max_size() const noexcept {
            return proxy->max_size();
        }

        [[nodiscard]] value& operator[](size_type index) noexcept {
            return proxy->operator[](index);
        }

        [[nodiscard]] const value& operator[](size_type index) const noexcept {
            return proxy->operator[](index);
        }

        [[nodiscard]] value& at(size_type index) {
            return proxy->at(index);
        }

        [[nodiscard]] const value& at(size_type index) const {
            return proxy->at(index);
        }

        [[nodiscard]] const value& front() const {
            return proxy->front();
        }

        [[nodiscard]] value& front() {
            return proxy->front();
        }

        [[nodiscard]] const value& back() const {
            return proxy->back();
        }

        [[nodiscard]] value& back() {
            return proxy->back();
        }
    };

    template <class T>
    class simple_array;

    template <class T>
    class simple_array_const_ref {
    protected:
        T* proxy;
        std::size_t size_;

        friend class simple_array<T>;

        static constexpr enbt::type_len type_len = []() constexpr {
            if constexpr (sizeof(T) == 1)
                return enbt::type_len::Tiny;
            else if constexpr (sizeof(T) == 2)
                return enbt::type_len::Short;
            else if constexpr (sizeof(T) == 4)
                return enbt::type_len::Default;
            else
                return enbt::type_len::Long;
        }();

        simple_array_const_ref(const value& abstract) {
            proxy = std::get<T*>(abstract.content());
            size_ = abstract.size();
        }

    public:
        [[nodiscard]] static simple_array_const_ref make_ref(const value& enbt) {
            auto ty = enbt.type_id();
            if (ty.type == enbt::type::sarray && ty.length == type_len && ty.is_signed == std::is_signed_v<T>)
                return simple_array_const_ref(enbt);
            else
                throw enbt::exception("value is not a simple array or not same");
        }

        using pointer = value*;
        using const_pointer = const value*;
        using reference = value&;
        using const_reference = const value&;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using iterator = value*;
        using const_iterator = const value*;
        using reverse_iterator = std::vector<value>::reverse_iterator;
        using const_reverse_iterator = std::vector<value>::const_reverse_iterator;

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

        [[nodiscard]] T operator[](std::size_t index) const noexcept {
            return proxy[index];
        }

        [[nodiscard]] T at(std::size_t index) const {
            if (size_ >= index)
                throw std::out_of_range("invalid simple_array index");
            return proxy[index];
        }

        [[nodiscard]] std::size_t size() const {
            return size_;
        }

        [[nodiscard]] const T* begin() const {
            return proxy;
        }

        [[nodiscard]] const T* end() const {
            return proxy + size_;
        }

        [[nodiscard]] const T* cbegin() const {
            return proxy;
        }

        [[nodiscard]] const T* cend() const {
            return proxy + size_;
        }

        [[nodiscard]] const T* rbegin() const {
            return proxy + size_;
        }

        [[nodiscard]] const T* rend() const {
            return proxy;
        }

        [[nodiscard]] const T* crbegin() const {
            return proxy + size_;
        }

        [[nodiscard]] const T* crend() const {
            return proxy;
        }
    };

    template <class T>
    class simple_array_ref {
    protected:
        T* proxy;
        std::size_t size_;

        friend class simple_array<T>;

        static constexpr enbt::type_len type_len = []() constexpr {
            if constexpr (sizeof(T) == 1)
                return enbt::type_len::Tiny;
            else if constexpr (sizeof(T) == 2)
                return enbt::type_len::Short;
            else if constexpr (sizeof(T) == 4)
                return enbt::type_len::Default;
            else
                return enbt::type_len::Long;
        }();

        simple_array_ref(value& abstract) {
            proxy = std::get<T*>(abstract.content());
            size_ = abstract.size();
        }

        simple_array_ref() {
            proxy = nullptr;
            size_++;
        }

    public:
        [[nodiscard]] static simple_array_ref make_ref(value& enbt) {
            auto ty = enbt.type_id();
            if (ty.type == enbt::type::sarray && ty.length == type_len && ty.is_signed == std::is_signed_v<T>)
                return simple_array_ref(enbt);
            else
                throw enbt::exception("value is not a simple array or not same");
        }

        [[nodiscard]] static simple_array_const_ref<T> make_ref(const value& enbt) {
            return simple_array_const_ref<T>::make_ref(enbt);
        }

        using pointer = value*;
        using const_pointer = const value*;
        using reference = value&;
        using const_reference = const value&;
        using size_type = std::size_t;
        using difference_type = ptrdiff_t;
        using iterator = value*;
        using const_iterator = const value*;
        using reverse_iterator = std::vector<value>::reverse_iterator;
        using const_reverse_iterator = std::vector<value>::const_reverse_iterator;

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

        [[nodiscard]] T& operator[](std::size_t index) noexcept {
            return proxy[index];
        }

        [[nodiscard]] T operator[](std::size_t index) const noexcept {
            return proxy[index];
        }

        [[nodiscard]] T at(std::size_t index) const {
            if (size_ >= index)
                throw std::out_of_range("invalid simple_array index");
            return proxy[index];
        }

        [[nodiscard]] T& at(std::size_t index) {
            if (size_ >= index)
                throw std::out_of_range("invalid simple_array index");
            return proxy[index];
        }

        [[nodiscard]] std::size_t size() const {
            return size_;
        }

        [[nodiscard]] T* begin() {
            return proxy;
        }

        [[nodiscard]] T* end() {
            return proxy + size_;
        }

        [[nodiscard]] const T* begin() const {
            return proxy;
        }

        [[nodiscard]] const T* end() const {
            return proxy + size_;
        }

        [[nodiscard]] const T* cbegin() const {
            return proxy;
        }

        [[nodiscard]] const T* cend() const {
            return proxy + size_;
        }

        [[nodiscard]] T* rbegin() {
            return proxy + size_;
        }

        [[nodiscard]] T* rend() {
            return proxy;
        }

        [[nodiscard]] const T* rbegin() const {
            return proxy + size_;
        }

        [[nodiscard]] const T* rend() const {
            return proxy;
        }

        [[nodiscard]] const T* crbegin() const {
            return proxy + size_;
        }

        [[nodiscard]] const T* crend() const {
            return proxy;
        }
    };

    class compound : public compound_ref {
        value holder;

    public:
        compound()
            : holder(std::unordered_map<std::string, value>()) {
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
        }

        compound(const compound& copy) {
            holder = copy.holder;
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
        }

        compound(compound&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
        }

        compound(std::unordered_map<std::string, value>&& init)
            : holder(init) {
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
        }

        compound(const std::unordered_map<std::string, value>& init)
            : holder(init) {
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
        }

        compound(std::initializer_list<std::pair<const std::string, value>> init)
            : holder(std::unordered_map<std::string, value>(init)) {
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
        }

        compound(compound_ref ref)
            : holder(*ref.proxy) {
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
        }

        compound(compound_const_ref ref)
            : holder(*ref.proxy) {
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
        }

        compound& operator=(const value& copy) {
            if (!copy.is_compound())
                throw enbt::exception("value is not a compound");
            holder = copy;
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
            return *this;
        }

        compound& operator=(value&& copy) {
            if (!copy.is_compound())
                throw enbt::exception("value is not a compound");
            holder = std::move(copy);
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
            return *this;
        }

        compound& operator=(const compound& copy) {
            holder = copy.holder;
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
            return *this;
        }

        compound& operator=(compound&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::unordered_map<std::string, value>*>(holder.content());
            return *this;
        }

        [[nodiscard]] bool operator==(const compound& tag) const {
            return holder == tag.holder;
        }

        [[nodiscard]] bool operator!=(const compound& tag) const {
            return holder != tag.holder;
        }

        [[nodiscard]] compound merge(const value& copy) &&;
        [[nodiscard]] compound merge(value&& move) &&;
        [[nodiscard]] compound merge(const std::unordered_map<std::string, value>& copy) &&;
        [[nodiscard]] compound merge(std::unordered_map<std::string, value>&& move) &&;

        [[nodiscard]] explicit operator value&() & {
            return holder;
        }

        [[nodiscard]] explicit operator const value&() const& {
            return holder;
        }

        [[nodiscard]] explicit operator value&&() && {
            return std::move(holder);
        }
    };

    class fixed_array : public fixed_array_ref {
        value holder;

    public:
        template <class T>
        fixed_array(std::initializer_list<T> list)
            : fixed_array(list.size()) {
            std::size_t i = 0;
            for (auto& item : list)
                (*proxy)[i++] = item;
        }

        fixed_array()
            : holder(std::vector<value>(), enbt::type::array) {
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        fixed_array(std::size_t array_size)
            : holder(std::vector<value>(array_size), enbt::type::array) {
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        fixed_array(const fixed_array& copy) {
            holder = copy.holder;
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        fixed_array(fixed_array&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        fixed_array(fixed_array_ref ref)
            : holder(*ref.proxy, ref.fixed_type) {
            proxy = std::get<std::vector<value>*>(holder.content());
            fixed_type = ref.fixed_type;
        }

        fixed_array& operator=(const fixed_array& copy) {
            holder = copy.holder;
            proxy = std::get<std::vector<value>*>(holder.content());
            return *this;
        }

        fixed_array& operator=(fixed_array&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::vector<value>*>(holder.content());
            return *this;
        }

        [[nodiscard]] bool operator==(const fixed_array& tag) const {
            return holder == tag.holder;
        }

        [[nodiscard]] bool operator!=(const fixed_array& tag) const {
            return holder != tag.holder;
        }

        [[nodiscard]] explicit operator value&() & {
            return holder;
        }

        [[nodiscard]] explicit operator const value&() const& {
            return holder;
        }

        [[nodiscard]] explicit operator value&&() && {
            return std::move(holder);
        }
    };

    class dynamic_array : public dynamic_array_ref {
        value holder;

    public:
        template <class T>
        dynamic_array(std::initializer_list<T> list)
            : dynamic_array() {
            std::size_t i = 0;
            for (auto& item : list)
                (*proxy)[i++] = item;
        }

        template <std::size_t arr_size>
        dynamic_array(const value arr[arr_size])
            : dynamic_array(arr, arr + arr_size) {}

        dynamic_array(const value* begin, const value* end)
            : dynamic_array() {
            *proxy = std::vector<value>(begin, end);
        }

        dynamic_array()
            : holder(std::vector<value>(), enbt::type::darray) {
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        dynamic_array(const std::vector<value>& arr)
            : holder(arr, enbt::type::darray) {
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        dynamic_array(std::vector<value>&& arr)
            : holder(std::move(arr), enbt::type::darray) {
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        dynamic_array(const dynamic_array& copy) {
            holder = copy.holder;
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        dynamic_array(dynamic_array&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        dynamic_array(dynamic_array_ref ref)
            : holder(*ref.proxy) {
            proxy = std::get<std::vector<value>*>(holder.content());
        }

        dynamic_array& operator=(const value& copy) {
            if (copy.get_type() != enbt::type::darray)
                throw enbt::exception("value is not a dynamic array");
            holder = copy;
            proxy = std::get<std::vector<value>*>(holder.content());
            return *this;
        }

        dynamic_array& operator=(value&& copy) {
            if (copy.get_type() != enbt::type::darray)
                throw enbt::exception("value is not a dynamic array");
            holder = std::move(copy);
            proxy = std::get<std::vector<value>*>(holder.content());
            return *this;
        }

        dynamic_array& operator=(const dynamic_array& copy) {
            holder = copy.holder;
            proxy = std::get<std::vector<value>*>(holder.content());
            return *this;
        }

        dynamic_array& operator=(dynamic_array&& move) {
            holder = std::move(move.holder);
            proxy = std::get<std::vector<value>*>(holder.content());
            return *this;
        }

        [[nodiscard]] bool operator==(const dynamic_array& tag) const {
            return holder == tag.holder;
        }

        [[nodiscard]] bool operator!=(const dynamic_array& tag) const {
            return holder != tag.holder;
        }

        [[nodiscard]] explicit operator value&() & {
            return holder;
        }

        [[nodiscard]] explicit operator const value&() const& {
            return holder;
        }

        [[nodiscard]] explicit operator value&&() && {
            return std::move(holder);
        }
    };

    template <class T>
    class simple_array : public simple_array_ref<T> {
        value holder;

    public:
        static constexpr enbt::type_id enbt_type = enbt::type_id(enbt::type::sarray, simple_array_ref<T>::type_len, std::is_signed_v<T>);

        simple_array(std::initializer_list<T> list)
            : simple_array(list.size()) {
            std::size_t i = 0;
            for (auto& item : list)
                simple_array_ref<T>::proxy[i++] = item;
        }

        template <size_t arr_size>
        simple_array(const T (&arr)[arr_size])
            : simple_array(arr, arr + arr_size) {}

        simple_array(const T* begin, const T* end)
            : simple_array(size_t(end - begin)) {
            std::size_t i = 0;
            for (auto it = begin; it != end; ++it)
                simple_array_ref<T>::proxy[i++] = *it;
        }

        simple_array(std::size_t array_size)
            : holder(enbt_type, array_size) {
            simple_array_ref<T>::proxy = std::get<T*>(holder.content());
            simple_array_ref<T>::size_ = holder.size();
        }

        simple_array(const simple_array& copy)
            : holder(copy.holder) {
            simple_array_ref<T>::proxy = std::get<T*>(holder.content());
            simple_array_ref<T>::size_ = holder.size();
        }

        simple_array(simple_array&& move)
            : holder(std::move(move.holder)) {
            simple_array_ref<T>::proxy = std::get<T*>(holder.content());
            simple_array_ref<T>::size_ = holder.size();
        }

        simple_array(simple_array_ref<T> ref)
            : holder(*ref.proxy, ref.size_) {
            simple_array_ref<T>::proxy = std::get<std::vector<value>*>(holder.content());
            simple_array_ref<T>::size_ = ref.size_;
        }

        simple_array(simple_array_const_ref<T> ref)
            : holder(*ref.proxy, ref.size_) {
            simple_array_ref<T>::proxy = std::get<std::vector<value>*>(holder.content());
            simple_array_ref<T>::size_ = ref.size_;
        }

        simple_array& operator=(const simple_array& copy) {
            holder = copy.holder;
            simple_array_ref<T>::proxy = std::get<T*>(holder.content());
            simple_array_ref<T>::size_ = holder.size();

            return *this;
        }

        simple_array& operator=(simple_array&& move) {
            holder = std::move(move.holder);
            simple_array_ref<T>::proxy = std::get<T*>(holder.content());
            simple_array_ref<T>::size_ = holder.size();
            return *this;
        }

        [[nodiscard]] bool operator==(const simple_array& tag) const {
            return holder == tag.holder;
        }

        [[nodiscard]] bool operator!=(const simple_array& tag) const {
            return holder != tag.holder;
        }

        [[nodiscard]] explicit operator value&() & {
            return holder;
        }

        [[nodiscard]] explicit operator const value&() const& {
            return holder;
        }

        [[nodiscard]] explicit operator value&&() && {
            return std::move(holder);
        }
    };

    using simple_array_ref_ui8 = simple_array_ref<std::uint8_t>;
    using simple_array_ref_ui16 = simple_array_ref<std::uint16_t>;
    using simple_array_ref_ui32 = simple_array_ref<std::uint32_t>;
    using simple_array_ref_ui64 = simple_array_ref<std::uint64_t>;
    using simple_array_ref_i8 = simple_array_ref<std::int8_t>;
    using simple_array_ref_i16 = simple_array_ref<std::int16_t>;
    using simple_array_ref_i32 = simple_array_ref<std::int32_t>;
    using simple_array_ref_i64 = simple_array_ref<std::int64_t>;

    using simple_array_ui8 = simple_array<std::uint8_t>;
    using simple_array_ui16 = simple_array<std::uint16_t>;
    using simple_array_ui32 = simple_array<std::uint32_t>;
    using simple_array_ui64 = simple_array<std::uint64_t>;
    using simple_array_i8 = simple_array<std::int8_t>;
    using simple_array_i16 = simple_array<std::int16_t>;
    using simple_array_i32 = simple_array<std::int32_t>;
    using simple_array_i64 = simple_array<std::int64_t>;

    class bit {
        value holder = value(false);

    public:
        bit() = default;

        bit(const value& abstract) {
            if (abstract.get_type() == enbt::type::bit)
                holder = abstract;
        }

        bit(value&& abstract) {
            if (abstract.get_type() == enbt::type::bit)
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

        bit& operator=(bool _value) {
            holder = value(_value);
            return *this;
        }

        operator bool() const {
            return holder;
        }

        [[nodiscard]] explicit operator value&() & {
            return holder;
        }

        [[nodiscard]] explicit operator const value&() const& {
            return holder;
        }

        [[nodiscard]] explicit operator value&&() && {
            return std::move(holder);
        }
    };

    class optional {
        value holder = value((value*)nullptr, enbt::type_id(enbt::type::optional, enbt::type_len::Tiny, false), 0, false);

    public:
        optional() = default;

        optional(const value& abstract) {
            if (abstract.get_type() == enbt::type::optional)
                holder = abstract;
        }

        optional(value&& abstract) {
            if (abstract.get_type() == enbt::type::optional)
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
            holder = value((value*)nullptr, enbt::type_id(enbt::type::optional, enbt::type_len::Tiny, false), 0, false);
        }

        [[nodiscard]] value& get() {
            if (auto res = holder.get_optional(); res != nullptr)
                return *res;
            else
                throw enbt::exception("Optional is empty");
        }

        [[nodiscard]] const value& get() const {
            if (auto res = holder.get_optional(); res != nullptr)
                return *res;
            else
                throw enbt::exception("Optional is empty");
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

        [[nodiscard]] explicit operator value&() & {
            return holder;
        }

        [[nodiscard]] explicit operator const value&() const& {
            return holder;
        }

        [[nodiscard]] explicit operator value&&() && {
            return std::move(holder);
        }
    };

    class uuid {
        value holder = value(enbt::raw_uuid());

    public:
        uuid() = default;

        uuid(const value& abstract) {
            if (abstract.get_type() == enbt::type::uuid)
                holder = abstract;
        }

        uuid(value&& abstract) {
            if (abstract.get_type() == enbt::type::uuid)
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

        uuid& operator=(enbt::raw_uuid value) {
            holder = value;
            return *this;
        }

        operator enbt::raw_uuid() const {
            return std::get<enbt::raw_uuid>(holder.content());
        }

        [[nodiscard]] explicit operator value&() & {
            return holder;
        }

        [[nodiscard]] explicit operator const value&() const& {
            return holder;
        }

        [[nodiscard]] explicit operator value&&() && {
            return std::move(holder);
        }
    };
}

namespace std {
    template <>
    struct hash<enbt::raw_uuid> {
        std::size_t operator()(const enbt::raw_uuid& uuid) const {
            std::uint64_t parts[2];
            std::memcpy(&parts[0], uuid.data, 8);
            std::memcpy(&parts[1], uuid.data + 8, 8);
            return std::hash<std::uint64_t>()(parts[0]) ^ std::hash<std::uint64_t>()(parts[1]);
        }
    };
}

#endif /* ENBT */


// value Example file
///////                       type (1 byte)                         compound len
//	[enbt::type_id{ type=compound, length=tiny }][5 (1byte unsigned)]
//		[(6)]["sarray"][{ type=string }][(7)]["Мир!"]
//		[(7)]["integer"][{ type=integer, length=long, is_signed=true }][-9223372036854775808]
//		[(4)]["uuid"][{ type=uuid }][0xD55F0C3556DA165E6F512203C78B57FF]
//		[(6)]["darray"][{ type=darray, length=tiny, is_signed=false }] [3]
//			[{ type=string }][(307)]["\"Pijamalı hasta yağız şoföre çabucak güvendi\"\"Victor jagt zwölf Boxkämpfer quer über den großen Sylter Deich\"\"съешь ещё этих мягких французских булок, да выпей чаю.\"\"Pranzo d'acqua fa volti sghembi\"\"Stróż pchnął kość w quiz gędźb vel fax myjń\"\"]
// 			[{ type=integer, length=long, is_signed=true }][9223372036854775807]
//			[{ type=uuid}][0xD55F0C3556DA165E6F512203C78B57FF]
//
