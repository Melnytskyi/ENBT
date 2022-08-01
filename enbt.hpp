#pragma once
#ifndef _ENBT_NO_REDEFINE_
#define _ENBT_NO_REDEFINE_
#include <unordered_map>
#include <string>
#include <istream>
#include <bit>
#include <any>
#include <variant>
#include <boost/uuid/detail/md5.hpp>
#define ENBT_VERSION_HEX 0x11
#define ENBT_VERSION_STR 1.1

//enchanted named binary tag
class EnbtException : public std::exception {
public:
	EnbtException(std::string&& reason) :std::exception(reason.c_str()) {}
	EnbtException(const char* reason) :std::exception(reason) {}
	EnbtException() :std::exception("EnbtException") {}
};

//TO-DO
//
// add support for logging
// bug fixes
// ....


class ENBT {
public:
	struct version {      //current 1.1, max 15.15
		uint8_t Major : 4;//0x01
		uint8_t Minor : 4;//0x01
	};


	typedef boost::uuids::uuid UUID;
	//version 1.0
	//file structure for ENBT:
	//	version[1byte]  (0x0F -> 1.x   0xF0   -> x.0)(max 15.15)
	//	value (ex: [Type_ID][type_define][type_data]
	// 
	//file structure for logh: // log headers
	//	[(string len)]{string}
	//	[(string len)]{string}
	//  ...
	
	struct Type_ID {
		enum class Endian : uint64_t {
			little,big,
			native = (unsigned)std::endian::native
		};
		enum class Type : uint64_t {
			none,    //[0byte]
			integer,
			floating,

			ex_integer,
				//tiny encoded
				// [(00XXXXXX)], [(01XXXXXX XXXXXXXX)], etc... 00 - 1 byte,01 - 2 byte,10 - 4 byte,11 - 8 byte
				//short is not implemented
				//default is var int
				//long is var long

			uuid,	//[16byte]
				//tiny -> 16 byte uuid/integer
				// 
				//TO-DO
				//short -> 32 byte key/integer
				//default -> 64 byte key/integer
				//long -> 128 byte key/integer

			string, 

			compound,	 
				// else will be used default string but 'string len' encoded as big endian which last 2 bits used as define 'string len' bytes len, 
				//[len][... items]   ((named items list))
				//					item [(len)][chars] {type_id 1byte} (value_define_and_data)

			array,
				//	[len][... items]   ((unnamed items list))
				//		item {type_id 1byte} (value_define_and_data)
				// 
				//if signed
				// 
				//	[len][type_id 1byte]{... items} /unnamed items array, if contain static value size reader can get one element without readding all elems[int,double,e.t.c..]
				//		item {value_define_and_data}

			structure, //[total types] [type_id ...] {type defies}
			optional,// 'any value'     (contain value if is_signed == true)  /example [optional,unsigned]
			//	 				 											  /example [optional,signed][utf8_str,tiny][3]{"Yay"}
			bit,//[0byte] bit in is_signed flag from Type_id byte
			//TO-DO
			//vector,		 //[len][type_id 1byte][items define]{... items} /example [vector,tiny][6][sarray,tiny][3]{"WoW"}{"YaY"}{"it\0"}{"is\0"}{"god"}{"!\0\0"}
			//					item {value_data}
			// 
			// 
			//
			__RESERVED_TO_IMPLEMENT_VECTOR__,

			//TO-DO
			log,
			//[(len)] { struct {} }
			//Length is mode enum
			//0[(fi)]{}
			//1[(fi)][(sb)]{}
			//2[(fi)][(sb)]{}[(sb)]
			//3   same as 2
			//fi - format index, exists only if signed, else list of types
			//sb - skip bytes for iterate



			unused0,
			unused1,
			//if domain is signed then that is named domain, else then that use local id domain
			//recomended use local id domain only when need to use in one program or product
			//if planed sharing with world, use named domain
			//named domain use utf8
			domain
		};
		enum class LenType : uint64_t {// array string, e.t.c. length always litle endian
			Tiny,
			Short,
			Default,
			Long
		};
		uint64_t is_signed : 1;
		Endian endian : 1;
		LenType length : 2;
		Type type : 4;

		///// START FOR FUTURE
		uint64_t domain_variant: 56 = 0;
		//domain can fit up to 72 057 594 037 927 935 types (0xFF FFFF FFFF FFFF)
		enum class StandartDomain : uint64_t {
			_reserved_from = 0x0,
			_reserved_to = 0xFF
		};
		std::vector<uint8_t> Encode() const {
			std::vector<uint8_t> res;
			res.push_back(PatrialEncode());
			if (NeedPostDecodeEncode()) {
				auto tmp = PostEncode();
				res.insert(res.end(), tmp.begin(), tmp.end());
			}
			return res;
		}
		uint8_t PatrialEncode() const {
			union {
				struct {
					uint8_t is_signed : 1;
					uint8_t endian : 1;
					uint8_t len : 2;
					uint8_t type : 4;
				} encode;
				uint8_t encoded = 0;
			};
			encode.is_signed = is_signed;
			encode.endian = (uint8_t)endian;
			encode.len = (uint8_t)length;
			encode.type = (uint8_t)type;
			return encoded;
		}
		std::vector<uint8_t> PostEncode() const {
			std::vector<uint8_t> tmp;
			switch (length)
			{
			case LenType::Tiny:
				return { (uint8_t)domain_variant };
			case LenType::Short:
			{
				union
				{
					uint16_t bp = 0;
					uint8_t lp[2];
				};
				bp = ENBT::ConvertEndian<ENBT::Endian::little>((uint16_t)domain_variant);
				tmp.insert(tmp.end(), lp, lp + 2);
				return tmp;
			}
			case LenType::Default:
			{
				union
				{
					uint32_t bp = 0;
					uint8_t lp[4];
				};
				bp = ENBT::ConvertEndian<ENBT::Endian::little>( (uint32_t)domain_variant);
				tmp.insert(tmp.end(), lp, lp + 4);
				return tmp;
			}
			case LenType::Long:
			{
				union
				{
					uint64_t bp = 0;
					uint8_t lp[8];
				};
				bp = ENBT::ConvertEndian<ENBT::Endian::little>(domain_variant);
				tmp.insert(tmp.end(), lp, lp + 8);
				return tmp;
			}
			default:
				return {};
			}
		}
		size_t Decode(const std::vector<uint8_t>& full_code, size_t index) {
			PatrialDecode(full_code[index]);
			if (NeedPostDecodeEncode())
				return PostDecode(full_code, index + 1);
			return index + 1;
		}
		void PatrialDecode(uint8_t basic_code) {
			union {
				struct {
					uint8_t is_signed : 1;
					uint8_t endian : 1;
					uint8_t len : 2;
					uint8_t type : 4;
				} decoded;
				uint8_t encoded = 0;
			};
			encoded = basic_code;
			is_signed = decoded.is_signed;
			endian = (Endian)decoded.endian;
			length = (LenType)decoded.len;
			type = (Type)decoded.type;
		}
		bool NeedPostDecodeEncode() const {
			return type == Type::domain;
		}
		size_t PostDecode(const std::vector<uint8_t>& part_code, size_t index) {
			if(type != Type::domain)
				throw EnbtException("Invalid typeid");

			switch (length) {
			case LenType::Tiny:
				domain_variant = part_code[index];
				++index;
				break;
			case LenType::Short:
			{
				union
				{
					uint16_t bp = 0;
					uint8_t lp[2];
				};
				lp[index] = part_code[0];
				lp[index+1] = part_code[1];
				domain_variant = ConvertEndian<ENBT::Endian::little>(bp);
				return index + 2;
			}
			case LenType::Default:
			{
				union
				{
					uint32_t bp = 0;
					uint8_t lp[4];
				};
				lp[0] = part_code[index];
				lp[1] = part_code[index+1];
				lp[2] = part_code[index+2];
				lp[3] = part_code[index+3];
				domain_variant = ConvertEndian<ENBT::Endian::little>(bp);
				return index + 4;
			}
			case LenType::Long:
			{
				union
				{
					uint64_t bp = 0;
					uint8_t lp[8];
				};
				lp[0] = part_code[index];
				lp[1] = part_code[index+1];
				lp[2] = part_code[index+2];
				lp[3] = part_code[index+3];
				lp[4] = part_code[index+4];
				lp[5] = part_code[index+5];
				lp[6] = part_code[index+6];
				lp[7] = part_code[index+7];
				domain_variant = ConvertEndian<ENBT::Endian::little>(bp);
				return index + 8;
			}
			default:
				throw EnbtException("Domain id too big");
			}
		}
		///// END FOR FUTURE


		std::endian getStdEndian() {
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
		Type_ID(Type ty = Type::none, LenType lt = LenType::Tiny, Endian en = Endian::native,bool sign = false) {
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

	typedef 
		std::variant <
			bool,
			uint8_t, int8_t,
			uint16_t, int16_t,
			uint32_t, int32_t,
			uint64_t, int64_t,
			float, double,
			uint8_t*, uint16_t*,
			uint32_t*,uint64_t*,
			std::vector<ENBT>*,//source pointer
			std::unordered_map<std::string, ENBT>*,//source pointer,
			UUID,
			ENBT*,nullptr_t
		> EnbtValue;

#pragma region EndianConvertHelper
	static void EndianSwap(void* value_ptr, size_t len) {
		char* tmp = new char[len];
		char* prox = (char*)value_ptr;
		int j = 0;
		for (int64_t i = len - 1; i >= 0; i--)
			tmp[i] = prox[j++];
		for (size_t i = 0; i < len; i++)
			prox[i] = prox[i];
		delete[]tmp;
	}
	static void ConvertEndian(ENBT::Endian value_endian, void* value_ptr, size_t len) {
		if (ENBT::Endian::native != value_endian)
			EndianSwap(value_ptr, len);
	}
	template<class T>
	static T ConvertEndian(ENBT::Endian value_endian, T val) {
		if (ENBT::Endian::native != value_endian)
			EndianSwap(&val, sizeof(T));
		return val;
	}
	template<class T>
	static T* ConvertEndianArr(ENBT::Endian value_endian, T* val, size_t size) {
		if (ENBT::Endian::native != value_endian)
			for (size_t i = 0; i < size; i++)
				EndianSwap(&val[i], sizeof(T));
		return val;
	}

	template<ENBT::Endian value_endian>
	static void ConvertEndian(void* value_ptr, size_t len) {
		if constexpr (ENBT::Endian::native != value_endian)
			EndianSwap(value_ptr, len);
	}
	template<ENBT::Endian value_endian, class T>
	static T ConvertEndian(T val) {
		if constexpr (ENBT::Endian::native != value_endian)
			EndianSwap(&val, sizeof(T));
		return val;
	}
	template<ENBT::Endian value_endian, class T>
	static T* ConvertEndianArr(T* val, size_t size) {
		if constexpr (ENBT::Endian::native != value_endian)
			for (size_t i = 0; i < size; i++)
				EndianSwap(&val[i], sizeof(T));
		return val;
	}

#pragma endregion
	
	class DomainImplementation {
	public:
		virtual void init(uint8_t*& to_init_value, Type_ID& tid,size_t& data_len) = 0;
		virtual bool need_destruct(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void destruct(uint8_t*& to_destruct_value, Type_ID& tid, size_t& data_len) = 0;
		virtual uint8_t* clone(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual size_t size(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual ENBT& index(uint8_t*& value, Type_ID tid, size_t data_len,size_t index) = 0;
		virtual ENBT index(const uint8_t*& value, Type_ID tid, size_t data_len, size_t index) = 0;
		virtual ENBT& fing(uint8_t*& value, Type_ID tid, size_t data_len, const std::string& str) = 0;
		virtual ENBT fing(const uint8_t*& value, Type_ID tid, size_t data_len, const std::string& str) = 0; 
		virtual bool exists(const uint8_t*& value, Type_ID tid, size_t data_len, const std::string& str) = 0;
		virtual void remove(uint8_t*& value, Type_ID tid, size_t data_len, size_t index) = 0;
		virtual void remove(uint8_t*& value, Type_ID tid, size_t data_len, const std::string& index) = 0;
		virtual size_t push(uint8_t*& value, Type_ID tid, size_t data_len, ENBT& val) = 0;
		virtual ENBT front(const uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual ENBT& front(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void pop(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void resize(uint8_t*& value, Type_ID tid, size_t data_len,size_t new_size) = 0;
		virtual void* begin(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void* end(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void next(const uint8_t*& value, Type_ID tid, size_t data_len, void*& interator, void* end) = 0;
		virtual bool can_next(const uint8_t*& value, Type_ID tid, size_t data_len, void* interator, void* end) = 0;
		virtual ENBT get_by_pointer(const uint8_t*& value, Type_ID tid, size_t data_len, void* pointer) = 0;
		virtual ENBT& get_by_pointer(uint8_t*& value, Type_ID tid, size_t data_len, void* pointer) = 0;

		virtual std::string to_DENBT() const = 0;
		virtual ENBT from_DENBT(const std::string&) = 0;
		virtual uint64_t domain_id() = 0;
		//return bytes to write in file
		virtual uint8_t* encode(size_t& len) = 0;
		//accept bytes from file, and return bytes to use in memory
		virtual uint8_t* decode(uint8_t* file_bytes, size_t file_bytes_len) = 0;
		virtual void skip(std::istream& stream) = 0;
	};
protected:
	static EnbtValue get_content(uint8_t* data,size_t data_len, Type_ID data_type_id) {
		uint8_t* real_data = get_data(data, data_type_id,data_len);
		switch (data_type_id.type)
		{
		case Type::integer:
			switch (data_type_id.length)
			{
			case  TypeLen::Tiny:
				if (data_type_id.is_signed)
					return *(int8_t*)real_data;
				else
					return *(uint8_t*)real_data;
			case  TypeLen::Short:
				if (data_type_id.is_signed)
					return *(int16_t*)real_data;
				else
					return *(uint16_t*)real_data;
			case  TypeLen::Default:
				if (data_type_id.is_signed)
					return *(int32_t*)real_data;
				else
					return *(uint32_t*)real_data;
			case  TypeLen::Long:
				if (data_type_id.is_signed)
					return *(int64_t*)real_data;
				else
					return *(uint64_t*)real_data;
			default:
				return nullptr;
			}
		case Type::ex_integer:
			switch (data_type_id.length)
			{
			case TypeLen::Tiny:
			case TypeLen::Long:
				if (data_type_id.is_signed)
					return *(int64_t*)real_data;
				else
					return *(uint64_t*)real_data;
			case TypeLen::Default:
				if (data_type_id.is_signed)
					return *(int32_t*)real_data;
				else
					return *(uint32_t*)real_data;
			case TypeLen::Short:
			default:
				return nullptr;
			}
		case Type::floating:
			switch (data_type_id.length)
			{
			case  TypeLen::Default:
				return *(float*)real_data;
			case  TypeLen::Long:
				return *(double*)real_data;
			default:
				return nullptr;
			}
		case Type::uuid:		return *(UUID*)real_data;
		case Type::string:
			switch (data_type_id.length)
			{
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
			return ((std::vector<ENBT>*)data);
		case Type::compound: 
			return ((std::unordered_map<std::string, ENBT>*)data);
		case Type::structure:
		case Type::optional: if (data) return ((ENBT*)data); else return nullptr;
		case Type::bit: return (bool)data_type_id.is_signed;
		default:							return nullptr;
		}
	}

	static uint8_t* clone_data(uint8_t* data, Type_ID data_type_id, size_t data_len) {
		switch (data_type_id.type)
		{
		case Type::integer:
		case Type::floating:
			if constexpr (sizeof(size_t) < 8)
				if (data_type_id.length == TypeLen::Long)
					return (uint8_t*)new uint64_t(*(uint64_t*)data);
			break;
		case Type::ex_integer:
			if constexpr (sizeof(size_t) < 8)
				if (data_type_id.length == TypeLen::Long || data_type_id.length == TypeLen::Tiny)
					return (uint8_t*)new uint64_t(*(uint64_t*)data);
			break;
		case Type::string:
		{
			switch (data_type_id.length)
			{
			case TypeLen::Tiny:
			{
				uint8_t* res = new uint8_t[data_len];
				for (size_t i = 0; i < data_len; i++)
					res[i] = data[i];
				return res;
			}
			case TypeLen::Short:
			{
				uint16_t* res = new uint16_t[data_len];
				uint16_t* proxy = (uint16_t*)data;
				for (size_t i = 0; i < data_len; i++)
					res[i] = proxy[i];
				return (uint8_t*)res;
			}
			case TypeLen::Default:
			{
				uint32_t* res = new uint32_t[data_len];
				uint32_t* proxy = (uint32_t*)data;
				for (size_t i = 0; i < data_len; i++)
					res[i] = proxy[i];
				return (uint8_t*)res;
			}
			case TypeLen::Long:
			{
				uint64_t* res = new uint64_t[data_len];
				uint64_t* proxy = (uint64_t*)data;
				for (size_t i = 0; i < data_len; i++)
					res[i] = proxy[i];
				return (uint8_t*)res;
			}
			default:
				break;
			}
		}
		break;
		case Type::array:
			return (uint8_t*)new std::vector<ENBT>(*(std::vector<ENBT>*)data);
		case Type::compound:
			return (uint8_t*)new std::unordered_map<std::string, ENBT>(*(std::unordered_map<std::string, ENBT>*)data);
		case Type::structure:
		{
			ENBT* cloned = new ENBT[data_len];
			ENBT* source = (ENBT*)data;
			for (size_t i = 0; i < data_len; i++)
				cloned[i] = source[i];
			return (uint8_t*)cloned; 
		}
		case Type::optional:
			if (data)
				return (uint8_t*)new ENBT((ENBT*)data);
			else
				return nullptr;
		case Type::uuid:
			return (uint8_t*)new UUID(*(UUID*)data);
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
	uint8_t* clone_data() const  {
		return clone_data(data, data_type_id, data_len);
	}

	static uint8_t* get_data(uint8_t*& data, Type_ID data_type_id, size_t data_len) {
		if (need_free(data_type_id, data_len))
			return data;
		else
			return (uint8_t*)&data;
	}
	static bool need_free(Type_ID data_type_id, size_t data_len) {
		switch (data_type_id.type)
		{
		case Type::integer:
		case Type::floating:
			if constexpr (sizeof(size_t) < 8)
				if (data_type_id.length == TypeLen::Long)
					return true;
			return false;
		case Type::ex_integer:
			if constexpr (sizeof(size_t) < 8)
				if (data_type_id.length == TypeLen::Long || data_type_id.length == TypeLen::Tiny)
					return true;
			return false;
		default:
			return true;
		}
	}
	static void free_data(uint8_t* data, Type_ID data_type_id, size_t data_len) noexcept {
		if (data == nullptr)
			return;
		switch (data_type_id.type)
		{
		case Type::none:
			break;
		case Type::integer:
		case Type::floating:
			if constexpr (sizeof(size_t) < 8)
				if (data_type_id.length == TypeLen::Long)
					delete (uint64_t*)data;
			break;
		case Type::ex_integer:
			if constexpr (sizeof(size_t) < 8)
				if (data_type_id.length == TypeLen::Long || data_type_id.length == TypeLen::Tiny)
					delete (uint64_t*)data;
			break;
		case Type::array:
			delete (std::vector<ENBT>*)data;
			break;
		case Type::compound:
			delete (std::unordered_map<std::string, ENBT>*)data;
			break;
		case  Type::structure:
			delete[] (ENBT*)data;
			break;
		case Type::optional:
			if(data_type_id.is_signed)
				delete (ENBT*)data;
			break;
		case Type::uuid:
			delete (UUID*)data;
			break;
		default:
			delete[] data;
		}
		data = nullptr;
	}


	template <class T>
	static size_t len(T* val) {
		T* len_calc = val;
		size_t size = 1;
		while (*len_calc++)size++;
		return size;
	}

	static void check_len(Type_ID tid, size_t len) {
		switch (tid.length)
		{
		case TypeLen::Tiny:
			if (tid.is_signed) {
				if (len > INT8_MAX)
					throw EnbtException("Invalid tid");
			}
			else {
				if (len > UINT8_MAX)
					throw EnbtException("Invalid tid");
			}
			break;
		case TypeLen::Short:
			if (tid.is_signed) {
				if (len > INT16_MAX)
					throw EnbtException("Invalid tid");
			}
			else {
				if (len > UINT16_MAX)
					throw EnbtException("Invalid tid");
			}
			break;
		case TypeLen::Default:
			if (tid.is_signed) {
				if (len > INT32_MAX)
					throw EnbtException("Invalid tid");
			}
			else {
				if (len > UINT32_MAX)
					throw EnbtException("Invalid tid");
			}
			break;
		case TypeLen::Long:
			if (tid.is_signed) {
				if (len > INT64_MAX)
					throw EnbtException("Invalid tid");
			}
			else {
				if (len > UINT64_MAX)
					throw EnbtException("Invalid tid");
			}
			break;
		}
	}

	//if data_len <= sizeof(size_t) contain value in ptr 
	//if data_len > sizeof(size_t) contain ptr to bytes array 
 	uint8_t* data = nullptr;
	size_t data_len;
	Type_ID data_type_id;

	template<class T>
	void set_data(T val) {
		data_len = sizeof(data_len);
		if (data_len <= sizeof(size_t) && data_type_id.type != Type::uuid) {
			data = nullptr;
			char* prox0 = (char*)&data;
			char* prox1 = (char*)&val;
			for (size_t i = 0; i < data_len; i++)
				prox0[i] = prox1[i];
		}
		else {
			free_data(data,data_type_id,data_len);
			data = (uint8_t*)new T(val);
		}
	}
	template<class T>
	void set_data(T* val,size_t len) {
		data_len = len * sizeof(T);
		if (data_len <= sizeof(size_t)) {
			char* prox0 = (char*)data;
			char* prox1 = (char*)val;
			for (size_t i = 0; i < data_len; i++)
				prox0[i] = prox1[i];
		}
		else {
			free_data(data, data_type_id, data_len);
			T* tmp = new T[len / sizeof(T)];
			for(size_t i=0;i<len;i++)
				tmp[i] = val[i];
			data = (uint8_t*)tmp;
		}
	}
	ENBT(uint8_t* data, size_t data_len, Type_ID data_type_id) :data(data), data_len(data_len), data_type_id(data_type_id) {}
public:
	ENBT() { data = nullptr; data_len = 0; data_type_id = Type_ID{ Type::none,TypeLen::Tiny }; }
	template<class T>
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
	template<class T = ENBT>
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
	ENBT(const std::unordered_map<std::string, ENBT>& compound, TypeLen len_type = TypeLen::Long) {
		data_type_id = Type_ID{ Type::compound,len_type,false };
		check_len(data_type_id, compound.size()); 
		data = (uint8_t*)new std::unordered_map<std::string, ENBT>(compound);
	}

	ENBT(std::vector<ENBT>&& array) {
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
	ENBT(std::unordered_map<std::string, ENBT>&& compound, TypeLen len_type = TypeLen::Long) {
		data_type_id = Type_ID{ Type::compound,len_type,false };
		check_len(data_type_id, compound.size());
		data = (uint8_t*)new std::unordered_map<std::string, ENBT>(std::move(compound));
	}

	ENBT(const uint8_t* utf8_str)  {
		size_t size = len(utf8_str);
		data_type_id = Type_ID{ Type::string,TypeLen::Tiny,false };
		uint8_t* str = new uint8_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const uint16_t* utf16_str,ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		size_t size = len(utf16_str);
		data_type_id = Type_ID{ Type::string,TypeLen::Short, str_endian,false };
		uint16_t* str = new uint16_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf16_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, size);

		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const uint32_t* utf32_str,ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		size_t size = len(utf32_str);
		data_type_id = Type_ID{ Type::string,TypeLen::Default, str_endian,false };
		uint32_t* str = new uint32_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf32_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, size);
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const uint64_t* utf64_str, ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		size_t size = len(utf64_str);
		data_type_id = Type_ID{ Type::string,TypeLen::Default, str_endian,false };
		uint64_t* str = new uint64_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf64_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, size);
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const uint8_t* utf8_str,size_t slen) {
		data_type_id = Type_ID{ Type::string,TypeLen::Tiny,false };
		uint8_t* str = new uint8_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const uint16_t* utf16_str, size_t slen,ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		data_type_id = Type_ID{ Type::string,TypeLen::Short, str_endian,false };
		uint16_t* str = new uint16_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf16_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, slen);


		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const uint32_t* utf32_str, size_t slen,ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false)  {
		data_type_id = Type_ID{ Type::string,TypeLen::Default, str_endian,false };
		uint32_t* str = new uint32_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf32_str[i];
		if(convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, slen);
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const uint64_t* utf64_str, size_t slen, ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		data_type_id = Type_ID{ Type::string,TypeLen::Long, str_endian,false };
		uint64_t* str = new uint64_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf64_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, slen);
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const int8_t* utf8_str) {
		size_t size = len(utf8_str);
		data_type_id = Type_ID{ Type::string,TypeLen::Tiny,true };
		int8_t* str = new int8_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const int16_t* utf16_str, ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		size_t size = len(utf16_str);
		data_type_id = Type_ID{ Type::string,TypeLen::Short, str_endian,true };
		int16_t* str = new int16_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf16_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, size);

		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const int32_t* utf32_str, ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		size_t size = len(utf32_str);
		data_type_id = Type_ID{ Type::string,TypeLen::Default, str_endian,true };
		int32_t* str = new int32_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf32_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, size);
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const int64_t* utf64_str, ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		size_t size = len(utf64_str);
		data_type_id = Type_ID{ Type::string,TypeLen::Default, str_endian,true };
		int64_t* str = new int64_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf64_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, size);
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const int8_t* utf8_str, size_t slen) {
		data_type_id = Type_ID{ Type::string,TypeLen::Tiny,true };
		int8_t* str = new int8_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const int16_t* utf16_str, size_t slen, ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		data_type_id = Type_ID{ Type::string,TypeLen::Short, str_endian,true };
		int16_t* str = new int16_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf16_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, slen);


		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const int32_t* utf32_str, size_t slen, ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		data_type_id = Type_ID{ Type::string,TypeLen::Default, str_endian,true };
		int32_t* str = new int32_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf32_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, slen);
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const int64_t* utf64_str, size_t slen, ENBT::Endian str_endian = ENBT::Endian::native, bool convert_endian = false) {
		data_type_id = Type_ID{ Type::string,TypeLen::Long, str_endian,true };
		int64_t* str = new int64_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf64_str[i];
		if (convert_endian && ENBT::Endian::native != str_endian)
			ConvertEndianArr(str_endian, str, slen);
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const char* utf8_str) {
		constexpr bool char_is_signed = (char)-1 < 0;
		size_t size = len(utf8_str);
		data_type_id = Type_ID{ Type::string,TypeLen::Tiny,char_is_signed };
		uint8_t* str = new uint8_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const char* utf8_str, size_t slen) {
		constexpr bool char_is_signed = (char)-1 < 0;
		data_type_id = Type_ID{ Type::string,TypeLen::Tiny,char_is_signed };
		int8_t* str = new int8_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = slen;
	}



	ENBT(UUID uuid, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		data_type_id = Type_ID{ Type::uuid };
		set_data(uuid);
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, data, data_len);
	}
	ENBT(bool byte) {
		data_type_id = Type_ID{ Type::bit,TypeLen::Tiny,byte };
		data_len = 0;
	}
	ENBT(int8_t byte) {
		set_data(byte);
		data_type_id = Type_ID{ Type::integer,TypeLen::Tiny,true };
	}
	ENBT(uint8_t byte) {
		set_data(byte);
		data_type_id = Type_ID{ Type::integer,TypeLen::Tiny };
	}
	ENBT(int16_t sh, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &sh, sizeof(int16_t));
		set_data(sh);
		data_type_id = Type_ID{ Type::integer,TypeLen::Short,endian,true };
	}
	ENBT(int32_t in,bool as_var, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &in, sizeof(int32_t));
		set_data(in);
		if(as_var)
			data_type_id = Type_ID{ Type::ex_integer,TypeLen::Default,endian,true };
		else
			data_type_id = Type_ID{ Type::integer,TypeLen::Default,endian,true };
	}
	ENBT(int64_t lon, bool as_var, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &lon, sizeof(int64_t));
		set_data(lon);
		if (as_var)
			data_type_id = Type_ID{ Type::ex_integer,TypeLen::Long,endian,true };
		else
			data_type_id = Type_ID{ Type::integer,TypeLen::Long,endian,true };
	}
	ENBT(int32_t in, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &in, sizeof(int32_t));
		set_data(in);
		data_type_id = Type_ID{ Type::integer,TypeLen::Default,endian,true };
	}
	ENBT(int64_t lon, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &lon, sizeof(int64_t));
		set_data(lon);
		data_type_id = Type_ID{ Type::integer,TypeLen::Long,endian,true };
	}
	ENBT(uint16_t sh, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &sh, sizeof(uint16_t));
		set_data(sh);
		data_type_id = Type_ID{ Type::integer,TypeLen::Short,endian };
	}
	ENBT(uint32_t in, bool as_var, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &in, sizeof(uint32_t));
		set_data(in);
		if (as_var)
			data_type_id = Type_ID{ Type::ex_integer,TypeLen::Default,endian,false };
		else
			data_type_id = Type_ID{ Type::integer,TypeLen::Default,endian,false };
	}
	ENBT(uint64_t lon, bool as_var, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &lon, sizeof(uint64_t));
		set_data(lon);
		if (as_var)
			data_type_id = Type_ID{ Type::ex_integer,TypeLen::Long,endian,false };
		else
			data_type_id = Type_ID{ Type::integer,TypeLen::Long,endian,false };
	}
	ENBT(uint32_t in, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &in, sizeof(uint32_t));
		set_data(in);
		data_type_id = Type_ID{ Type::integer,TypeLen::Default,endian,false };
	}
	ENBT(uint64_t lon, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &lon, sizeof(uint64_t));
		set_data(lon);
		data_type_id = Type_ID{ Type::integer,TypeLen::Long,endian,false };
	}
	ENBT(float flo, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &flo, sizeof(float));
		set_data(flo);
		data_type_id = Type_ID{ Type::floating,TypeLen::Default,endian };
	}
	ENBT(double dou, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if(convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &dou, sizeof(double));
		set_data(dou);
		data_type_id = Type_ID{ Type::floating,TypeLen::Long,endian };
	}
	ENBT(EnbtValue val, Type_ID tid,size_t length, bool string_convert_endian_on_write = true) {
		data_type_id = tid;
		data_len = 0;
		switch (tid.type)
		{
		case Type::integer:
			switch (data_type_id.length)
			{
			case  TypeLen::Tiny:
				if (data_type_id.is_signed)
					set_data(std::get<int8_t>(val));
				else
					set_data(std::get<uint8_t>(val));
				break;
			case  TypeLen::Short:
				if (data_type_id.is_signed)
					set_data(std::get<int16_t>(val));
				else
					set_data(std::get<uint16_t>(val));
				break;
			case  TypeLen::Default:
				if (data_type_id.is_signed)
					set_data(std::get<int32_t>(val));
				else
					set_data(std::get<uint32_t>(val));
				break;
			case  TypeLen::Long:
				if (data_type_id.is_signed)
					set_data(std::get<int64_t>(val));
				else
					set_data(std::get<uint64_t>(val));
			}
			break;
		case Type::floating:
			switch (data_type_id.length)
			{
			case  TypeLen::Default:
				set_data(std::get<float>(val));
				break;
			case  TypeLen::Long:
				set_data(std::get<double>(val));
			}
			break;
		case Type::ex_integer:
			switch (data_type_id.length)
			{
			case TypeLen::Tiny:
				if (data_type_id.is_signed)
					set_data(std::get<int64_t>(val));
				else
					set_data(std::get<uint64_t>(val));
				break;
			case TypeLen::Default:
				if (data_type_id.is_signed)
					set_data(std::get<int32_t>(val));
				else
					set_data(std::get<uint32_t>(val));
				break;
			case  TypeLen::Long:
				if (data_type_id.is_signed)
					set_data(std::get<int64_t>(val));
				else
					set_data(std::get<uint64_t>(val));
			}
			break;
		case Type::uuid:set_data(std::get<uint8_t*>(val), 16); break;
		case Type::string: 
		{
			switch (data_type_id.length) {
			case  TypeLen::Tiny:
				set_data(std::get<uint8_t*>(val), len(std::get<uint8_t*>(val)));
				break;
			case  TypeLen::Short:
				set_data(std::get<uint16_t*>(val), len(std::get<uint16_t*>(val)));
				break;
			case  TypeLen::Default:
				set_data(std::get<uint32_t*>(val), len(std::get<uint32_t*>(val)));
				break;
			case  TypeLen::Long:
				set_data(std::get<uint64_t*>(val), len(std::get<uint64_t*>(val)));
			}
			tid.is_signed = string_convert_endian_on_write;
			break;
		}
		case Type::array:set_data(new std::vector<ENBT>(*std::get<std::vector<ENBT>*>(val))); break;
		case Type::compound:set_data(new std::unordered_map<std::string, ENBT>(*std::get<std::unordered_map<std::string, ENBT>*>(val))); break;
		case Type::structure:set_data(clone_data((uint8_t*)std::get<ENBT*>(val), tid, length)); data_len = length; break;
		case Type::bit:set_data(clone_data((uint8_t*)std::get<bool>(val), tid, length)); data_len = length; break;
		default:
			data = nullptr;
			data_len = 0;
		}
	}
	ENBT(ENBT* structureValues, size_t elems, TypeLen len_type = TypeLen::Tiny) {
		data_type_id = Type_ID{ Type::structure,len_type };
		if (!structureValues)
			throw EnbtException("structure is nullptr");
		if(!elems)
			throw EnbtException("structure canont be zero elements");
		check_len(data_type_id, elems * 4);
		data = clone_data((uint8_t*)structureValues, data_type_id, elems);
		data_len = elems;
	}
	ENBT(ENBT* optional) {
		if (optional) {
			data_type_id = Type_ID{ Type::optional,TypeLen::Tiny,true };
			data = clone_data((uint8_t*)optional, data_type_id, 0);
		}
		else
			data_type_id = Type_ID{ Type::optional };
		data_len = 0;
	}
	ENBT(nullptr_t) {
		data_type_id = Type_ID{ Type::optional };
		data_len = 0;
		data = nullptr;
	}
	ENBT(int64_t lon, TypeLen len_type, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &lon, sizeof(int64_t));
		set_data(lon);
		assert(len_type != TypeLen::Short && "Unsuported ex_integer");
		assert(len_type == TypeLen::Default && (int32_t)lon!= lon && "Too large ex_integer");
		data_type_id = Type_ID{ Type::ex_integer,len_type,endian,true };
	}
	ENBT(uint64_t lon, TypeLen len_type, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &lon, sizeof(int64_t));
		set_data(lon);
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &lon, sizeof(int64_t));
		set_data(lon);
		assert(len_type != TypeLen::Short && "Unsuported ex_integer");
		assert(len_type == TypeLen::Default && (int32_t)lon != lon && "Too large ex_integer");
		data_type_id = Type_ID{ Type::ex_integer,len_type,endian,true };
	}
	ENBT(int32_t in, TypeLen len_type, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &in, sizeof(int64_t));
		set_data(in);
		assert(len_type != TypeLen::Short && "Unsuported ex_integer");
		data_type_id = Type_ID{ Type::ex_integer,len_type,endian,true };
	}
	ENBT(uint32_t in, TypeLen len_type, ENBT::Endian endian = ENBT::Endian::native, bool convert_endian = false) {
		if (convert_endian && ENBT::Endian::native != endian)
			ConvertEndian(endian, &in, sizeof(int64_t));
		set_data(in);
		assert(len_type != TypeLen::Short && "Unsuported ex_integer");
		data_type_id = Type_ID{ Type::ex_integer,len_type,endian,true };
	}


	ENBT(Type_ID tid,size_t len = 0) {
		switch (tid.type)
		{
		case Type::compound:
			operator=(ENBT(std::unordered_map<std::string, ENBT>(), tid.length));
			break;
		case Type::array:
			operator=(ENBT(std::vector<ENBT>(len)));
			break;
		case Type::string:
			if (len) {
				switch (tid.length)
				{
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

	ENBT(Type typ, size_t len = 0) {
		switch (len) {
		case 0:
			operator=(Type_ID(typ,TypeLen::Tiny, false));
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

	ENBT(const ENBT& copy) {
		operator=(copy);
	}	
	ENBT(bool optional, ENBT&& value) {
		if (optional) {
			data_type_id = Type_ID(Type::optional, TypeLen::Tiny, true);
			data = (uint8_t*)new ENBT(std::move(value));
		}
		else {
			data_type_id = Type_ID(Type::optional, TypeLen::Tiny, false);
			data = nullptr;
		}
		data_len = 0;
	}
	ENBT(bool optional, const ENBT& value) {
		if (optional) {
			data_type_id = Type_ID(Type::optional, TypeLen::Tiny, true);
			data = (uint8_t*)new ENBT(value);
		}
		else {
			data_type_id = Type_ID(Type::optional, TypeLen::Tiny, false);
			data = nullptr;
		}
		data_len = 0;
	}
	ENBT(ENBT&& copy) noexcept {
		operator=(std::move(copy));
	}
	ENBT& operator=(ENBT&& copy) noexcept {
		if (data)
			free_data(data, data_type_id, data_len);
		data = copy.data;
		data_len = copy.data_len;
		data_type_id = copy.data_type_id;

		copy.data_type_id = {};
		return *this;
	}
	~ENBT() {
		free_data(data, data_type_id, data_len);
	}

	ENBT& operator=(const ENBT& copy) {
		if (data)
			free_data(data, data_type_id, data_len);
		data = copy.clone_data();
		data_len = copy.data_len;
		data_type_id = copy.data_type_id;
		return *this;
	}
	template<class T>
	ENBT& operator=(const T& set_value) {
		return operator=(ENBT(set_value));
	}

	bool type_equal(Type_ID tid) const  {
		return !( data_type_id != tid);
	}
	ENBT& operator[](size_t index);
	ENBT& operator[](int index) {
		return operator[]((size_t)index);
	}
	ENBT& operator[](const std::string& index);
	ENBT& operator[](const char* index);
	const ENBT& operator[](size_t index) const;
	const ENBT& operator[](int index) const {
		return operator[]((size_t)index);
	}
	const ENBT& operator[](const std::string& index) const;
	const ENBT& operator[](const char* index) const;

	size_t size() const {
		return data_len;
	}
	Type_ID type_id() const {return data_type_id;}

	EnbtValue content() const {
		return get_content(data, data_len, data_type_id);
	}

	void set_optional(const ENBT& value) {
		if (data_type_id.type == Type::optional) {
			data_type_id.is_signed = true;
			free_data(data, data_type_id, data_len);
			data = (uint8_t*)new ENBT(value);
		}
	}
	void set_optional(ENBT&& value) {
		if (data_type_id.type == Type::optional) {
			data_type_id.is_signed = true;
			free_data(data,data_type_id,data_len);
			data = (uint8_t*)new ENBT(std::move(value));
		}
	}
	void set_optional() {
		if (data_type_id.type == Type::optional) {
			free_data(data,data_type_id,data_len);
			data_type_id.is_signed = false;
		}
	}

	const ENBT* get_optional() const {
		if (data_type_id.type == Type::optional)
			if (data_type_id.is_signed)
				return (ENBT*)data;
		return nullptr;
	}
	ENBT* get_optional() {
		if (data_type_id.type == Type::optional)
			if (data_type_id.is_signed)
				return (ENBT*)data;
		return nullptr;
	}

	bool contains() const {
		if (data_type_id.type == Type::optional)
			if (data_type_id.is_signed)
				return true;
		return data_type_id.type != Type::none;
	}
	bool contains(const std::string& index) const {
		return type_equal(Type::compound) ? ((std::unordered_map<std::string, ENBT>*)data)->contains(index) : false;
	}
	



	Type get_type() const {
		return data_type_id.type;
	}
	TypeLen get_type_len() const {
		return data_type_id.length;
	}
	bool get_type_sign() const {
		return data_type_id.is_signed;
	}
	uint64_t get_type_domain() const {
		if (data_type_id.type == Type::domain)
			return data_type_id.domain_variant;
		else throw EnbtException("The not domain");
	}

	const uint8_t* get_ptr() const {
		return data;
	}

	void remove(size_t index) {
		if (type_equal(Type::array))
			((std::vector<ENBT>*)data)->erase(((std::vector<ENBT>*)data)->begin() + index);
		else throw EnbtException("Cannont remove item from non array type");
	}
	size_t remove(std::string name);
	size_t push(const ENBT& enbt) {
		if (type_equal(Type::array)) {
			if (data_type_id.type == Type::array) {
				if (data_len)
					if (operator[](0).data_type_id != enbt.data_type_id)
						throw EnbtException("Invalid type for pushing array");
			}
			((std::vector<ENBT>*)data)->push_back(enbt);
			return data_len++;
		}
		else throw EnbtException("Cannont push to non array type");
	}
	ENBT& front() {
		if (data_type_id.type == Type::array) {
			if(!data_len)
				throw EnbtException("Array empty");
			return ((std::vector<ENBT>*)data)->front();
		}
		else throw EnbtException("Cannont get front item from non array type");
	}
	void pop() {
		if (type_equal(Type::array)) {
			if (!data_len)
				throw EnbtException("Array empty");
			((std::vector<ENBT>*)data)->pop_back();
		}
		else throw EnbtException("Cannont pop front item from non array type");
	}
	void resize(size_t siz) {
		if (type_equal(Type::array)) {
			((std::vector<ENBT>*)data)->resize(siz);
			if (siz < UINT8_MAX)
				data_type_id.length = TypeLen::Tiny;
			else if (siz < UINT16_MAX)
				data_type_id.length = TypeLen::Short;
			else if (siz < UINT32_MAX)
				data_type_id.length = TypeLen::Default;
			else
				data_type_id.length = TypeLen::Long;
		}
		else if (type_equal(Type::string)) {
			switch (data_type_id.length)
			{
			case TypeLen::Tiny:
			{
				uint8_t* n = new uint8_t[siz];
				for (size_t i = 0; i < siz && i < data_len; i++)
					n[i] = data[i];
				delete[]data;
				data_len = siz;
				data = n;
				break;
			}
			case TypeLen::Short:
			{
				uint16_t* n = new uint16_t[siz];
				uint16_t* prox = (uint16_t*)data;
				for (size_t i = 0; i < siz && i < data_len; i++)
					n[i] = prox[i];
				delete[] data;
				data_len = siz;
				data = (uint8_t*)n;
				break;
			}
			case TypeLen::Default:
			{
				uint32_t* n = new uint32_t[siz];
				uint32_t* prox = (uint32_t*)data;
				for (size_t i = 0; i < siz && i < data_len; i++)
					n[i] = prox[i];
				delete[] data;
				data_len = siz;
				data = (uint8_t*)n;
				break;
			}
			case TypeLen::Long:
			{
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



		}
		else throw EnbtException("Cannont resize non array type");
	}
	bool operator==(const ENBT& enbt) const {
		if (enbt.data_type_id == data_type_id && data_len == enbt.data_len) {
			switch (data_type_id.type)
			{
			case Type::string:
				switch (data_type_id.length)
				{
				case TypeLen::Tiny:
				{
					uint8_t* other = enbt.data;
					for (size_t i = 0; i < data_len; i++)
						if (data[i] != other[i])return false;
					break;
				}
				case TypeLen::Short:
				{
					uint16_t* im = (uint16_t*)data;
					uint16_t* other = (uint16_t*)enbt.data;
					for (size_t i = 0; i < data_len; i++)
						if (im[i] != other[i])return false;
					break;
				}
				case TypeLen::Default:
				{
					uint32_t* im = (uint32_t*)data;
					uint32_t* other = (uint32_t*)enbt.data;
					for (size_t i = 0; i < data_len; i++)
						if (im[i] != other[i])return false;
					break;
				}
				case TypeLen::Long:
				{
					uint64_t* im = (uint64_t*)data;
					uint64_t* other = (uint64_t*)enbt.data;
					for (size_t i = 0; i < data_len; i++)
						if (im[i] != other[i])return false;
					break;
				}
				}
				return true;
			case Type::structure:
			case Type::array:
				return (*std::get<std::vector<ENBT>*>(content())) == (*std::get<std::vector<ENBT>*>(enbt.content()));
			case Type::compound:
				return (*std::get<std::unordered_map<std::string, ENBT>*>(content())) == (*std::get<std::unordered_map<std::string, ENBT>*>(enbt.content()));
			case Type::optional:
				if (data_type_id.is_signed)
					return (*(ENBT*)data) == (*(ENBT*)enbt.data);
				return true;
			default:
				return content() == enbt.content();
			}
		}
		else
			return false;
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
	operator std::string() const;
	operator const uint8_t*() const {
		return std::get<uint8_t*>(content());
	}
	operator const int8_t*() const {
		return (int8_t*)std::get<uint8_t*>(content());
	}
	operator const char*() const {
		return (char*)std::get<uint8_t*>(content());
	}
	operator const int16_t* () const {
		return (int16_t*)std::get<uint16_t*>(content());
	}
	operator const uint16_t* () const {
		return std::get<uint16_t*>(content());
	}
	operator const int32_t* () const {
		return (int32_t*)std::get<uint32_t*>(content());
	}
	operator const uint32_t* () const {
		return std::get<uint32_t*>(content());
	}
	operator const int64_t* () const {
		return (int64_t*)std::get<uint64_t*>(content());
	}
	operator const uint64_t* () const {
		return std::get<uint64_t*>(content());
	}
	template<class T = ENBT>
	operator std::vector<ENBT>() const {
		if(data_type_id.type == Type::array)
			return *std::get<std::vector<ENBT>*>(content());
		else if (data_type_id.type == Type::string) {
			std::vector<ENBT> tmp(data_len);
			if (data_type_id.is_signed) {
				switch (data_type_id.length) {
				case TypeLen::Tiny:
					for (size_t i = 0; i < data_len; i++)
						tmp[i] = ((int8_t*)data)[i];
					break;
				case TypeLen::Short:
					for (size_t i = 0; i < data_len; i++)
						tmp[i] = ENBT(((int16_t*)data)[i],data_type_id.endian, false);
					break;
				case TypeLen::Default:
					for (size_t i = 0; i < data_len; i++)
						tmp[i] = ENBT(((int32_t*)data)[i], data_type_id.endian, false);
					break;
				case TypeLen::Long:
					for (size_t i = 0; i < data_len; i++)
						tmp[i] = ENBT(((int64_t*)data)[i], data_type_id.endian, false);
					break;
				}
			}
			else {
				switch (data_type_id.length) {
				case TypeLen::Tiny:
					for (size_t i = 0; i < data_len; i++)
						tmp[i] = data[i];
					break;
				case TypeLen::Short:
					for (size_t i = 0; i < data_len; i++)
						tmp[i] = ENBT(((uint16_t*)data)[i], data_type_id.endian, false);
					break;
				case TypeLen::Default:
					for (size_t i = 0; i < data_len; i++)
						tmp[i] = ENBT(((uint32_t*)data)[i], data_type_id.endian, false);
					break;
				case TypeLen::Long:
					for (size_t i = 0; i < data_len; i++)
						tmp[i] = ENBT(((uint64_t*)data)[i], data_type_id.endian, false);
					break;
				}
			}
			return tmp;
		}
		else if (data_type_id.type == Type::structure) {
			std::vector<ENBT> tmp(data_len);
			for (size_t i = 0; i < data_len; i++)
				tmp[i] = ((ENBT*)data)[i];
			return tmp;
		}
		else
			throw EnbtException("Invalid cast");
	}
	template<class T>
	operator std::vector<T>() const {
		std::vector<T> res;
		if (data_type_id.type == Type::array) {
			std::vector<ENBT>& tmp = *std::get<std::vector<ENBT>*>(content());
			res.reserve(tmp.size());
			for (auto& temp : tmp)
				res.push_back((T)(temp));
		}
		else if (data_type_id.type == Type::string) {
			res.resize(data_len);
			if (data_type_id.is_signed) {
				switch (data_type_id.length) {
				case TypeLen::Tiny:
					for (size_t i = 0; i < data_len; i++)
						res[i] = (T)(((int8_t*)data)[i]);
					break;
				case TypeLen::Short:
					for (size_t i = 0; i < data_len; i++)
						res[i] = (T)(((int16_t*)data)[i]);
					break;
				case TypeLen::Default:
					for (size_t i = 0; i < data_len; i++)
						res[i] = (T)(((int32_t*)data)[i]);
					break;
				case TypeLen::Long:
					for (size_t i = 0; i < data_len; i++)
						res[i] = (T)(((int64_t*)data)[i]);
					break;
				}
			}
			else {
				switch (data_type_id.length) {
				case TypeLen::Tiny:
					for (size_t i = 0; i < data_len; i++)
						res[i] = (T)(((uint8_t*)data)[i]);
					break;
				case TypeLen::Short:
					for (size_t i = 0; i < data_len; i++)
						res[i] = (T)(((uint16_t*)data)[i]);
					break;
				case TypeLen::Default:
					for (size_t i = 0; i < data_len; i++)
						res[i] = (T)(((uint32_t*)data)[i]);
					break;
				case TypeLen::Long:
					for (size_t i = 0; i < data_len; i++)
						res[i] = (T)(((uint64_t*)data)[i]);
					break;
				}
			}
		}
		else
			throw EnbtException("Invalid cast");
		return res;
	}
	operator std::unordered_map<std::string, ENBT>() const;
	operator UUID() const {
		return std::get<UUID>(content());
	}


	class ConstInterator {
	protected:
		ENBT::Type_ID interate_type;
		void* pointer;
	public:
		ConstInterator(const ENBT& enbt,bool in_begin = true) {
			interate_type = enbt.data_type_id;
			switch (enbt.data_type_id.type)
			{
			case ENBT::Type::array:
				if(in_begin)
					pointer = new std::vector<ENBT>::iterator(
						(*(std::vector<ENBT>*)enbt.data).begin()
					);
				else
					pointer = new std::vector<ENBT>::iterator(
						(*(std::vector<ENBT>*)enbt.data).end()
					);
				break;
			case ENBT::Type::compound:
				if (interate_type.is_signed)
				{
					if (in_begin)
						pointer = new std::unordered_map<uint16_t, ENBT>::iterator(
							(*(std::unordered_map<uint16_t, ENBT>*)enbt.data).begin()
						);
					else
						pointer = new std::unordered_map<uint16_t, ENBT>::iterator(
							(*(std::unordered_map<uint16_t, ENBT>*)enbt.data).end()
						);
				}
				else {
					if (in_begin)
						pointer = new std::unordered_map<std::string, ENBT>::iterator(
							(*(std::unordered_map<std::string, ENBT>*)enbt.data).begin()
						);
					else
						pointer = new std::unordered_map<std::string, ENBT>::iterator(
							(*(std::unordered_map<std::string, ENBT>*)enbt.data).end()
						);
				}
				break;
			default:
				throw EnbtException("Invalid type");
			}
		}
		ConstInterator(ConstInterator&& interator) noexcept {
			interate_type = interator.interate_type;
			pointer = interator.pointer;
			interator.pointer = nullptr;
		}
		ConstInterator(const ConstInterator& interator) {
			interate_type = interator.interate_type;
			switch (interate_type.type)
			{
			case ENBT::Type::array:
				pointer = new std::vector<ENBT>::iterator(
					(*(std::vector<ENBT>::iterator*)interator.pointer)
				);
				break;
			case ENBT::Type::compound:
				if (interate_type.is_signed)
					pointer = new std::unordered_map<uint16_t, ENBT>::iterator(
						(*(std::unordered_map<uint16_t, ENBT>::iterator*)interator.pointer)
					);
				else
					pointer = new std::unordered_map<std::string, ENBT>::iterator(
						(*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer)
					);
				break;
			default:
				throw EnbtException("Unreachable exception in non debug enviropement");
			}
		}


		ConstInterator& operator++() {
			switch (interate_type.type)
			{
			case ENBT::Type::array:
				(*(std::vector<ENBT>::iterator*)pointer)++;
				break;
			case ENBT::Type::compound:
				if (interate_type.is_signed) 
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer)++;
				else
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
			switch (interate_type.type)
			{
			case ENBT::Type::array:
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
			switch (interate_type.type)
			{
			case ENBT::Type::array:
				return
					(*(std::vector<ENBT>::iterator*)pointer)
					==
					(*(std::vector<ENBT>::iterator*)interator.pointer);
			case ENBT::Type::compound:
				return
					(*(std::unordered_map<std::string, ENBT>::iterator*)pointer)
					==
					(*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer);
				break;
			}
			return false;
		}
		bool operator!=(const ConstInterator& interator) const {
			switch (interate_type.type)
			{
			case ENBT::Type::array:
				return
					(*(std::vector<ENBT>::iterator*)pointer)
					!=
					(*(std::vector<ENBT>::iterator*)interator.pointer);
			case ENBT::Type::compound:
				return
					(*(std::unordered_map<std::string, ENBT>::iterator*)pointer)
					!=
					(*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer);
				break;
			}
			return false;
		}

		std::pair<const std::string, const ENBT&> operator*() const {
			switch (interate_type.type)
			{
			case ENBT::Type::array:
				return { "", *(*(std::vector<ENBT>::iterator*)pointer) };
			case ENBT::Type::compound:
				auto& tmp = (*(std::unordered_map<std::string, ENBT>::iterator*)pointer);
				return std::pair<const std::string, const ENBT&>(
					tmp->first,
					tmp->second
				);
			}
			throw EnbtException("Unreachable exception in non debug enviropement");
		}
	};
	class Interator : public ConstInterator {
		
	public:
		Interator(ENBT& enbt, bool in_begin = true) : ConstInterator(enbt, in_begin) {};
		Interator(Interator&& interator) noexcept : ConstInterator(interator) {};
		Interator(const Interator& interator) : ConstInterator(interator) {}


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
		std::pair<const std::string, ENBT&> operator*()  {
			switch (interate_type.type)
			{
			case ENBT::Type::array:
				return { "", *(*(std::vector<ENBT>::iterator*)pointer) };
			case ENBT::Type::compound:
				auto& tmp = (*(std::unordered_map<std::string, ENBT>::iterator*)pointer);
				return std::pair<const std::string, ENBT&>(
					tmp->first,
					tmp->second
				);
			}
			throw EnbtException("Unreachable exception in non debug enviropement");
		}
	};
	class CopyInterator {
	protected:
		ENBT::Type_ID interate_type;
		void* pointer;
	public:
		CopyInterator(const ENBT& enbt, bool in_begin = true) {
			interate_type = enbt.data_type_id;
			switch (enbt.data_type_id.type)
			{
			case ENBT::Type::string:
				if(in_begin)
					pointer = const_cast<uint8_t*>(enbt.get_ptr());
				else {
					switch (interate_type.length)
					{
					case ENBT::TypeLen::Tiny:
						pointer = const_cast<uint8_t*>(enbt.get_ptr() + enbt.size());
						return;
					case ENBT::TypeLen::Short:
						pointer = const_cast<uint8_t*>(enbt.get_ptr() + enbt.size() * 2);
						return;
					case ENBT::TypeLen::Default:
						pointer = const_cast<uint8_t*>(enbt.get_ptr() + enbt.size() * 4);
						return;
					case ENBT::TypeLen::Long:
						pointer = const_cast<uint8_t*>(enbt.get_ptr() + enbt.size() * 8);
						return;
					default:
						pointer = nullptr;
					}
				}
				break;
			case ENBT::Type::array:
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
				if (interate_type.is_signed)
				{
					if (in_begin)
						pointer = new std::unordered_map<uint16_t, ENBT>::iterator(
							(*(std::unordered_map<uint16_t, ENBT>*)enbt.data).begin()
						);
					else
						pointer = new std::unordered_map<uint16_t, ENBT>::iterator(
							(*(std::unordered_map<uint16_t, ENBT>*)enbt.data).end()
						);
				}
				else {
					if (in_begin)
						pointer = new std::unordered_map<std::string, ENBT>::iterator(
							(*(std::unordered_map<std::string, ENBT>*)enbt.data).begin()
						);
					else
						pointer = new std::unordered_map<std::string, ENBT>::iterator(
							(*(std::unordered_map<std::string, ENBT>*)enbt.data).end()
						);
				}
				break;
			default:
				throw EnbtException("Invalid type");
			}
		}
		CopyInterator(CopyInterator&& interator) noexcept {
			interate_type = interator.interate_type;
			pointer = interator.pointer;
			interator.pointer = nullptr;
		}
		CopyInterator(const CopyInterator& interator) {
			interate_type = interator.interate_type;
			switch (interate_type.type)
			{
			case ENBT::Type::string:
				pointer = interator.pointer;
				break;
			case ENBT::Type::array:
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
				throw EnbtException("Unreachable exception in non debug enviropement");
			}
		}


		CopyInterator& operator++() {
			switch (interate_type.type)
			{
			case ENBT::Type::string:
				switch (interate_type.length)
				{
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
			switch (interate_type.type)
			{
			case ENBT::Type::string:
				switch (interate_type.length)
				{
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
			if (interator.interate_type != interate_type)
				return false;
			switch (interate_type.type)
			{
			case ENBT::Type::string:
				return pointer == interator.pointer;
			case ENBT::Type::array:
				return
					(*(std::vector<ENBT>::iterator*)pointer)
					==
					(*(std::vector<ENBT>::iterator*)interator.pointer);
			case ENBT::Type::compound:
				return
					(*(std::unordered_map<std::string, ENBT>::iterator*)pointer)
					==
					(*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer);
				break;
			}
			return false;
		}
		bool operator!=(const CopyInterator& interator) const {
			if (interator.interate_type != interate_type)
				return false; 
			switch (interate_type.type)
			{
			case ENBT::Type::string:
				return pointer != interator.pointer;
			case ENBT::Type::array:
				return
					(*(std::vector<ENBT>::iterator*)pointer)
					!=
					(*(std::vector<ENBT>::iterator*)interator.pointer);
			case ENBT::Type::compound:
				return
					(*(std::unordered_map<std::string, ENBT>::iterator*)pointer)
					!=
					(*(std::unordered_map<std::string, ENBT>::iterator*)interator.pointer);
				break;
			}
			return false;
		}

		std::pair<const std::string, ENBT> operator*() const {
			switch (interate_type.type)
			{
			case ENBT::Type::string:
				if(interate_type.is_signed)
				{
					switch (interate_type.length)
					{
					case ENBT::TypeLen::Tiny:
						return { "",*(int8_t*)pointer };
						break;
					case ENBT::TypeLen::Short:
						return { "",*(int16_t*)pointer };
						break;
					case ENBT::TypeLen::Default:
						return { "",*(int32_t*)pointer };
						break;
					case ENBT::TypeLen::Long:
						return { "",*(int64_t*)pointer };
						break;
					default:
						break;
					}
				}
				else {
					switch (interate_type.length)
					{
					case ENBT::TypeLen::Tiny:
						return { "",*(uint8_t*)pointer };
						break;
					case ENBT::TypeLen::Short:
						return { "",*(uint16_t*)pointer };
						break;
					case ENBT::TypeLen::Default:
						return { "",*(uint32_t*)pointer };
						break;
					case ENBT::TypeLen::Long:
						return { "",*(uint64_t*)pointer };
						break;
					default:
						break;
					}
				}
				break;
			case ENBT::Type::array:
				return { "", *(*(std::vector<ENBT>::iterator*)pointer) };
			case ENBT::Type::compound:
				auto& tmp = (*(std::unordered_map<std::string, ENBT>::iterator*)pointer);
				return std::pair<const std::string, ENBT>(
					tmp->first,
					tmp->second
				);
			}
			throw EnbtException("Unreachable exception in non debug enviropement");
		}
	};


	ConstInterator begin() const {
		return ConstInterator(*this, true);
	}
	ConstInterator end() const {
		return ConstInterator(*this, false);
	}
	Interator begin() {
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

	std::string to_senbt() const;
	static ENBT from_senbt(const std::string&);
	void shrink_to_fit();
};
inline std::istream& operator>>(std::istream& is, ENBT::UUID& uuid) {
	is >> uuid.data[0] >> uuid.data[1] >> uuid.data[2] >> uuid.data[3] >> uuid.data[4] >> uuid.data[5] >> uuid.data[6] >> uuid.data[7];
	is >> uuid.data[8] >> uuid.data[9] >> uuid.data[10] >> uuid.data[11] >> uuid.data[12] >> uuid.data[13] >> uuid.data[14] >> uuid.data[15];
	return is;
}
inline std::ostream& operator<<(std::ostream& os, const ENBT::UUID& uuid) {
	os << uuid.data[0] << uuid.data[1] << uuid.data[2] << uuid.data[3] << uuid.data[4] << uuid.data[5] << uuid.data[6] << uuid.data[7];
	os << uuid.data[8] << uuid.data[9] << uuid.data[10] << uuid.data[11] << uuid.data[12] << uuid.data[13] << uuid.data[14] << uuid.data[15];
	return os;
}

inline std::istream& operator>>(std::istream& is, ENBT::Type_ID& tid) {
	uint8_t part;
	is >> part;
	tid.PatrialDecode(part);
	if (tid.NeedPostDecodeEncode()) {
		std::vector<uint8_t> tmp;
		switch (tid.length)
		{
		case ENBT::TypeLen::Long:
			is >> part; tmp.push_back(part);
			is >> part; tmp.push_back(part);
			is >> part; tmp.push_back(part);
			is >> part; tmp.push_back(part);
			[[fallthrough]];
		case ENBT::TypeLen::Default:
			is >> part; tmp.push_back(part);
			is >> part; tmp.push_back(part);
			[[fallthrough]];
		case ENBT::TypeLen::Short:
			is >> part; tmp.push_back(part);
			[[fallthrough]];
		case ENBT::TypeLen::Tiny:
			is >> part; tmp.push_back(part);
		}
		tid.PostDecode(tmp,0);
	}
	return is;
}
inline std::ostream& operator<<(std::ostream& os, ENBT::Type_ID tid) {
	for (auto tmp : tid.Encode())
		os << tmp;
	return os;
}



class ENBTHelper {
public:
	static std::vector<std::string> SplitS(const std::string& str,const std::string& delimiter) {
		std::vector<std::string> res;
		size_t pos;
		size_t index = 0;
		while ((pos = str.find(delimiter, index)) != std::string::npos) {
			res.push_back(str.substr(index, pos));
			index += pos;
		}
		return res;
	}

	template<class T> static void WriteVar(std::ostream& write_stream, T value, ENBT::Endian endian = ENBT::Endian::native) {
		value = ENBT::ConvertEndian(endian, value);
		do {
			char currentByte = (char)(value & 0b01111111);

			value >>= 7;
			if (value != 0) currentByte |= 0b10000000;

			write_stream << currentByte;
		} while (value != 0);
	}
	template<class T> static void WriteVar(std::ostream& write_stream, ENBT::EnbtValue value, ENBT::Endian endian = ENBT::Endian::native) {
		WriteVar(write_stream, std::get<T>(value), endian);
	}

	static void WriteCompressLen(std::ostream& write_stream, uint64_t len) {
		union {
			uint64_t full = 0;
			uint8_t part[8];

		}b;
		b.full = ENBT::ConvertEndian<ENBT::Endian::big>(len);

		constexpr struct {
			uint64_t b64 : 62 = -1;
			uint64_t b32 : 30 = -1;
			uint64_t b16 : 14 = -1;
			uint64_t b8 : 6 = -1;
		} m;
		if (len <= m.b8) {
			b.full <<= 2;
			write_stream << b.part[0];
		}
		else if (len <= m.b16) {
			b.full <<= 2;
			b.part[0] |= 1;
			write_stream << b.part[0];
			write_stream << b.part[1];
		}
		else if (len <= m.b32) {
			b.full <<= 2;
			b.part[0] |= 2;
			write_stream << b.part[0];
			write_stream << b.part[1];
			write_stream << b.part[2];
			write_stream << b.part[3];
		}
		else if (len <= m.b64) {
			b.full <<= 2;
			b.part[0] |= 3;
			write_stream << b.part[0];
			write_stream << b.part[1];
			write_stream << b.part[2];
			write_stream << b.part[3];
			write_stream << b.part[4];
			write_stream << b.part[5];
			write_stream << b.part[6];
			write_stream << b.part[7];
		}
		else
			throw std::overflow_error("uint64_t cannont put in to uint60_t");
	}
	static void WriteTypeID(std::ostream& write_stream, ENBT::Type_ID tid) {
		write_stream << tid;
	}

	template<class T> static void WriteValue(std::ostream& write_stream, T value, ENBT::Endian endian = ENBT::Endian::native) {
		if constexpr (std::is_same<T, ENBT::UUID>())
			ENBT::ConvertEndian(endian, value.data, 16);
		else 
			value = ENBT::ConvertEndian(endian, value);
		uint8_t* proxy = (uint8_t*)&value;
		for (size_t i = 0; i < sizeof(T); i++)
			write_stream << proxy[i];
	}
	template<class T> static void WriteValue(std::ostream& write_stream, ENBT::EnbtValue value, ENBT::Endian endian = ENBT::Endian::native) {
		return WriteValue(write_stream, std::get<T>(value), endian);
	}

	template<class T> static void WriteArray(std::ostream& write_stream, T* values, size_t len, ENBT::Endian endian = ENBT::Endian::native) {
		if constexpr (sizeof(T) == 1) {
			for (size_t i = 0; i < len; i++)
				write_stream << values[i];
		}
		else {
			for (size_t i = 0; i < len; i++)
				WriteValue(write_stream, values[i], endian);
		}
	}
	template<class T> static void WriteArray(std::ostream& write_stream, ENBT::EnbtValue* values, size_t len, ENBT::Endian endian = ENBT::Endian::native) {
		T* arr = new T[len];
		for (size_t i = 0; i < len; i++)
			arr[i] = std::get<T>(values[i]);
		WriteArray(write_stream, arr, len, endian);
		delete[] arr;
	}

	template<class T> static void WriteDefineLen(std::ostream& write_stream, T value) {
		return WriteValue(write_stream, value, ENBT::Endian::little);
	}
	static void WriteDefineLen(std::ostream& write_stream, uint64_t len, ENBT::Type_ID tid) {
		switch (tid.length)
		{
		case ENBT::TypeLen::Tiny:
			if (len != ((uint8_t)len))
				throw EnbtException("cannont convert value to uint8_t");
			WriteDefineLen(write_stream, (uint8_t)len);
			break;
		case ENBT::TypeLen::Short:
			if (len != ((uint16_t)len))
				throw EnbtException("cannont convert value to uint16_t");
			WriteDefineLen(write_stream, (uint16_t)len);
			break;
		case ENBT::TypeLen::Default:
			if (len != ((uint32_t)len))
				throw EnbtException("cannont convert value to uint32_t");
			WriteDefineLen(write_stream, (uint32_t)len);
			break;
		case ENBT::TypeLen::Long:
			WriteDefineLen(write_stream, (uint64_t)len);
			break;
		}
	}

	static void InitalizeVersion(std::ostream& write_stream) {
		write_stream << (char)ENBT_VERSION_HEX;
	}
	static void InitalizeHeader(std::ostream& write_stream) {
		write_stream << (char)0x89 << "ENBT";
	}

	static void WriteCompoud(std::ostream& write_stream, const ENBT& val) {
		auto result = std::get<std::unordered_map<std::string, ENBT>*>(val.content());
		WriteDefineLen(write_stream, result->size(), val.type_id());
		for (auto& it : *result) {
			WriteCompressLen(write_stream, it.first.size());
			WriteArray(write_stream, it.first.c_str(), it.first.size());
			WriteToken(write_stream, it.second);
		}
	}
	static void WriteArray(std::ostream& write_stream, const ENBT& val) {
		if (!val.type_equal(ENBT::Type::array))
			throw EnbtException("This is not array for serialize it");
		auto result = (std::vector<ENBT>*)val.get_ptr();
		size_t len = result->size();
		WriteDefineLen(write_stream, len, val.type_id());
		if (len) {
			if (val.get_type_sign()) {
				ENBT::Type_ID tid = (*result)[0].type_id();
				if (tid.type != ENBT::Type::bit) {
					WriteTypeID(write_stream, tid);
					for (auto& it : *result)
						WriteValue(write_stream, it);
				}
				else {
					WriteTypeID(write_stream, tid);
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
			else {
				for (auto& it : *result)
					WriteToken(write_stream, it);
			}
		}
	}


	static void WriteSimpleArray(std::ostream& write_stream, const ENBT& val) {
		WriteCompressLen(write_stream, val.size());
		switch (val.type_id().length)
		{
		case ENBT::TypeLen::Tiny:
			WriteArray(write_stream, val.get_ptr(), val.size(), val.type_id().endian);
			break;
		case ENBT::TypeLen::Short:
			WriteArray(write_stream, (uint16_t*)val.get_ptr(), val.size(), val.type_id().endian);
			break;
		case ENBT::TypeLen::Default:
			WriteArray(write_stream, (uint32_t*)val.get_ptr(), val.size(), val.type_id().endian);
			break;
		case ENBT::TypeLen::Long:
			WriteArray(write_stream, (uint64_t*)val.get_ptr(), val.size(), val.type_id().endian);
			break;
		default:
			break;
		}

	}
	static void WriteDomain(std::ostream& write_stream, const ENBT& val) {
		ENBT::DomainImplementation* tmp = (ENBT::DomainImplementation*)val.get_ptr();
		size_t len = 0;
		WriteArray(write_stream, tmp->encode(len), len);
	}
	static void WriteValue(std::ostream& write_stream, const ENBT& val){
		ENBT::Type_ID tid = val.type_id();
		switch (tid.type)
		{
		case ENBT::Type::integer:
			switch (tid.length)
			{
			case  ENBT::TypeLen::Tiny:
				if (tid.is_signed)
					return WriteValue<int8_t>(write_stream, val.content(), tid.endian);
				else
					return WriteValue<uint8_t>(write_stream, val.content(), tid.endian);
			case  ENBT::TypeLen::Short:
				if (tid.is_signed)
					return WriteValue<int16_t>(write_stream, val.content(), tid.endian);
				else
					return WriteValue<uint16_t>(write_stream, val.content(), tid.endian);
			case  ENBT::TypeLen::Default:
				if (tid.is_signed)
					return WriteValue<int32_t>(write_stream, val.content(), tid.endian);
				else
					return WriteValue<uint32_t>(write_stream, val.content(), tid.endian);
			case  ENBT::TypeLen::Long:
				if (tid.is_signed)
					return WriteValue<int64_t>(write_stream, val.content(), tid.endian);
				else
					return WriteValue<uint64_t>(write_stream, val.content(), tid.endian);
			}
			return;
		case ENBT::Type::floating:
			switch (tid.length)
			{
			case  ENBT::TypeLen::Default:
				return WriteValue<float>(write_stream, val.content(), tid.endian);
			case  ENBT::TypeLen::Long:
				return WriteValue<double>(write_stream, val.content(), tid.endian);
			}
			return;
		case ENBT::Type::ex_integer:
			switch (tid.length)
			{
			case ENBT::TypeLen::Tiny:
				if (tid.is_signed)
					return WriteCompressLen(write_stream, (int64_t)val);
				else
					return WriteCompressLen(write_stream, (uint64_t)val);
			case  ENBT::TypeLen::Default:
				if (tid.is_signed)
					return WriteVar<int32_t>(write_stream, val.content(), tid.endian);
				else
					return WriteVar<uint32_t>(write_stream, val.content(), tid.endian);
			case  ENBT::TypeLen::Long:
				if (tid.is_signed)
					return WriteVar<int64_t>(write_stream, val.content(), tid.endian);
				else
					return WriteVar<uint64_t>(write_stream, val.content(), tid.endian);
			}
			return;
		case  ENBT::Type::uuid:	return WriteValue<ENBT::UUID>(write_stream, val.content(), tid.endian);
		case ENBT::Type::string:   return WriteSimpleArray(write_stream, val);
		case ENBT::Type::compound:	return WriteCompoud(write_stream, val);
		case ENBT::Type::array:	return WriteArray(write_stream, val);
		case ENBT::Type::optional:
			if (val.contains())
				WriteToken(write_stream, *val.get_optional());
			break;
		case ENBT::Type::domain:
			return WriteDomain(write_stream, val);
		}
	}
	static void WriteToken(std::ostream& write_stream, const ENBT& val) {
		write_stream << val.type_id();
		WriteValue(write_stream, val);
	}






	template<class T> static T ReadVar(std::istream& read_stream, ENBT::Endian endian) {
		constexpr int max_offset = (sizeof(T) / 5 * 5 + ((sizeof(T) % 5) > 0)) * 8;
		T decodedInt = 0;
		T bitOffset = 0;
		char currentByte = 0;
		do {
			if (bitOffset == max_offset) throw EnbtException("Var value too big");
			read_stream >> currentByte ;
			decodedInt |= T(currentByte & 0b01111111) << bitOffset;
			bitOffset += 7;
		} while ((currentByte & 0b10000000) != 0);
		return ENBT::ConvertEndian(endian, decodedInt);
	}

	static ENBT::Type_ID ReadTypeID(std::istream& read_stream) {
		ENBT::Type_ID result;
		read_stream >> result;
		return result;
	}
	template<class T> static T ReadValue(std::istream& read_stream, ENBT::Endian endian = ENBT::Endian::native) {
		T tmp;
		if constexpr (std::is_same<T, ENBT::UUID>()) {
			for (size_t i = 0; i < 16; i++)
				read_stream >> tmp.data[i];
			ENBT::ConvertEndian(endian, tmp.data, 16);
		}
		else {
			uint8_t* proxy = (uint8_t*)&tmp;
			for (size_t i = 0; i < sizeof(T); i++)
				read_stream >> proxy[i];
			ENBT::ConvertEndian(endian, tmp);
		}
		return tmp;
	}
	template<class T> static T* ReadArray(std::istream& read_stream,size_t len, ENBT::Endian endian = ENBT::Endian::native) {
		T* tmp = new T[len];
		if constexpr (sizeof(T) == 1) {
			for (size_t i = 0; i < len; i++)
				read_stream >> tmp[i];
		}
		else {
			for (size_t i = 0; i < len; i++)
				tmp[i] = ReadValue<T>(read_stream, endian);
		}
		return tmp;
	}


	template<class T> static T ReadDefineLen(std::istream& read_stream) {
		T tmp;
		if constexpr (std::is_same<T, ENBT::UUID>()) {
			for (size_t i = 0; i < 16; i++)
				read_stream >> tmp.data[i];
			ENBT::ConvertEndian<ENBT::Endian::little>(tmp.data, 16);
		}
		else {
			uint8_t* proxy = (uint8_t*)&tmp;
			for (size_t i = 0; i < sizeof(T); i++)
				read_stream >> proxy[i];
			ENBT::ConvertEndian<ENBT::Endian::little>(tmp);
		}
		return tmp;
	}
	static size_t ReadDefineLen(std::istream& read_stream, ENBT::Type_ID tid) {
		switch (tid.length) 
		{
		case ENBT::TypeLen::Tiny:
			return ReadDefineLen<uint8_t>(read_stream);
		case ENBT::TypeLen::Short:
			return ReadDefineLen<uint16_t>(read_stream);
		case ENBT::TypeLen::Default:
			return ReadDefineLen<uint32_t>(read_stream);
		case ENBT::TypeLen::Long: {
			uint64_t val = ReadDefineLen<uint64_t>(read_stream);
			if constexpr(sizeof(size_t)<8) {
				if ((size_t)val != val)
					throw std::overflow_error("Array length too big for this platform");
			}
			return val;
		}
		default:
			return 0;
		}
	}
	static uint64_t ReadDefineLen64(std::istream& read_stream, ENBT::Type_ID tid) {
		switch (tid.length)
		{
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
		//return ReadVar<uint64_t>(read_stream, ENBT::Endian::little);
		union {
			uint8_t complete = 0;
			struct {
				uint8_t len_flag : 2;
				uint8_t len : 6;
			} patrial;
		}b;
		read_stream >> b.complete;
		switch (b.patrial.len_flag) {
		case 0: 
			return b.complete >> 2;
		case 1: {
			uint16_t full = b.patrial.len;
			uint16_t additional;
			read_stream >> additional;
			full |= additional << 6;
			return ENBT::ConvertEndian<ENBT::Endian::little>(full);
		}
		case 2: {
			uint32_t full = b.patrial.len;
			uint32_t additional;
			read_stream >> additional;
			full |= additional << 6;
			read_stream >> additional;
			full |= additional << 14;
			read_stream >> additional;
			full |= additional << 22;
			return ENBT::ConvertEndian<ENBT::Endian::little>(full);
		}
		case 3: {
			uint64_t full = b.patrial.len;full <<= 56;
			uint64_t additional;
			read_stream >> additional;
			full |= additional << 6;
			read_stream >> additional;
			full |= additional << 14;
			read_stream >> additional;
			full |= additional << 22;
			read_stream >> additional;
			full |= additional << 30;
			read_stream >> additional;
			full |= additional << 38;
			read_stream >> additional;
			full |= additional << 46;
			read_stream >> additional;
			full |= additional << 54;
			return ENBT::ConvertEndian<ENBT::Endian::little>(full);
		}
		default:
			return 0;
		}
	}
	static std::string ReadCompoundString(std::istream& read_stream) {
		uint64_t read = ReadCompressLen(read_stream);
		std::string res;
		res.resize(read);
		for (uint64_t i = 0; i < read; i++)
			read_stream >> res[i];
		return res;
	}
	static ENBT ReadCompoud(std::istream& read_stream, ENBT::Type_ID tid) {
		size_t len = ReadDefineLen(read_stream, tid);
		std::unordered_map<std::string, ENBT> result;
		for (size_t i = 0; i < len; i++) {
			std::string key = ReadCompoundString(read_stream);
			result[key] = ReadToken(read_stream);
		}
		return result;
	}
	static std::vector<ENBT> ReadArray(std::istream& read_stream, ENBT::Type_ID tid) {
		size_t len = ReadDefineLen(read_stream, tid);
		std::vector<ENBT> result(len);
		if (tid.is_signed) {
			ENBT::Type_ID atid = ReadTypeID(read_stream);
			if (atid == ENBT::Type::bit) {
				int8_t i = 0;
				uint8_t value = 0;
				read_stream >> value;
				for (auto& it : result) {
					if (i >= 8) {
						i = 0;
						read_stream >> value;
					}
					it = (bool)(value << i);
				}
			}
			else {
				for (size_t i = 0; i < len; i++)
					result[i] = ReadValue(read_stream, atid);
			}
		}
		else {
			for (size_t i = 0; i < len; i++)
				result[i] = ReadToken(read_stream);
		}
		return result;
	}
	static ENBT Readstring(std::istream& read_stream, ENBT::Type_ID tid) {
		uint64_t len = ReadCompressLen(read_stream);
		ENBT res;
		switch (tid.length)
		{
		case ENBT::TypeLen::Tiny: 
		{
			uint8_t* arr = ReadArray<uint8_t>(read_stream, len, tid.endian);
			res = { arr,len };
			delete[] arr;
			break;
		}
		case ENBT::TypeLen::Short:
		{
			uint16_t* arr = ReadArray<uint16_t>(read_stream, len, tid.endian);
			res = { arr,len };
			delete[] arr;
			break;
		}
		case ENBT::TypeLen::Default: 
		{
			uint32_t* arr = ReadArray<uint32_t>(read_stream, len, tid.endian);
			res = { arr,len };
			delete[] arr;
			break;
		}
		case ENBT::TypeLen::Long: 
		{
			uint64_t* arr = ReadArray<uint64_t>(read_stream, len, tid.endian);
			res = { arr,len };
			delete[] arr;
			break;
		}
		default:
			throw EnbtException();
		}
		return res;
	}
	static ENBT ReadValue(std::istream& read_stream, ENBT::Type_ID tid) {
		switch (tid.type)
		{
		case ENBT::Type::integer:
			switch (tid.length)
			{
			case  ENBT::TypeLen::Tiny:
				if (tid.is_signed)
					return ReadValue<int8_t>(read_stream, tid.endian);
				else
					return ReadValue<uint8_t>(read_stream, tid.endian);
			case  ENBT::TypeLen::Short:
				if (tid.is_signed)
					return ReadValue<int16_t>(read_stream, tid.endian);
				else
					return ReadValue<uint16_t>(read_stream, tid.endian);
			case  ENBT::TypeLen::Default:
				if (tid.is_signed)
					return ReadValue<int32_t>(read_stream, tid.endian);
				else
					return ReadValue<uint32_t>(read_stream, tid.endian);
			case  ENBT::TypeLen::Long:
				if (tid.is_signed)
					return ReadValue<int64_t>(read_stream, tid.endian);
				else
					return ReadValue<uint64_t>(read_stream, tid.endian);
			default:
				return ENBT();
			}
		case ENBT::Type::floating:
			switch (tid.length)
			{
			case  ENBT::TypeLen::Default:
				return ReadValue<float>(read_stream, tid.endian);
			case  ENBT::TypeLen::Long:
				return ReadValue<double>(read_stream, tid.endian);
			default:
				return ENBT();
			}
		case ENBT::Type::ex_integer:
			switch (tid.length)
			{
			case ENBT::TypeLen::Tiny:
				return ReadCompressLen(read_stream);
			case ENBT::TypeLen::Short:
				throw EnbtException("Unsuported short ex_integer");
			case ENBT::TypeLen::Default:
				if (tid.is_signed)
					return ReadVar<int32_t>(read_stream, tid.endian);
				else
					return ReadVar<uint32_t>(read_stream, tid.endian);
			case ENBT::TypeLen::Long:
				if (tid.is_signed)
					return ReadVar<int64_t>(read_stream, tid.endian);
				else
					return ReadVar<uint64_t>(read_stream, tid.endian);
			default:
				return ENBT();
			}
		case  ENBT::Type::uuid:	    return ReadValue<ENBT::UUID>(read_stream, tid.endian);
		case ENBT::Type::string:    return Readstring(read_stream, tid);
		case ENBT::Type::compound:	return ReadCompoud(read_stream,tid);
		case ENBT::Type::array:	    return ReadArray(read_stream,tid);
		case ENBT::Type::optional:  return tid.is_signed ? ENBT(true, ReadToken(read_stream)) : ENBT(false, ENBT());
		case ENBT::Type::bit:		return ENBT((bool)tid.is_signed);
		default:					return ENBT();
		}
	}
	static ENBT ReadToken(std::istream& read_stream) {
		return ReadValue(read_stream,ReadTypeID(read_stream));
	}


	static bool CheckVersion(std::istream& read_stream) {
		return ReadValue<uint8_t>(read_stream) == ENBT_VERSION_HEX;
	}
	static bool CheckHeader(std::istream& read_stream) {
		uint8_t not_symbol;
		read_stream >> not_symbol;
		if (not_symbol != 0x89)
			return false;
		char aa[5]{ 0 };
		read_stream.readsome(aa, 4);
		return aa == std::string("ENBT");
	}

	static void SkipSignedCompoud(std::istream& read_stream, uint64_t len,bool wide) {
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
	static void SkipUnsignedCompoud(std::istream& read_stream,uint64_t len, bool wide) {
		uint8_t add = 4;
		if (wide)add = 8;
		for (uint64_t i = 0; i < len; i++) {
			SkipUnsignedCompoundString(read_stream);
			SkipToken(read_stream);
		}
	}
	static void SkipCompoud(std::istream& read_stream, ENBT::Type_ID tid) {
		uint64_t len = ReadDefineLen64(read_stream, tid);
		if (tid.is_signed)
			SkipSignedCompoud(read_stream, len, false);
		else
			SkipUnsignedCompoud(read_stream, len, true);
	}

	//return zero if canont, else return type size
	static uint8_t CanFastIndex(ENBT::Type_ID tid) {
		switch (tid.type)
		{
		case ENBT::Type::integer:
		case ENBT::Type::floating:
			switch (tid.length)
			{
			case ENBT::TypeLen::Tiny:		return 1;
			case ENBT::TypeLen::Short:		return 2;
			case ENBT::TypeLen::Default:	return 4;
			case ENBT::TypeLen::Long:		return 8;
			}
			break;
		case ENBT::Type::uuid:
			return 16;
		case ENBT::Type::bit:
			return 1;
		default:
			return 0;
		}
	}

	static void SkipSignedArray(std::istream& read_stream, ENBT::Type_ID tid) {
		int index_multipler;
		uint64_t len = ReadDefineLen64(read_stream, tid);
		if (!(index_multipler = CanFastIndex(ReadTypeID(read_stream))))
			for (uint64_t i = 0; i < len; i++)
				SkipToken(read_stream);
		else {
			if (tid == ENBT::Type::bit) {
				uint64_t actual_len = len / 8;
				if (len % 8)
					++actual_len;
				read_stream.seekg(read_stream.tellg() += actual_len);
			}
			else
				read_stream.seekg(read_stream.tellg() += len * index_multipler);
		}
	}
	static void SkipStaticArray(std::istream& read_stream, ENBT::Type_ID tid) {
		uint64_t len = ReadDefineLen64(read_stream, tid);
		for (uint64_t i = 0; i < len; i++)
			SkipToken(read_stream);
	}

	static void SkipArray(std::istream& read_stream, ENBT::Type_ID tid) {
		if (tid.is_signed)
			SkipSignedArray(read_stream, tid);
		else
			SkipStaticArray(read_stream, tid);
	}
	static void Skipstring(std::istream& read_stream, ENBT::Type_ID tid) {
		uint64_t index = ReadCompressLen(read_stream);
		switch (tid.length)
		{
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
	static void SkipValue(std::istream& read_stream,ENBT::Type_ID tid) {
		switch (tid.type)
		{
		case ENBT::Type::floating:
		case ENBT::Type::integer:
			switch (tid.length)
			{
			case  ENBT::TypeLen::Tiny:
				read_stream.seekg(read_stream.tellg() += 1); break;
			case  ENBT::TypeLen::Short:
				read_stream.seekg(read_stream.tellg() += 2); break;
			case  ENBT::TypeLen::Default:
				read_stream.seekg(read_stream.tellg() += 4); break;
			case  ENBT::TypeLen::Long:
				read_stream.seekg(read_stream.tellg() += 8); break;
			}
			break;
		case ENBT::Type::ex_integer:
			switch (tid.length)
			{
			case ENBT::TypeLen::Tiny:
				ReadCompressLen(read_stream);
				break;
			case ENBT::TypeLen::Short:
				throw EnbtException("Unsuported short ex_integer");
			case ENBT::TypeLen::Default:
				if (tid.is_signed)
					ReadVar<int32_t>(read_stream, ENBT::Endian::native);
				else
					ReadVar<uint32_t>(read_stream, ENBT::Endian::native);
				break;
			case ENBT::TypeLen::Long:
				if (tid.is_signed)
					ReadVar<int64_t>(read_stream, ENBT::Endian::native);
				else
					ReadVar<uint64_t>(read_stream, ENBT::Endian::native);
			}
			break;
		case  ENBT::Type::uuid:	read_stream.seekg(read_stream.tellg() += 16); break;
		case ENBT::Type::string:   Skipstring(read_stream, tid); break;
		case ENBT::Type::array:	SkipArray(read_stream, tid);   break;
		case ENBT::Type::compound:	SkipCompoud(read_stream, tid); break;
		case ENBT::Type::optional:	
			if (tid.is_signed) 
				SkipToken(read_stream);
			break;
		}
	}
	static void SkipToken(std::istream& read_stream) {
		return SkipValue(read_stream, ReadTypeID(read_stream));
	}

	//move read stream cursor to value in compound, return empty ENBT::Type_ID if value not found
	static ENBT::Type_ID FindValueCompound(std::istream& read_stream, ENBT::Type_ID tid, std::string key) {
		size_t len = ReadDefineLen(read_stream, tid);
		for (size_t i = 0; i < len; i++) {
			if (ReadCompoundString(read_stream) != key) SkipValue(read_stream, ReadTypeID(read_stream));
			else return ReadTypeID(read_stream);
		}
		return ENBT::Type_ID();
	}



	static void IndexSignedArray(std::istream& read_stream, uint64_t index, uint64_t len, ENBT::Type_ID targetId) {
		if (uint8_t skiper = CanFastIndex(targetId)) {
			if (targetId != ENBT::Type::bit)
				read_stream.seekg(read_stream.tellg() += index * skiper);
			else
				read_stream.seekg(read_stream.tellg() += index / 8);
		}
		else
			for (uint64_t i = 0; i < index; i++)
				SkipValue(read_stream, targetId);

	}
	static ENBT::Type_ID IndexArray(std::istream& read_stream, uint64_t index, ENBT::Type_ID arr_tid) {
		switch (arr_tid.type)
		{
		case ENBT::Type::array:
		{
			uint64_t len = ReadDefineLen64(read_stream, arr_tid);
			if (index >= len)
				throw EnbtException('[' + std::to_string(index) + "] out of range " + std::to_string(len));
			if (arr_tid.is_signed) {
				auto targetId = ReadTypeID(read_stream);
				IndexSignedArray(read_stream, index, len, targetId);
				return targetId;
			}
			else {
				for (uint64_t i = 0; i < index; i++)
					SkipToken(read_stream);
				return ReadTypeID(read_stream);
			}
			break;
		}
		default:
			throw EnbtException("Invalid type id");
		}
	}
	static ENBT::Type_ID IndexArray(std::istream& read_stream, uint64_t index) {
		return IndexArray(read_stream, index, ReadTypeID(read_stream));
	}

	//move read_stream cursor to value,
	//value_path similar: "0/the test/4/54",
	//return value ENBT::Type_ID, if value not found return empty Type_ID
	//can throw EnbtException
	static ENBT::Type_ID MoveToValuePath(std::istream& read_stream, const std::string& value_path) {
		try {
			ENBT::Type_ID last_type;
			for (auto&& tmp : SplitS(value_path, "/")) {
				auto tid = ReadTypeID(read_stream);
				switch (tid.type)
				{
				case ENBT::Type::array:
					last_type = IndexArray(read_stream, std::stoull(tmp), tid);
					continue;
				case ENBT::Type::compound:
					if ((last_type = FindValueCompound(read_stream,tid, tmp)) == ENBT::Type_ID()) return ENBT::Type_ID();
					continue;
				default:
					return ENBT::Type_ID();
				}
			}
			return last_type;
		}
		catch (const std::out_of_range&) {
			throw;
		}
		catch (const std::exception&) {
			return ENBT::Type_ID();
		}
	}

	static ENBT GetValuePath(std::istream& read_stream, const std::string& value_path) {
		auto old_pos = read_stream.tellg();
		bool is_bit_value = false;
		size_t index = 0;
		try {
			ENBT::Type_ID last_type;
			ENBT::Type_ID tid;
			for (auto&& tmp : SplitS(value_path, "/")) {
				tid = ReadTypeID(read_stream);
				switch (tid.type)
				{
				case ENBT::Type::array:
					last_type = IndexArray(read_stream, index = std::stoull(tmp), tid);
					continue;
				case ENBT::Type::compound:
					if ((last_type = FindValueCompound(read_stream, tid, tmp)) == ENBT::Type_ID()) throw EnbtException("Value not found");
					continue;
				default:
					throw std::invalid_argument("Invalid Path to Value");
				}
			}
			if (last_type.type == ENBT::Type::bit && tid.is_signed)
				return (bool)(ReadValue<uint8_t>(read_stream) << (index % 8));
			return ReadValue(read_stream, tid);
		}
		catch (...) {
			read_stream.seekg(old_pos);
			throw;
		}
		read_stream.seekg(old_pos);
	}
};


inline std::istream& operator>>(std::istream& is, ENBT& tid) {
	tid = ENBTHelper::ReadToken(is);
	return is;
}
inline std::ostream& operator<<(std::ostream& os, const ENBT& tid) {
	ENBTHelper::WriteToken(os,tid);
	return os;
}

#undef ENBT_VERSION_HEX
#undef ENBT_VERSION_STR
#endif




//TO-DO
//EBLF(enchanted binary log formats) Example file // used in encoding enbt logs 
// 
// [0x11] version
// "Some event happened in {} with this arguments {...\,}"
// "User {0} logged at {2} in {3} utc time"
// "User {0} fail register cause {...\ }"
// "Exception {1} occoured, reasons: {...\\n}"
// 
// format:
//	  every line - one item
//    \n -> next line
//    \{ -> { symbol
//    \} -> } symbol
//    {} -> mean one item from struct
//    {...} -> mean all unused elements from struct
//    {...\XXX} -> mean all unused elements from struct divided by XXX symbols
//    {...Number} -> mean use Number unused elements from struct
//    {...Number\XXX} -> mean use Number unused elements from struct divided by XXX symbols
//    {Number} -> mean value by index from structure
//    {Number..} -> mean value range from number to end in structure
//    {Number..\XXX} -> mean value range from number to end in structure divided by XXX symbols
//    {Number..Number} -> mean value range from structure
//    {Number..Number\XXX} -> mean value range from structure divided by XXX symbols
// 
// 
// 
// 
// 


//Examle
// ENBT Example file
///////                       type (1 byte)                         compound len
//	[ENBT::Type_ID{ type=compound,length = tiny}][5 (1byte unsigned)]
//		["Halo "][{ type=string,length = tiny}][7]["Мир!"]
//		["long"][{ type=integer,length = long,is_signed=true}][-9223372036854775808]
//		["uuid"][{ type=uuid}][0xD55F0C3556DA165E6F512203C78B57FF]
//		["arr"][{ type=array,length = tiny}] [3]
//			[{ type=string,length = tiny,is_signed=false}][307]["\"Pijamalı hasta yağız şoföre çabucak güvendi\"\"Victor jagt zwölf Boxkämpfer quer über den großen Sylter Deich\"\"съешь ещё этих мягких французских булок, да выпей чаю.\"\"Pranzo d'acqua fa volti sghembi\"\"Stróż pchnął kość w quiz gędźb vel fax myjń\"\"]
// 			[{ type=integer,length = long,is_signed=true}][9223372036854775807]
//			[{ type=uuid}][0xD55F0C3556DA165E6F512203C78B57FF]
//		