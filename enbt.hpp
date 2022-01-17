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
#define ENBT_VERSION_HEX 0x10
#define ENBT_VERSION_STR 1.0
//enchanted named binary tag
class EnbtException : public std::exception {
public:
	EnbtException(std::string&& reason) :std::exception(reason.c_str()) {}
	EnbtException(const char* reason) :std::exception(reason) {}
	EnbtException() :std::exception("EnbtException") {}
};

class ENBT {
public:
	struct version {      //current 1.0, max 15.15
		uint8_t Major : 4;//0x01
		uint8_t Minor : 4;//0x00
		//path not match as version cause file structure and types will always match with any path version,
		//if added new type or changed structure minor will incremented, not increment if type removed or total new simpple types count not reached 5(all types reserving, not added in new paths)
		//if minor reached 16 or structure has significant changes major increment
	};


	typedef boost::uuids::uuid UUID;
	//version 1.0
	//file structure for ENBT:
	//	version[1byte]  (0x0F -> 1.x   0xF0   -> x.0)(max 15.15)
	//	value (ex: [Type_ID][type_define][type_data]
	// 
	//file structure for ASN:
	//	version[1byte]  (0x0F -> 1.x   0xF0   -> x.0)(max 15.15)
	//	[2byte as clount] [..items]
	//		item [sero_end_str]
	// 
	
	struct Type_ID {
		enum class Endian : uint64_t {
			little, big,
			native = (unsigned char)std::endian::native
		};
		enum class Type : uint64_t {
			none,    //[0byte]
			integer,
			floating,
			var_integer,//ony default and long length
			uuid,	//[16byte]
			sarray, // [(len)]{chars} if signed, endian convert will be enabled(simple array)

			compound,	 
			//if compound is signed it will be use strings refered by name id
			//len in signed mode can be only tiny and short mode
			//[len][... items]   ((named items list))
			//					item {name id 2byte} {type_id 1byte} (value_define_and_data)
			// else will be used default string but 'string len' encoded as big endian which last 2 bits used as define 'string len' bytes len, ex[00XXXXXX], [01XXXXXX XXXXXXXX], etc... 00 - 1 byte,01 - 2 byte,02 - 4 byte,03 - 8 byte
			//[len][... items]   ((named items list))
			//					item [(len)][chars] {type_id 1byte} (value_define_and_data)

			darray,//		 [len][... items]   ((unnamed items list))
			//				item {type_id 1byte} (value_define_and_data)


			array,		//[len][type_id 1byte]{... items} /unnamed items array, if contain static value size reader can get one element without readding all elems[int,double,e.t.c..]
			//				item {value_define_and_data}
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
			unused1,
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
		//domain can fit up to 72 057 594 037 927 935 types
		enum class StandartDomain : uint64_t {

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
			} temp;
			temp.encode.is_signed = is_signed;
			temp.encode.endian = (uint8_t)endian;
			temp.encode.len = (uint8_t)length;
			temp.encode.type = (uint8_t)type;
			return temp.encoded;
		}
		std::vector<uint8_t> PostEncode() const {
			std::vector<uint8_t> tmp;
			switch (length)
			{
			case ENBT::Type_ID::LenType::Tiny:
				return { (uint8_t)domain_variant };
			case ENBT::Type_ID::LenType::Short:
			{
				union
				{
					uint16_t bp = 0;
					uint8_t lp[2];
				} build;
				build.bp = ENBT::ConvertEndian(std::endian::little, (uint16_t)domain_variant);
				tmp.insert(tmp.end(), build.lp, build.lp + 2);
				return tmp;
			}
			case ENBT::Type_ID::LenType::Default:
			{
				union
				{
					uint32_t bp = 0;
					uint8_t lp[4];
				} build;
				build.bp = ENBT::ConvertEndian(std::endian::little, (uint32_t)domain_variant);
				tmp.insert(tmp.end(), build.lp, build.lp + 4);
				return tmp;
			}
			case ENBT::Type_ID::LenType::Long:
			{
				union
				{
					uint64_t bp = 0;
					uint8_t lp[8];
				} build;
				build.bp = ENBT::ConvertEndian(std::endian::little, domain_variant);
				tmp.insert(tmp.end(), build.lp, build.lp + 8);
				return tmp;
			}
			default:
				return {};
			}
		}
		void Decode(std::vector<uint8_t> full_code) {
			PatrialDecode(full_code[1]);
			if (NeedPostDecodeEncode())
				PostDecode({ full_code.begin() + 1,full_code.end() });
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
			} temp;
			temp.encoded = basic_code;
			is_signed = temp.decoded.is_signed;
			endian = (Endian)temp.decoded.endian;
			length = (LenType)temp.decoded.len;
			type = (Type)temp.decoded.type;
		}
		bool NeedPostDecodeEncode() const {
			return type == ENBT::Type_ID::Type::domain;
		}
		void PostDecode(std::vector<uint8_t> part_code) {
			if(type != ENBT::Type_ID::Type::domain)
				throw EnbtException("Invalid typeid");
			switch (length)
			{
			case ENBT::Type_ID::LenType::Tiny:
				domain_variant = part_code[0];
				break;
			case ENBT::Type_ID::LenType::Short:
			{
				union
				{
					uint16_t bp = 0;
					uint8_t lp[2];
				} build;
				build.lp[0] = part_code[0];
				build.lp[1] = part_code[1];
				domain_variant = ConvertEndian(std::endian::little, build.bp);
				break;
			}
			case ENBT::Type_ID::LenType::Default:
			{
				union
				{
					uint32_t bp = 0;
					uint8_t lp[4];
				} build;
				build.lp[0] = part_code[0];
				build.lp[1] = part_code[1];
				build.lp[2] = part_code[2];
				build.lp[3] = part_code[3];
				domain_variant = ConvertEndian(std::endian::little, build.bp);
				break;
			}
			case ENBT::Type_ID::LenType::Long:
			{
				union
				{
					uint64_t bp = 0;
					uint8_t lp[8];
				} build;
				build.lp[0] = part_code[0];
				build.lp[1] = part_code[1];
				build.lp[2] = part_code[2];
				build.lp[3] = part_code[3];
				build.lp[4] = part_code[4];
				build.lp[5] = part_code[5];
				build.lp[6] = part_code[6];
				build.lp[7] = part_code[7];
				domain_variant = ConvertEndian(std::endian::little, build.bp);
				break;
			}
			}
			if (domain_variant < UINT8_MAX) length = LenType::Tiny;
			else if (domain_variant < UINT16_MAX)length = LenType::Short;
			else if (domain_variant < UINT32_MAX)length = LenType::Default;
			else if (domain_variant > 72057594037927935)length = LenType::Long;
			else throw EnbtException("Fail encode domain");

		}
		///// END FOR FUTURE


		std::endian getEndian() {
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

	private:
		
	};
	typedef Type_ID::Type Type;
	typedef Type_ID::LenType TypeLen;

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
			std::unordered_map<uint16_t, ENBT>*,//source pointer,
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
	static void ConvertEndian(std::endian value_endian, void* value_ptr, size_t len) {
		if (std::endian::native != value_endian)
			EndianSwap(value_ptr, len);
	}
	template<class T>
	static T ConvertEndian(std::endian value_endian, T val) {
		if (std::endian::native != value_endian)
			EndianSwap(&val, sizeof(T));
		return val;
	}
	template<class T>
	static T* ConvertEndianArr(std::endian value_endian, T* val, size_t size) {
		if (std::endian::native != value_endian)
			for (size_t i = 0; i < size; i++)
				EndianSwap(&val[i], sizeof(T));
		return val;
	}
#pragma endregion
	
	class DomainImplementation {
		virtual void Init(uint8_t*& to_init_value, Type_ID& tid,size_t& data_len) = 0;
		virtual bool NeedDestruct(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void Destruct(uint8_t*& to_destruct_value, Type_ID& tid, size_t& data_len) = 0;
		virtual uint8_t* Clone(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual size_t Size(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual ENBT& Index(uint8_t*& value, Type_ID tid, size_t data_len,size_t index) = 0;
		virtual ENBT Index(const uint8_t*& value, Type_ID tid, size_t data_len, size_t index) = 0;
		virtual ENBT& Fing(uint8_t*& value, Type_ID tid, size_t data_len, const std::string& str) = 0;
		virtual ENBT Fing(const uint8_t*& value, Type_ID tid, size_t data_len, const std::string& str) = 0; 
		virtual bool Exists(const uint8_t*& value, Type_ID tid, size_t data_len, const std::string& str) = 0;
		virtual void Remove(uint8_t*& value, Type_ID tid, size_t data_len, size_t index) = 0;
		virtual void Remove(uint8_t*& value, Type_ID tid, size_t data_len, const std::string& index) = 0;
		virtual size_t Push(uint8_t*& value, Type_ID tid, size_t data_len, ENBT& val) = 0;
		virtual ENBT Front(const uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual ENBT& Front(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void Pop(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void Resize(uint8_t*& value, Type_ID tid, size_t data_len,size_t new_size) = 0;
		virtual void* Begin(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void* End(uint8_t*& value, Type_ID tid, size_t data_len) = 0;
		virtual void Next(const uint8_t*& value, Type_ID tid, size_t data_len, void*& interator, void* end) = 0;
		virtual bool CanNext(const uint8_t*& value, Type_ID tid, size_t data_len, void* interator, void* end) = 0;
		virtual ENBT GetByPointer(const uint8_t*& value, Type_ID tid, size_t data_len, void* pointer) = 0;
		virtual ENBT& GetByPointer(uint8_t*& value, Type_ID tid, size_t data_len, void* pointer) = 0;

		virtual std::string toDENBT() = 0;
	};

	template<class T>
	static T fromVar(uint8_t* ch) {
		constexpr int max_offset = (sizeof(T) / 5 * 5 + ((sizeof(T) % 5) > 0)) * 8;
		T decodedInt = 0;
		T bitOffset = 0;
		char currentByte = 0;
		int i = 0;
		do {
			if (bitOffset == max_offset) throw EnbtException("VarInt is too big");
			currentByte = ch[i++];
			decodedInt |= (currentByte & 0b01111111) << bitOffset;
			bitOffset += 7;
		} while ((currentByte & 0b10000000) != 0);
		return decodedInt;
	}

protected:
	static EnbtValue GetContent(uint8_t* data,size_t data_len, Type_ID data_type_id) {
		uint8_t* real_data = getData(data, data_type_id,data_len);
		switch (data_type_id.type)
		{
		case ENBT::Type_ID::Type::integer:
		case ENBT::Type_ID::Type::var_integer:
			switch (data_type_id.length)
			{
			case  ENBT::Type_ID::LenType::Tiny:
				if (data_type_id.is_signed)
					return (int8_t)real_data[0];
				else
					return (uint8_t)real_data[0];
			case  ENBT::Type_ID::LenType::Short:
				if (data_type_id.is_signed)
					return ((int16_t*)real_data)[0];
				else
					return ((uint16_t*)real_data)[0];
			case  ENBT::Type_ID::LenType::Default:
				if (data_type_id.is_signed)
					return ((int32_t*)real_data)[0];
				else
					return ((uint32_t*)real_data)[0];
			case  ENBT::Type_ID::LenType::Long:
				if (data_type_id.is_signed)
					return ((int64_t*)real_data)[0];
				else
					return ((uint64_t*)real_data)[0];
			default:
				return nullptr;
			}
		case ENBT::Type_ID::Type::floating:
			switch (data_type_id.length)
			{
			case  ENBT::Type_ID::LenType::Default:
				return ((float*)real_data)[0];
			case  ENBT::Type_ID::LenType::Long:
				return ((double*)real_data)[0];
			default:
				return nullptr;
			}
		case ENBT::Type_ID::Type::uuid:		return *(UUID*)real_data;
		case ENBT::Type_ID::Type::sarray:
			switch (data_type_id.length)
			{
			case ENBT::Type_ID::LenType::Tiny:
				return data;
			case ENBT::Type_ID::LenType::Short:
				return (uint16_t*)data;
			case ENBT::Type_ID::LenType::Default:
				return (uint32_t*)data;
			case ENBT::Type_ID::LenType::Long:
				return (uint64_t*)data;
			default:
				return data;
			}
			return real_data;
		case ENBT::Type_ID::Type::array:
		case ENBT::Type_ID::Type::darray:	return ((std::vector<ENBT>*)data);
		case ENBT::Type_ID::Type::compound: {
			if(data_type_id.is_signed)
				return ((std::unordered_map<uint16_t, ENBT>*)data);
			else
				return ((std::unordered_map<std::string, ENBT>*)data);
		}
		case ENBT::Type_ID::Type::structure:
		case ENBT::Type_ID::Type::optional: if (data) return ((ENBT*)data); else return nullptr;
		case ENBT::Type_ID::Type::bit: return (bool)data_type_id.is_signed;
		default:							return nullptr;
		}
	}

	static uint8_t* CloneData(uint8_t* data, Type_ID data_type_id, size_t data_len) {
		switch (data_type_id.type)
		{
		case ENBT::Type_ID::Type::sarray:
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
		case ENBT::Type_ID::Type::array:
		case ENBT::Type_ID::Type::darray:
			return (uint8_t*)new std::vector<ENBT>(*(std::vector<ENBT>*)data);
		case ENBT::Type_ID::Type::compound:
			if(data_type_id.is_signed)
				return (uint8_t*)new std::unordered_map<uint16_t, ENBT>(*(std::unordered_map<uint16_t, ENBT>*)data);
			else
				return (uint8_t*)new std::unordered_map<std::string, ENBT>(*(std::unordered_map<std::string, ENBT>*)data);
		case ENBT::Type_ID::Type::structure:
		{
			ENBT* cloned = new ENBT[data_len];
			ENBT* source = (ENBT*)data;
			for (size_t i = 0; i < data_len; i++)
				cloned[i] = source[i];
			return (uint8_t*)cloned; 
		}
		case ENBT::Type_ID::Type::optional:
			if (data)
				return (uint8_t*)new ENBT((ENBT*)data);
			else
				return nullptr;
		case ENBT::Type_ID::Type::uuid:
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
	uint8_t* CloneData() const  {
		return CloneData(data, data_type_id, data_len);
	}

	static uint8_t* getData(uint8_t*& data, Type_ID data_type_id, size_t data_len) {
		if (NeedFree(data_type_id, data_len))
			return data;
		else
			return (uint8_t*)&data;
	}
	static bool NeedFree(Type_ID data_type_id, size_t data_len) {
		switch (data_type_id.type)
		{
		case ENBT::Type_ID::Type::integer:
		case ENBT::Type_ID::Type::floating:
			return false;
		default:
			return true;
		}
	}
	static void FreeData(uint8_t* data, Type_ID data_type_id, size_t data_len) {
		if (data == nullptr)
			return;
		switch (data_type_id.type)
		{
		case ENBT::Type_ID::Type::integer:
		case ENBT::Type_ID::Type::floating:
			break;
		case ENBT::Type_ID::Type::array:
		case ENBT::Type_ID::Type::darray:
			delete (std::vector<ENBT>*)data;
			break;
		case ENBT::Type_ID::Type::compound:
			if(data_type_id.is_signed)
				delete (std::unordered_map<uint16_t, ENBT>*)data;
			else
				delete (std::unordered_map<std::string, ENBT>*)data;
			break;
		case  ENBT::Type_ID::Type::structure:
			delete[] (ENBT*)data;
			break;
		case ENBT::Type_ID::Type::optional:
			delete (ENBT*)data;
			break;
		case ENBT::Type_ID::Type::uuid:
			delete (UUID*)data;
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

	template<class T>
	void SetData(T val) {
		data_len = sizeof(data_len);
		if (data_len <= 8 && data_type_id.type != Type::uuid) {
			data = nullptr;
			char* prox0 = (char*)&data;
			char* prox1 = (char*)&val;
			for (size_t i = 0; i < data_len; i++)
				prox0[i] = prox1[i];
		}
		else {
			FreeData(data,data_type_id,data_len);
			data = (uint8_t*)new T(val);
		}
	}
	template<class T>
	void SetData(T* val,size_t len) {
		data_len = len * sizeof(T);
		if (data_len <= 8) {
			char* prox0 = (char*)data;
			char* prox1 = (char*)val;
			for (size_t i = 0; i < data_len; i++)
				prox0[i] = prox1[i];
		}
		else {
			FreeData(data, data_type_id, data_len);
			T* tmp = new T[len / sizeof(T)];
			for(size_t i=0;i<len;i++)
				tmp[i] = val[i];
			data = (uint8_t*)tmp;
		}
	}

	static const char** global_strings;
	static uint16_t total_strings;

	template <class T>
	static size_t len(T* val) {
		T* len_calc = val;
		size_t size = 1;
		while (*len_calc++)size++;
		return size;
	}

	void checkLen(Type_ID tid,size_t len) {
		switch (tid.length)
		{
		case ENBT::Type_ID::LenType::Tiny:
			if (tid.is_signed) {
				if (len > INT8_MAX)
					throw EnbtException("Invalid tid");
			}
			else {
				if (len > UINT8_MAX)
					throw EnbtException("Invalid tid");
			}
			break;
		case ENBT::Type_ID::LenType::Short:
			if (tid.is_signed) {
				if (len > INT16_MAX)
					throw EnbtException("Invalid tid");
			}
			else {
				if (len > UINT16_MAX)
					throw EnbtException("Invalid tid");
			}
			break;
		case ENBT::Type_ID::LenType::Default:
			if (tid.is_signed) {
				if (len > INT32_MAX)
					throw EnbtException("Invalid tid");
			}
			else {
				if (len > UINT32_MAX)
					throw EnbtException("Invalid tid");
			}
			break;
		case ENBT::Type_ID::LenType::Long:
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
public:
	ENBT() { data = nullptr; data_len = 0; data_type_id = Type_ID{ Type_ID::Type::none,Type_ID::LenType::Tiny }; }
	template<class T>
	ENBT(const std::vector<T>& array) {
		data_len = array.size();
		data_type_id.type = Type_ID::Type::array;
		data_type_id.is_signed = 0;
		data_type_id.endian = Type_ID::Endian::native;
		if (data_len <= UINT8_MAX)
			data_type_id.length = Type_ID::LenType::Tiny;
		else if (data_len <= UINT16_MAX)
			data_type_id.length = Type_ID::LenType::Short;
		else if (data_len <= UINT32_MAX)
			data_type_id.length = Type_ID::LenType::Default;
		else
			data_type_id.length = Type_ID::LenType::Long;
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
			data_type_id.type = Type_ID::Type::array;
		else
			data_type_id.type = Type_ID::Type::darray;
		if (data_len <= UINT8_MAX)
			data_type_id.length = Type_ID::LenType::Tiny;
		else if (data_len <= UINT16_MAX)
			data_type_id.length = Type_ID::LenType::Short;
		else if (data_len <= UINT32_MAX)
			data_type_id.length = Type_ID::LenType::Default;
		else
			data_type_id.length = Type_ID::LenType::Long;
		data_type_id.is_signed = 0;
		data_type_id.endian = Type_ID::Endian::native;
		data = (uint8_t*)new std::vector<ENBT>(array);
	}
	ENBT(const std::vector<ENBT>& array, Type_ID tid) {
		if (tid.type == ENBT::Type_ID::Type::darray);
		else if (tid.type == ENBT::Type_ID::Type::array) {
			if (array.size()) {
				Type_ID tid_check = array[0].type_id();
				for (auto& check : array)
					if (!check.type_equal(tid_check))
						throw EnbtException("Invalid tid");
			}
		}
		else 
			throw EnbtException("Invalid tid");
		checkLen(tid, array.size());
		data_type_id = tid;
		data = (uint8_t*)new std::vector<ENBT>(array);
	}
	ENBT(const std::unordered_map<uint16_t, ENBT>& compound, Type_ID::LenType len_type = Type_ID::LenType::Short) {
		data_type_id = Type_ID{ Type_ID::Type::compound,len_type,true };
		switch (len_type)
		{
		case ENBT::Type_ID::LenType::Tiny:
		case ENBT::Type_ID::LenType::Short:
			checkLen(data_type_id, compound.size()); break;
		default:
			throw EnbtException("Invalid tid");
		}
		data = (uint8_t*)new std::unordered_map<uint16_t, ENBT>(compound);
	}
	ENBT(const std::unordered_map<std::string, ENBT>& compound, Type_ID::LenType len_type = Type_ID::LenType::Long) {
		data_type_id = Type_ID{ Type_ID::Type::compound,len_type,false };
		checkLen(data_type_id, compound.size()); 
		data = (uint8_t*)new std::unordered_map<std::string, ENBT>(compound);
	}

	ENBT(const uint8_t* utf8_str)  {
		size_t size = len(utf8_str);
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Tiny,false };
		uint8_t* str = new uint8_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const uint16_t* utf16_str,std::endian str_endian = std::endian::native, bool convert_endian = true) {
		size_t size = len(utf16_str);
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Short, str_endian,false };
		uint16_t* str = new uint16_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf16_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, size);

		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const uint32_t* utf32_str,std::endian str_endian = std::endian::native, bool convert_endian = true) {
		size_t size = len(utf32_str);
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Default, str_endian,false };
		uint32_t* str = new uint32_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf32_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, size);
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const uint64_t* utf64_str, std::endian str_endian = std::endian::native, bool convert_endian = true) {
		size_t size = len(utf64_str);
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Default, str_endian,false };
		uint64_t* str = new uint64_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf64_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, size);
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const uint8_t* utf8_str,size_t slen) {
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Tiny,false };
		uint8_t* str = new uint8_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const uint16_t* utf16_str, size_t slen,std::endian str_endian = std::endian::native, bool convert_endian = true) {
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Short, str_endian,false };
		uint16_t* str = new uint16_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf16_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, slen);


		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const uint32_t* utf32_str, size_t slen,std::endian str_endian = std::endian::native, bool convert_endian = true)  {
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Default, str_endian,false };
		uint32_t* str = new uint32_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf32_str[i];
		if(convert_endian)
			ConvertEndianArr(str_endian, str, slen);
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const uint64_t* utf64_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true) {
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Long, str_endian,false };
		uint64_t* str = new uint64_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf64_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, slen);
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const int8_t* utf8_str) {
		size_t size = len(utf8_str);
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Tiny,true };
		int8_t* str = new int8_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const int16_t* utf16_str, std::endian str_endian = std::endian::native, bool convert_endian = true) {
		size_t size = len(utf16_str);
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Short, str_endian,true };
		int16_t* str = new int16_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf16_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, size);

		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const int32_t* utf32_str, std::endian str_endian = std::endian::native, bool convert_endian = true) {
		size_t size = len(utf32_str);
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Default, str_endian,true };
		int32_t* str = new int32_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf32_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, size);
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const int64_t* utf64_str, std::endian str_endian = std::endian::native, bool convert_endian = true) {
		size_t size = len(utf64_str);
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Default, str_endian,true };
		int64_t* str = new int64_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf64_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, size);
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const int8_t* utf8_str, size_t slen) {
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Tiny,true };
		int8_t* str = new int8_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const int16_t* utf16_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true) {
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Short, str_endian,true };
		int16_t* str = new int16_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf16_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, slen);


		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const int32_t* utf32_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true) {
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Default, str_endian,true };
		int32_t* str = new int32_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf32_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, slen);
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const int64_t* utf64_str, size_t slen, std::endian str_endian = std::endian::native, bool convert_endian = true) {
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Long, str_endian,true };
		int64_t* str = new int64_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf64_str[i];
		if (convert_endian)
			ConvertEndianArr(str_endian, str, slen);
		data = (uint8_t*)str;
		data_len = slen;
	}
	ENBT(const char* utf8_str) {
		constexpr bool char_is_signed = (char)-1 < 0;
		size_t size = len(utf8_str);
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Tiny,char_is_signed };
		uint8_t* str = new uint8_t[size];
		for (size_t i = 0; i < size; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = size;
	}
	ENBT(const char* utf8_str, size_t slen) {
		constexpr bool char_is_signed = (char)-1 < 0;
		data_type_id = Type_ID{ Type_ID::Type::sarray,Type_ID::LenType::Tiny,char_is_signed };
		int8_t* str = new int8_t[slen];
		for (size_t i = 0; i < slen; i++)
			str[i] = utf8_str[i];
		data = (uint8_t*)str;
		data_len = slen;
	}


	ENBT(UUID uuid, std::endian endian = std::endian::native, bool convert_endian = false) {
		data_type_id = Type_ID{ Type_ID::Type::uuid };
		SetData(uuid);
		if (convert_endian)
			ConvertEndian(endian, data, data_len);
	}
	ENBT(bool byte) {
		data_type_id = Type_ID{ Type_ID::Type::bit,Type_ID::LenType::Tiny,byte };
		data_len = 0;
	}
	ENBT(int8_t byte) {
		SetData(byte);
		data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Tiny,true };
	}
	ENBT(uint8_t byte) {
		SetData(byte);
		data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Tiny };
	}
	ENBT(int16_t sh, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &sh, sizeof(int16_t));
		SetData(sh);
		data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Short,endian,true };
	}
	ENBT(int32_t in,bool as_var, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &in, sizeof(int32_t));
		SetData(in);
		if(as_var)
			data_type_id = Type_ID{ Type_ID::Type::var_integer,Type_ID::LenType::Default,endian,true };
		else
			data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Default,endian,true };
	}
	ENBT(int64_t lon, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &lon, sizeof(int64_t));
		SetData(lon);
		if (as_var)
			data_type_id = Type_ID{ Type_ID::Type::var_integer,Type_ID::LenType::Long,endian,true };
		else
			data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Long,endian,true };
	}
	ENBT(int32_t in, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &in, sizeof(int32_t));
		SetData(in);
		data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Default,endian,true };
	}
	ENBT(int64_t lon, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &lon, sizeof(int64_t));
		SetData(lon);
		data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Long,endian,true };
	}
	ENBT(uint16_t sh, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &sh, sizeof(uint16_t));
		SetData(sh);
		data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Short,endian };
	}
	ENBT(uint32_t in, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &in, sizeof(uint32_t));
		SetData(in);
		if (as_var)
			data_type_id = Type_ID{ Type_ID::Type::var_integer,Type_ID::LenType::Default,endian,false };
		else
			data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Default,endian,false };
	}
	ENBT(uint64_t lon, bool as_var, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &lon, sizeof(uint64_t));
		SetData(lon);
		if (as_var)
			data_type_id = Type_ID{ Type_ID::Type::var_integer,Type_ID::LenType::Long,endian,false };
		else
			data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Long,endian,false };
	}
	ENBT(uint32_t in, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &in, sizeof(uint32_t));
		SetData(in);
		data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Default,endian,false };
	}
	ENBT(uint64_t lon, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &lon, sizeof(uint64_t));
		SetData(lon);
		data_type_id = Type_ID{ Type_ID::Type::integer,Type_ID::LenType::Long,endian,false };
	}
	ENBT(float flo, std::endian endian = std::endian::native, bool convert_endian = false) {
		if (convert_endian)
			ConvertEndian(endian, &flo, sizeof(float));
		SetData(flo);
		data_type_id = Type_ID{ Type_ID::Type::floating,Type_ID::LenType::Default,endian };
	}
	ENBT(double dou, std::endian endian = std::endian::native, bool convert_endian = false) {
		if(convert_endian)
			ConvertEndian(endian, &dou, sizeof(double));
		SetData(dou);
		data_type_id = Type_ID{ Type_ID::Type::floating,Type_ID::LenType::Long,endian };
	}
	ENBT(EnbtValue val, Type_ID tid,size_t length, bool convert_endian = true) {
		data_type_id = tid;
		data_len = 0;
		switch (tid.type)
		{
		case ENBT::Type_ID::Type::integer:
			switch (data_type_id.length)
			{
			case  ENBT::Type_ID::LenType::Tiny:
				if (data_type_id.is_signed)
					SetData(std::get<int8_t>(val));
				else
					SetData(std::get<uint8_t>(val));
				break;
			case  ENBT::Type_ID::LenType::Short:
				if (data_type_id.is_signed)
					SetData(std::get<int16_t>(val));
				else
					SetData(std::get<uint16_t>(val));
				break;
			case  ENBT::Type_ID::LenType::Default:
				if (data_type_id.is_signed)
					SetData(std::get<int32_t>(val));
				else
					SetData(std::get<uint32_t>(val));
				break;
			case  ENBT::Type_ID::LenType::Long:
				if (data_type_id.is_signed)
					SetData(std::get<int64_t>(val));
				else
					SetData(std::get<uint64_t>(val));
			}
			break;
		case ENBT::Type_ID::Type::floating:
			switch (data_type_id.length)
			{
			case  ENBT::Type_ID::LenType::Default:
				SetData(std::get<float>(val));
				break;
			case  ENBT::Type_ID::LenType::Long:
				SetData(std::get<double>(val));
			}
			break;
		case ENBT::Type_ID::Type::var_integer:
			switch (data_type_id.length)
			{
			case  ENBT::Type_ID::LenType::Default:
				if (data_type_id.is_signed)
					SetData(std::get<int32_t>(val));
				else
					SetData(std::get<uint32_t>(val));
				break;
			case  ENBT::Type_ID::LenType::Long:
				if (data_type_id.is_signed)
					SetData(std::get<int64_t>(val));
				else
					SetData(std::get<uint64_t>(val));
			}
			break;
		case ENBT::Type_ID::Type::uuid:SetData(std::get<uint8_t*>(val), 16); break;
		case ENBT::Type_ID::Type::sarray: 
		{
			switch (data_type_id.length) {
			case  ENBT::Type_ID::LenType::Tiny:
				SetData(std::get<uint8_t*>(val), len(std::get<uint8_t*>(val)));
				break;
			case  ENBT::Type_ID::LenType::Short:
				SetData(std::get<uint16_t*>(val), len(std::get<uint16_t*>(val)));
				break;
			case  ENBT::Type_ID::LenType::Default:
				SetData(std::get<uint32_t*>(val), len(std::get<uint32_t*>(val)));
				break;
			case  ENBT::Type_ID::LenType::Long:
				SetData(std::get<uint64_t*>(val), len(std::get<uint64_t*>(val)));
			}
			tid.is_signed = convert_endian;
			break;
		}
		case ENBT::Type_ID::Type::array:
		case ENBT::Type_ID::Type::darray:SetData(new std::vector<ENBT>(*std::get<std::vector<ENBT>*>(val))); break;
		case ENBT::Type_ID::Type::compound:
			if(tid.is_signed)
				SetData(new std::unordered_map<uint16_t, ENBT>(*std::get<std::unordered_map<uint16_t, ENBT>*>(val)));
			else
				SetData(new std::unordered_map<std::string, ENBT>(*std::get<std::unordered_map<std::string, ENBT>*>(val)));
			break;
		case ENBT::Type_ID::Type::structure:SetData(CloneData((uint8_t*)std::get<ENBT*>(val), tid, length)); data_len = length; break;
		case ENBT::Type_ID::Type::bit:SetData(CloneData((uint8_t*)std::get<bool>(val), tid, length)); data_len = length; break;
		default:
			data = nullptr;
			data_len = 0;
		}
	}
	ENBT(ENBT* structureValues, size_t elems, Type_ID::LenType len_type = Type_ID::LenType::Tiny) {
		data_type_id = Type_ID{ Type_ID::Type::structure,len_type };
		if (!structureValues)
			throw EnbtException("structure is nullptr");
		if(!elems)
			throw EnbtException("structure canont be zero elements");
		checkLen(data_type_id, elems * 4);
		data = CloneData((uint8_t*)structureValues, data_type_id, elems);
		data_len = elems;
	}

	ENBT(Type_ID tid,size_t len = 0) {
		switch (tid.type)
		{
		case Type_ID::Type::compound:
			if(tid.is_signed)
				operator=(ENBT(std::unordered_map<uint16_t, ENBT>(), tid.length));
			else
				operator=(ENBT(std::unordered_map<std::string, ENBT>(), tid.length));
			break;
		case Type_ID::Type::array:
		case Type_ID::Type::darray:
			operator=(ENBT(std::vector<ENBT>(len),tid));
			break;
		case Type_ID::Type::sarray:
			if (len) {
				switch (tid.length)
				{
				case Type_ID::LenType::Tiny:
					data = new uint8_t[len](0);
					break;
				case Type_ID::LenType::Short:
					data = (uint8_t*)new uint16_t[len](0);
					break;
				case Type_ID::LenType::Default:
					data = (uint8_t*)new uint32_t[len](0);
					break;
				case Type_ID::LenType::Long:
					data = (uint8_t*)new uint64_t[len](0);
					break;
				}
			}
			data_type_id = tid;
			data_len = len;

			break;
		case Type_ID::Type::optional:
			data_type_id = tid;
			data_type_id.is_signed = false;
			data_len = 0;
			data = nullptr;
			break;
		default:
			operator=(ENBT());
		}
	}

	ENBT(Type_ID::Type typ, size_t len = 0) {
		switch (len) {
		case 0:
			operator=(Type_ID(typ,Type_ID::LenType::Tiny, false));
			break;
		case 1:
			operator=(Type_ID(typ, Type_ID::LenType::Short, false));
			break;
		case 2:
			operator=(Type_ID(typ, Type_ID::LenType::Default, false));
			break;
		case 3:
			operator=(Type_ID(typ, Type_ID::LenType::Long, false));
			break;
		default:
			operator=(Type_ID(typ, Type_ID::LenType::Default, false));
		}
	}

	ENBT(const ENBT& copy) {
		operator=(copy);
	}	
	ENBT(bool optional, ENBT&& value) {
		if (optional) {
			data_type_id = Type_ID(Type_ID::Type::optional, Type_ID::LenType::Tiny, true);
			data = (uint8_t*)new ENBT(std::move(value));
		}
		else {
			data_type_id = Type_ID(Type_ID::Type::optional, Type_ID::LenType::Tiny, false);
			data = nullptr;
		}
		data_len = 0;
	}
	ENBT(bool optional, const ENBT& value) {
		if (optional) {
			data_type_id = Type_ID(Type_ID::Type::optional, Type_ID::LenType::Tiny, true);
			data = (uint8_t*)new ENBT(value);
		}
		else {
			data_type_id = Type_ID(Type_ID::Type::optional, Type_ID::LenType::Tiny, false);
			data = nullptr;
		}
		data_len = 0;
	}
	ENBT(ENBT&& copy) noexcept {
		operator=(std::move(copy));
	}
	ENBT& operator=(ENBT&& copy) noexcept {
		data = copy.data;
		copy.data = nullptr;
		data_len = copy.data_len;
		data_type_id = copy.data_type_id;
		return *this;
	}
	~ENBT() {
		FreeData(data, data_type_id, data_len);
	}

	ENBT& operator=(const ENBT& copy) {
		data = copy.CloneData();
		data_len = copy.data_len;
		data_type_id = copy.data_type_id;
		return *this;
	}
	template<class T>
	ENBT& operator=(const T& set_value) {
		if constexpr (std::is_same<T, uint8_t>().value || std::is_same<T, int8_t>().value || std::is_same<T, uint16_t>().value || std::is_same<T, int16_t>().value || std::is_same<T, uint32_t>().value || std::is_same<T, int32_t>().value || std::is_same<T, uint64_t>().value || std::is_same<T, int64_t>().value || std::is_same<T, float>().value || std::is_same<T, double>().value)
		{
			if (data_type_id.type == Type::integer || data_type_id.type == Type::var_integer) {
				switch (data_type_id.length)
				{
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
			}
			else if (data_type_id.type == Type::floating) {
				switch (data_type_id.length)
				{
				case TypeLen::Default:
					SetData((float)set_value);
					break;
				case TypeLen::Long:
					SetData((double)set_value);
					break;
				default:
					break;
				}
			}
			else operator=(ENBT(set_value));
		}
		else if constexpr (std::is_same<T, UUID>().value)
			SetData(set_value);
		else if constexpr (std::is_same<T, EnbtValue>().value)
			operator=(ENBT(set_value, data_type_id,data_len,data_type_id.endian));
		else if constexpr (std::is_same<T, bool>().value) {
			if (data_type_id.type == Type::bit)
				data_type_id.is_signed = set_value;
			else
				operator=(ENBT(set_value));
		}
		else
			operator=(ENBT(set_value));

		return *this;
	}

	bool type_equal(Type_ID tid) const  {
		return !( data_type_id != tid);
	}
	bool is_compoud() const {
		return data_type_id.type == Type_ID::Type::compound;
	}
	bool is_tiny_compoud() const {
		if (data_type_id.type != Type_ID::Type::compound)
			return false;
		return data_type_id.is_signed;
	}
	bool is_long_compoud() const {
		if (data_type_id.type != Type_ID::Type::compound)
			return false;
		return !data_type_id.is_signed;
	}
	bool is_array() const {
		return data_type_id.type == Type_ID::Type::array || data_type_id.type == Type_ID::Type::darray;
	}
	bool is_sarray() const {
		return data_type_id.type == Type_ID::Type::sarray;
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

	static std::vector<std::string> GetAStrings() {
		std::vector<std::string> astr;
		astr.resize(total_strings);
		for (uint16_t s = 0; s < total_strings; s++)
			astr[s] = global_strings[s];
		return astr;
	}
	static void SetAStrings(const std::vector<std::string>& alias_strings) {
		uint16_t ts = alias_strings.size();
		if (ts != alias_strings.size())
			throw std::out_of_range("Too many strings to be fit in uint16_t");

		char** astrs = new char*[ts];
		for (uint16_t i = 0; i < ts; i++) {
			size_t str_len = alias_strings[i].size();
			auto to_copy = alias_strings[i].c_str();
			auto to_paste = astrs[i] = new char[str_len];
			for (size_t j = 0; j < str_len; j++)
				to_paste[j] = to_copy[j];
		}
		if (global_strings)
			delete[] global_strings;
		total_strings = ts;
		global_strings = (const char**)astrs;
	}
	static uint16_t ToAliasedStr(const char* str) {
		for (uint16_t i = 0; i < total_strings; i++)
			if (strcmp(str, global_strings[i]) == 0)
				return i;
		throw EnbtException("NotFound");
	}
	static const char* FromAliasedStr(uint16_t index) {
		if (index >= total_strings)
			return global_strings[index];
		else throw EnbtException("Out of range in alias strings");
	}
	static bool existAliasedStr(const char* str) {
		for (uint16_t i = 0; i < total_strings; i++)
			if (strcmp(str, global_strings[i]) == 0)
				return true;
		return false;
	}
	static bool existAliasedStr(uint16_t id) {
		return total_strings > id;
	}
	ENBT& getByCompoundKey(uint16_t key) {
		if (data_type_id.type == Type_ID::Type::compound) {
			if (data_type_id.is_signed)
				return (*(std::unordered_map<uint16_t, ENBT>*)data)[key];
			else 
				return (*(std::unordered_map<std::string, ENBT>*)data)[FromAliasedStr(key)];
		}
		else
			throw EnbtException("This enbt is not compound");
	}
	bool removeCompoundKey(uint16_t key) {
		if (data_type_id.type == Type_ID::Type::compound) {
			if (data_type_id.is_signed){
				if ((*(std::unordered_map<uint16_t, ENBT>*)data).contains(key)) {
					(*(std::unordered_map<uint16_t, ENBT>*)data).erase(key);
					return true;
				}
			}
			else {
				auto alased = FromAliasedStr(key);
				if ((*(std::unordered_map<std::string, ENBT>*)data).contains(alased)) {
					(*(std::unordered_map<std::string, ENBT>*)data).erase(alased);
					return true;
				}
			}
		}
		return false;
	}

	size_t size() const {
		return data_len;
	}
	Type_ID type_id() const {return data_type_id;}

	EnbtValue content() const {
		return GetContent(data, data_len, data_type_id);
	}

	void setOptional(const ENBT& value) {
		if (data_type_id.type == Type_ID::Type::optional) {
			data_type_id.is_signed = true;
			FreeData(data, data_type_id, data_len);
			data = (uint8_t*)new ENBT(value);
		}
	}
	void setOptional(ENBT&& value) {
		if (data_type_id.type == Type_ID::Type::optional) {
			data_type_id.is_signed = true;
			FreeData(data,data_type_id,data_len);
			data = (uint8_t*)new ENBT(std::move(value));
		}
	}
	void setOptional() {
		if (data_type_id.type == Type_ID::Type::optional) {
			FreeData(data,data_type_id,data_len);
			data_type_id.is_signed = false;
		}
	}

	const ENBT* getOptional() const {
		if (data_type_id.type == Type_ID::Type::optional)
			if (data_type_id.is_signed)
				return (ENBT*)data;
		return nullptr;
	}
	ENBT* getOptional() {
		if (data_type_id.type == Type_ID::Type::optional)
			if (data_type_id.is_signed)
				return (ENBT*)data;
		return nullptr;
	}

	bool contains() const {
		if (data_type_id.type == Type_ID::Type::optional)
			if (data_type_id.is_signed)
				return true;
		return data_type_id.type != Type_ID::Type::none;
	}
	bool contains(const char* index) const {
		if (is_compoud()) {
			if (data_type_id.is_signed)
				return ((std::unordered_map<uint16_t, ENBT>*)data)->contains(ToAliasedStr(index));
			else 
				return ((std::unordered_map<std::string, ENBT>*)data)->contains(index);
		}
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
	uint64_t getTypeDomain() const {
		if (data_type_id.type == Type::domain)
			return data_type_id.domain_variant;
		else throw EnbtException("The not domain");
	}

	const uint8_t* getPtr() const {
		return data;
	}

	void remove(size_t index) {
		if (is_array()) 
			((std::vector<ENBT>*)data)->erase(((std::vector<ENBT>*)data)->begin() + index);
		else throw EnbtException("Cannont remove item from non array type");
	}
	void remove(std::string name);
	size_t push(const ENBT& enbt) {
		if (is_array()) {
			if (data_type_id.type == Type_ID::Type::array) {
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
		if (is_array()) {
			if(!data_len)
				throw EnbtException("Array empty");
			return ((std::vector<ENBT>*)data)->front();
		}
		else throw EnbtException("Cannont get front item from non array type");
	}
	void pop() {
		if (is_array()) {
			if (!data_len)
				throw EnbtException("Array empty");
			((std::vector<ENBT>*)data)->pop_back();
		}
		else throw EnbtException("Cannont pop front item from non array type");
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
		}
		else if (is_sarray()) {
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
			case Type::sarray:
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
			case Type::darray:
				return (*std::get<std::vector<ENBT>*>(content())) == (*std::get<std::vector<ENBT>*>(enbt.content()));
			case Type::compound:
				return (*std::get<std::unordered_map<uint16_t, ENBT>*>(content())) == (*std::get<std::unordered_map<uint16_t, ENBT>*>(enbt.content()));
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
	operator std::string() const {
		return (char*)std::get<uint8_t*>(content());
	}
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
		return *std::get<std::vector<ENBT>*>(content());
	}
	template<class T>
	operator std::vector<T>() const {
		std::vector<T> res;
		res.reserve(size());
		std::vector<ENBT>& tmp = *std::get<std::vector<ENBT>*>(content());
		for (auto& temp : tmp)
			res.push_back(std::get<T>(temp.content()));
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
			case ENBT::Type::darray:
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
			case ENBT::Type::darray:
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
			case ENBT::Type::darray:
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
			case ENBT::Type::darray:
				(*(std::vector<ENBT>::iterator*)pointer)--;
				break;
			case ENBT::Type::compound:
				if (interate_type.is_signed)
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer)--;
				else
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
			case ENBT::Type::darray:
				return
					(*(std::vector<ENBT>::iterator*)pointer)
					==
					(*(std::vector<ENBT>::iterator*)pointer);
			case ENBT::Type::compound:
				if (interate_type.is_signed)
					return
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer)
					==
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)interator.pointer);
				else
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
			case ENBT::Type::darray:
				return
					(*(std::vector<ENBT>::iterator*)pointer)
					!=
					(*(std::vector<ENBT>::iterator*)pointer);
			case ENBT::Type::compound:
				if (interate_type.is_signed)
					return
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer)
					!=
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)interator.pointer);
				else
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
			case ENBT::Type::darray:
				return { "", *(*(std::vector<ENBT>::iterator*)pointer) };
			case ENBT::Type::compound:
				if (interate_type.is_signed)
				{
					auto& tmp = (*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer);
					return std::pair<const std::string, const ENBT&>(
						std::string(ENBT::FromAliasedStr(tmp->first)),
						tmp->second
						);
				}
				else
				{
					auto& tmp = (*(std::unordered_map<std::string, ENBT>::iterator*)pointer);
					return std::pair<const std::string, const ENBT&>(
						tmp->first,
						tmp->second
					);
				}
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
			case ENBT::Type::darray:
				return { "", *(*(std::vector<ENBT>::iterator*)pointer) };
			case ENBT::Type::compound:
				if (interate_type.is_signed)
				{
					auto& tmp = (*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer);
					return std::pair<const std::string, ENBT&>(
						std::string(ENBT::FromAliasedStr(tmp->first)),
						tmp->second
					);
				}
				else
				{
					auto& tmp = (*(std::unordered_map<std::string, ENBT>::iterator*)pointer);
					return std::pair<const std::string, ENBT&>(
						tmp->first,
						tmp->second
					);
				}
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
			case ENBT::Type::sarray:
				if(in_begin)
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


		CopyInterator& operator++() {
			switch (interate_type.type)
			{
			case ENBT::Type::sarray:
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
			case ENBT::Type::darray:
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
		CopyInterator operator++(int) {
			CopyInterator temp = *this;
			operator++();
			return temp;
		}
		CopyInterator& operator--() {
			switch (interate_type.type)
			{
			case ENBT::Type::sarray:
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
			case ENBT::Type::darray:
				(*(std::vector<ENBT>::iterator*)pointer)--;
				break;
			case ENBT::Type::compound:
				if (interate_type.is_signed)
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer)--;
				else
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
			case ENBT::Type::sarray:
				return pointer == interator.pointer;
			case ENBT::Type::array:
			case ENBT::Type::darray:
				return
					(*(std::vector<ENBT>::iterator*)pointer)
					==
					(*(std::vector<ENBT>::iterator*)pointer);
			case ENBT::Type::compound:
				if (interate_type.is_signed)
					return
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer)
					==
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)interator.pointer);
				else
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
			case ENBT::Type::sarray:
				return pointer == interator.pointer;
			case ENBT::Type::array:
			case ENBT::Type::darray:
				return
					(*(std::vector<ENBT>::iterator*)pointer)
					!=
					(*(std::vector<ENBT>::iterator*)pointer);
			case ENBT::Type::compound:
				if (interate_type.is_signed)
					return
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer)
					!=
					(*(std::unordered_map<uint16_t, ENBT>::iterator*)interator.pointer);
				else
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
			case ENBT::Type::sarray:
				if(interate_type.is_signed)
				{
					switch (interate_type.length)
					{
					case ENBT::TypeLen::Tiny:
						return { "",((int8_t*)pointer) };
						break;
					case ENBT::TypeLen::Short:
						return { "",((int16_t*)pointer) };
						break;
					case ENBT::TypeLen::Default:
						return { "",((int32_t*)pointer) };
						break;
					case ENBT::TypeLen::Long:
						return { "",((int64_t*)pointer) };
						break;
					default:
						break;
					}
				}
				else {
					switch (interate_type.length)
					{
					case ENBT::TypeLen::Tiny:
						return { "",((uint8_t*)pointer) };
						break;
					case ENBT::TypeLen::Short:
						return { "",((uint16_t*)pointer) };
						break;
					case ENBT::TypeLen::Default:
						return { "",((uint32_t*)pointer) };
						break;
					case ENBT::TypeLen::Long:
						return { "",((uint64_t*)pointer) };
						break;
					default:
						break;
					}
				}
				break;
			case ENBT::Type::array:
			case ENBT::Type::darray:
				return { "", *(*(std::vector<ENBT>::iterator*)pointer) };
			case ENBT::Type::compound:
				if (interate_type.is_signed)
				{
					auto& tmp = (*(std::unordered_map<uint16_t, ENBT>::iterator*)pointer);
					return std::pair<const std::string, ENBT>(
						std::string(ENBT::FromAliasedStr(tmp->first)),
						tmp->second
					);
				}
				else
				{
					auto& tmp = (*(std::unordered_map<std::string, ENBT>::iterator*)pointer);
					return std::pair<const std::string, ENBT>(
						tmp->first,
						tmp->second
					);
				}
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
inline std::istream& operator>>(std::istream& is, ENBT::UUID& uuid) {
	is >> uuid.data[0] >> uuid.data[1] >> uuid.data[2] >> uuid.data[3] >> uuid.data[4] >> uuid.data[5] >> uuid.data[6] >> uuid.data[7];
	is >> uuid.data[8] >> uuid.data[9] >> uuid.data[10] >> uuid.data[11] >> uuid.data[12] >> uuid.data[13] >> uuid.data[14] >> uuid.data[15];
	return is;
}
inline std::ostream& operator<<(std::ostream& os, ENBT::UUID& uuid) {
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
		case ENBT::Type_ID::LenType::Long:
			is >> part; tmp.push_back(part);
			is >> part; tmp.push_back(part);
			is >> part; tmp.push_back(part);
			is >> part; tmp.push_back(part);
			[[fallthrough]];
		case ENBT::Type_ID::LenType::Default:
			is >> part; tmp.push_back(part);
			is >> part; tmp.push_back(part);
			[[fallthrough]];
		case ENBT::Type_ID::LenType::Short:
			is >> part; tmp.push_back(part);
			[[fallthrough]];
		case ENBT::Type_ID::LenType::Tiny:
			is >> part; tmp.push_back(part);
		}
		tid.PostDecode(tmp);
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

	static void WriteCompressLen(std::ostream& write_stream, uint64_t len) {
		union {
			uint64_t full = 0;
			uint8_t part[8];

		}b;
		b.full = ENBT::ConvertEndian(std::endian::big, len);

		constexpr struct {
			uint64_t b64 : 62 = -1;
			uint64_t b32 : 30 = -1;
			uint64_t b16 : 14 = -1;
			uint64_t b8 : 6 = -1;
		} m;
		if (len <= m.b8) {
			write_stream << b.part[0];
		}
		else if (len <= m.b16) {
			b.part[0] |= 1;
			write_stream << b.part[0];
			write_stream << b.part[1];
		}
		else if (len <= m.b32) {
			b.part[0] |= 2;
			write_stream << b.part[0];
			write_stream << b.part[1];
			write_stream << b.part[2];
			write_stream << b.part[3];
		}
		else if (len <= m.b64) {
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

	template<class T> static void WriteVar(std::ostream& write_stream, T value, std::endian endian = std::endian::native) {
		value = ENBT::ConvertEndian(endian, value);
		do {
			char currentByte = (char)(value & 0b01111111);

			value >>= 7;
			if (value != 0) currentByte |= 0b10000000;

			write_stream << currentByte;
		} while (value != 0);
	}
	template<class T> static void WriteVar(std::ostream& write_stream, ENBT::EnbtValue value, std::endian endian = std::endian::native) {
		WriteVar(write_stream, std::get<T>(value), endian);
	}
	static void WriteTypeID(std::ostream& write_stream, ENBT::Type_ID tid) {
		write_stream << tid;
	}

	template<class T> static void WriteValue(std::ostream& write_stream, T value, std::endian endian = std::endian::native) {
		if constexpr (std::is_same<T, ENBT::UUID>())
			ENBT::ConvertEndian(endian, value.data, 16);
		else 
			value = ENBT::ConvertEndian(endian, value);
		uint8_t* proxy = (uint8_t*)&value;
		for (size_t i = 0; i < sizeof(T); i++)
			write_stream << proxy[i];
	}
	template<class T> static void WriteValue(std::ostream& write_stream, ENBT::EnbtValue value, std::endian endian = std::endian::native) {
		return WriteValue(write_stream, std::get<T>(value), endian);
	}

	template<class T> static void WriteArray(std::ostream& write_stream, T* values, size_t len, std::endian endian = std::endian::native) {
		if constexpr (sizeof(T) == 1) {
			for (size_t i = 0; i < len; i++)
				write_stream << values[i];
		}
		else {
			for (size_t i = 0; i < len; i++)
				WriteValue(write_stream, values[i], endian);
		}
	}
	template<class T> static void WriteArray(std::ostream& write_stream, ENBT::EnbtValue* values, size_t len, std::endian endian = std::endian::native) {
		T* arr = new T[len];
		for (size_t i = 0; i < len; i++)
			arr[i] = std::get<T>(values[i]);
		WriteArray(write_stream, arr, len, endian);
		delete[] arr;
	}

	template<class T> static void WriteDefineLen(std::ostream& write_stream, T value) {
		return WriteValue(write_stream, value, std::endian::little);
	}
	static void WriteDefineLen(std::ostream& write_stream, uint64_t len, ENBT::Type_ID tid) {
		switch (tid.length)
		{
		case ENBT::Type_ID::LenType::Tiny:
			if (len != ((uint8_t)len))
				throw EnbtException("cannont convert value to uint8_t");
			WriteDefineLen(write_stream, (uint8_t)len);
			break;
		case ENBT::Type_ID::LenType::Short:
			if (len != ((uint16_t)len))
				throw EnbtException("cannont convert value to uint16_t");
			WriteDefineLen(write_stream, (uint16_t)len);
			break;
		case ENBT::Type_ID::LenType::Default:
			if (len != ((uint32_t)len))
				throw EnbtException("cannont convert value to uint32_t");
			WriteDefineLen(write_stream, (uint32_t)len);
			break;
		case ENBT::Type_ID::LenType::Long:
				return WriteDefineLen(write_stream, (uint64_t)len);
			break;
		}
	}

	static void InitalizeVersion(std::ostream& write_stream) {
		write_stream << (char)ENBT_VERSION_HEX;
	}

	static void SaveAStrings(std::ostream& write_stream) {
		auto&& temp = ENBT::GetAStrings();
		uint16_t len = (uint16_t)temp.size();
		if (len != temp.size())
			throw std::overflow_error("string length too long for put to uint16_t value");
		WriteDefineLen<uint16_t>(write_stream, len);
		for (uint16_t i = 0; i < len; i++) {
			auto cstr = temp[i].c_str();
			do
			{
				write_stream << *cstr;
			} while (*cstr++);
		}
	}

	static void WriteCompoud(std::ostream& write_stream, const ENBT& val) {
		if (val.type_id().is_signed) {
			auto result = std::get<std::unordered_map<uint16_t, ENBT>*>(val.content());
			WriteDefineLen(write_stream, result->size(), val.type_id());
			for (auto& it : *result) {
				WriteDefineLen<uint16_t>(write_stream, it.first);
				WriteToken(write_stream, it.second);
			}
		} else {
			auto result = std::get<std::unordered_map<std::string, ENBT>*>(val.content());
			WriteDefineLen(write_stream, result->size(), val.type_id());
			for (auto& it : *result) {
				WriteCompressLen(write_stream, it.first.size());
				WriteArray(write_stream, it.first.c_str(), it.first.size());
				WriteToken(write_stream, it.second);
			}
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
			if (tid.type != ENBT::Type_ID::Type::bit) {
				WriteTypeID(write_stream, tid);
				for (auto& it : *result)
					WriteValue(write_stream, it);
			}
			else {
				tid.is_signed = false;
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
	}
	static void WriteDArray(std::ostream& write_stream, const ENBT& val) {
		if (!val.is_array())
			throw EnbtException("This is not array for serialize it");
		auto result = (std::vector<ENBT>*)val.getPtr();
		WriteDefineLen(write_stream, result->size(),val.type_id());
		for (auto& it : *result)
			WriteToken(write_stream, it);
	}


	static void WriteSimpleArray(std::ostream& write_stream, const ENBT& val) {
		WriteCompressLen(write_stream, val.size());
		switch (val.type_id().length)
		{
		case ENBT::Type_ID::LenType::Tiny:
			if (val.type_id().is_signed)
				WriteArray(write_stream, val.getPtr(), val.size(), val.type_id().getEndian());
			else
				WriteArray(write_stream, val.getPtr(), val.size());
			break;
		case ENBT::Type_ID::LenType::Short:
			if (val.type_id().is_signed)
				WriteArray(write_stream, (uint16_t*)val.getPtr(), val.size(), val.type_id().getEndian());
			else
				WriteArray(write_stream, (uint16_t*)val.getPtr(), val.size());
			break;
		case ENBT::Type_ID::LenType::Default:
			if (val.type_id().is_signed)
				WriteArray(write_stream, (uint32_t*)val.getPtr(), val.size(), val.type_id().getEndian());
			else
				WriteArray(write_stream, (uint32_t*)val.getPtr(), val.size());
			break;
		case ENBT::Type_ID::LenType::Long:
			if (val.type_id().is_signed)
				WriteArray(write_stream, (uint64_t*)val.getPtr(), val.size(), val.type_id().getEndian());
			else
				WriteArray(write_stream, (uint64_t*)val.getPtr(), val.size());
			break;
		default:
			break;
		}

	}

	static void WriteValue(std::ostream& write_stream, const ENBT& val){
		ENBT::Type_ID tid = val.type_id();
		switch (tid.type)
		{
		case ENBT::Type_ID::Type::integer:
			switch (tid.length)
			{
			case  ENBT::Type_ID::LenType::Tiny:
				if (tid.is_signed)
					return WriteValue<int8_t>(write_stream, val.content(), tid.getEndian());
				else
					return WriteValue<uint8_t>(write_stream, val.content(), tid.getEndian());
			case  ENBT::Type_ID::LenType::Short:
				if (tid.is_signed)
					return WriteValue<int16_t>(write_stream, val.content(), tid.getEndian());
				else
					return WriteValue<uint16_t>(write_stream, val.content(), tid.getEndian());
			case  ENBT::Type_ID::LenType::Default:
				if (tid.is_signed)
					return WriteValue<int32_t>(write_stream, val.content(), tid.getEndian());
				else
					return WriteValue<uint32_t>(write_stream, val.content(), tid.getEndian());
			case  ENBT::Type_ID::LenType::Long:
				if (tid.is_signed)
					return WriteValue<int64_t>(write_stream, val.content(), tid.getEndian());
				else
					return WriteValue<uint64_t>(write_stream, val.content(), tid.getEndian());
			}
			return;
		case ENBT::Type_ID::Type::floating:
			switch (tid.length)
			{
			case  ENBT::Type_ID::LenType::Default:
				return WriteValue<float>(write_stream, val.content(), tid.getEndian());
			case  ENBT::Type_ID::LenType::Long:
				return WriteValue<double>(write_stream, val.content(), tid.getEndian());
			}
			return;
		case ENBT::Type_ID::Type::var_integer:
			switch (tid.length)
			{
			case  ENBT::Type_ID::LenType::Default:
				if (tid.is_signed)
					return WriteVar<int32_t>(write_stream, val.content(), tid.getEndian());
				else
					return WriteVar<uint32_t>(write_stream, val.content(), tid.getEndian());
			case  ENBT::Type_ID::LenType::Long:
				if (tid.is_signed)
					return WriteVar<int64_t>(write_stream, val.content(), tid.getEndian());
				else
					return WriteVar<uint64_t>(write_stream, val.content(), tid.getEndian());
			}
			return;
		case  ENBT::Type_ID::Type::uuid:	return WriteValue<ENBT::UUID>(write_stream, val.content(), tid.getEndian());
		case ENBT::Type_ID::Type::sarray:   return WriteSimpleArray(write_stream, val);
		case ENBT::Type_ID::Type::darray:	return WriteDArray(write_stream, val);
		case ENBT::Type_ID::Type::compound:	return WriteCompoud(write_stream, val);
		case ENBT::Type_ID::Type::array:	return WriteArray(write_stream, val);
		case ENBT::Type_ID::Type::optional:
			if (val.contains())
				WriteToken(write_stream, *val.getOptional());
			break;
		}
	}
	static void WriteToken(std::ostream& write_stream, const ENBT& val) {
		write_stream << val.type_id();
		WriteValue(write_stream, val);
	}






	template<class T> static T ReadVar(std::istream& read_stream, std::endian endian) {
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
	template<class T> static T ReadValue(std::istream& read_stream, std::endian endian = std::endian::native) {
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
	template<class T> static T* ReadArray(std::istream& read_stream,size_t len, std::endian endian = std::endian::native) {
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
		return ReadValue<T>(read_stream, std::endian::little);
	}
	static size_t ReadDefineLen(std::istream& read_stream, ENBT::Type_ID tid) {
		switch (tid.length)
		{
		case ENBT::Type_ID::LenType::Tiny:
			return ReadDefineLen<uint8_t>(read_stream);
		case ENBT::Type_ID::LenType::Short:
			return ReadDefineLen<uint16_t>(read_stream);
		case ENBT::Type_ID::LenType::Default:
			return ReadDefineLen<uint32_t>(read_stream);
		case ENBT::Type_ID::LenType::Long:
		{
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
		switch (tid.length)
		{
		case ENBT::Type_ID::LenType::Tiny:
			return ReadDefineLen<uint8_t>(read_stream);
		case ENBT::Type_ID::LenType::Short:
			return ReadDefineLen<uint16_t>(read_stream);
		case ENBT::Type_ID::LenType::Default:
			return ReadDefineLen<uint32_t>(read_stream);
		case ENBT::Type_ID::LenType::Long:
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
			} patrial;
		}b;
		read_stream >> b.complete;
		switch (b.patrial.len_flag) {
		case 0:
			return b.patrial.len;
		case 1:
		{
			uint16_t full = b.patrial.len;full <<= 8;
			uint8_t additional;
			read_stream >> additional;
			full |= additional;
			return ENBT::ConvertEndian(std::endian::little, full);
		}
		case 2:
		{
			uint32_t full = b.patrial.len;
			full <<= 24;
			uint8_t additional;
			read_stream >> additional;
			full |= additional; full <<= 16;
			read_stream >> additional;
			full |= additional; full <<= 8;
			read_stream >> additional;
			full |= additional;
			return ENBT::ConvertEndian(std::endian::little,full);
		}
		case 3:
		{
			uint64_t full = b.patrial.len;full <<= 56;
			uint8_t additional;
			read_stream >> additional;
			full |= additional; full <<= 48;
			read_stream >> additional;
			full |= additional; full <<= 40;
			read_stream >> additional;
			full |= additional; full <<= 24;
			read_stream >> additional;
			full |= additional; full <<= 24;
			read_stream >> additional;
			full |= additional; full <<= 16;
			read_stream >> additional;
			full |= additional; full <<= 8;
			read_stream >> additional;
			full |= additional;
			return ENBT::ConvertEndian(std::endian::little, full);
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
		if(tid.is_signed){
			std::unordered_map<uint16_t, ENBT> result;
			for (size_t i = 0; i < len; i++) {
				uint16_t key = ReadDefineLen<uint16_t>(read_stream);
				result[key] = ReadToken(read_stream);
			}
			return result;
		} else {
			std::unordered_map<std::string, ENBT> result;
			for (size_t i = 0; i < len; i++) {
				std::string key = ReadCompoundString(read_stream);
				result[key] = ReadToken(read_stream);
			}
			return result;
		}
	}
	static std::vector<ENBT> ReadArray(std::istream& read_stream, ENBT::Type_ID tid) {
		size_t len = ReadDefineLen(read_stream, tid);
		ENBT::Type_ID atid = ReadTypeID(read_stream);
		std::vector<ENBT> result(len);
		if (atid == ENBT::Type_ID::Type::bit) {
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
		return result;
	}
	static std::vector<ENBT> ReadDArray(std::istream& read_stream, ENBT::Type_ID tid) {
		size_t len = ReadDefineLen(read_stream, tid);
		std::vector<ENBT> result(len);
		for(size_t i = 0; i < len; i++)
			result[i] = ReadToken(read_stream);
		return result;
	}
	static ENBT ReadSArray(std::istream& read_stream, ENBT::Type_ID tid) {
		uint64_t len = ReadCompressLen(read_stream);
		auto endian = tid.getEndian();
		ENBT res;
		switch (tid.length)
		{
		case ENBT::Type_ID::LenType::Tiny: 
		{
			uint8_t* arr = ReadArray<uint8_t>(read_stream, len, endian);
			res = { arr,len };
			delete[] arr;
			break;
		}
		case ENBT::Type_ID::LenType::Short:
		{
			uint16_t* arr = ReadArray<uint16_t>(read_stream, len, endian);
			res = { arr,len };
			delete[] arr;
			break;
		}
		case ENBT::Type_ID::LenType::Default: 
		{
			uint32_t* arr = ReadArray<uint32_t>(read_stream, len, endian);
			res = { arr,len };
			delete[] arr;
			break;
		}
		case ENBT::Type_ID::LenType::Long: 
		{
			uint64_t* arr = ReadArray<uint64_t>(read_stream, len, endian);
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
		case ENBT::Type_ID::Type::integer:
			switch (tid.length)
			{
			case  ENBT::Type_ID::LenType::Tiny:
				if (tid.is_signed)
					return ReadValue<int8_t>(read_stream, tid.getEndian());
				else
					return ReadValue<uint8_t>(read_stream, tid.getEndian());
			case  ENBT::Type_ID::LenType::Short:
				if (tid.is_signed)
					return ReadValue<int16_t>(read_stream, tid.getEndian());
				else
					return ReadValue<uint16_t>(read_stream, tid.getEndian());
			case  ENBT::Type_ID::LenType::Default:
				if (tid.is_signed)
					return ReadValue<int32_t>(read_stream, tid.getEndian());
				else
					return ReadValue<uint32_t>(read_stream, tid.getEndian());
			case  ENBT::Type_ID::LenType::Long:
				if (tid.is_signed)
					return ReadValue<int64_t>(read_stream, tid.getEndian());
				else
					return ReadValue<uint64_t>(read_stream, tid.getEndian());
			default:
				return ENBT();
			}
		case ENBT::Type_ID::Type::floating:
			switch (tid.length)
			{
			case  ENBT::Type_ID::LenType::Default:
				return ReadValue<float>(read_stream, tid.getEndian());
			case  ENBT::Type_ID::LenType::Long:
				return ReadValue<double>(read_stream, tid.getEndian());
			default:
				return ENBT();
			}
		case ENBT::Type_ID::Type::var_integer:
			switch (tid.length)
			{
			case  ENBT::Type_ID::LenType::Default:
				if (tid.is_signed)
					return ReadVar<int32_t>(read_stream, tid.getEndian());
				else
					return ReadVar<uint32_t>(read_stream, tid.getEndian());
			case  ENBT::Type_ID::LenType::Long:
				if (tid.is_signed)
					return ReadVar<int64_t>(read_stream, tid.getEndian());
				else
					return ReadVar<uint64_t>(read_stream, tid.getEndian());
			default:
				return ENBT();
			}
		case  ENBT::Type_ID::Type::uuid:	return ReadValue<ENBT::UUID>(read_stream, tid.getEndian());
		case ENBT::Type_ID::Type::sarray:   return ReadSArray(read_stream, tid);
		case ENBT::Type_ID::Type::darray:	return ENBT(ReadDArray(read_stream,tid), tid);
		case ENBT::Type_ID::Type::compound:	return ReadCompoud(read_stream,tid);
		case ENBT::Type_ID::Type::array:	return ENBT(ReadArray(read_stream,tid), tid);
		case ENBT::Type_ID::Type::optional: return tid.is_signed ? ENBT(true, ReadToken(read_stream)) : ENBT(false, ENBT());
		case ENBT::Type_ID::Type::bit:		return ENBT((bool)tid.is_signed);
		default:							return ENBT();
		}
	}
	static ENBT ReadToken(std::istream& read_stream) {
		return ReadValue(read_stream,ReadTypeID(read_stream));
	}

	static void LoadAStrings(std::istream& read_stream) {
		uint16_t len = ReadDefineLen<uint16_t>(read_stream);
		std::vector<std::string> res;
		std::string ss;
		for (uint16_t i = 0; i < len; i++) {
			while (true) {
				char ch;
				read_stream >> ch;
				if (ch == 0)
					break;
				ss += ch;
			}
			res.push_back(ss);
			ss.clear();
		}
		ENBT::SetAStrings(res);
	}


	static void CheckVersion(std::istream& read_stream) {
		if(ReadValue<uint8_t>(read_stream)!= ENBT_VERSION_HEX) throw EnbtException("Unsuported version");
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
		case ENBT::Type_ID::Type::integer:
		case ENBT::Type_ID::Type::floating:
			switch (tid.length)
			{
			case ENBT::Type_ID::LenType::Tiny:		return 1;
			case ENBT::Type_ID::LenType::Short:		return 2;
			case ENBT::Type_ID::LenType::Default:	return 4;
			case ENBT::Type_ID::LenType::Long:		return 8;
			}
			break;
		case ENBT::Type_ID::Type::uuid:
			return 16;
		case ENBT::Type_ID::Type::bit:
			return 1;
		default:
			return 0;
		}
	}

	static void SkipArray(std::istream& read_stream, ENBT::Type_ID tid) {
		int index_multipler;
		uint64_t len = ReadDefineLen64(read_stream, tid);
		if (!(index_multipler = CanFastIndex(ReadTypeID(read_stream)))) 
			for (uint64_t i = 0; i < len; i++)
				SkipToken(read_stream);
		else {
			if (tid == ENBT::Type_ID::Type::bit) {
				uint64_t actual_len = len / 8;
				if (len % 8)
					++actual_len;
				read_stream.seekg(read_stream.tellg() += actual_len);
			}
			else 
				read_stream.seekg(read_stream.tellg() += len * index_multipler);
		}
	}
	static void SkipDArray(std::istream& read_stream, ENBT::Type_ID tid) {
		uint64_t len = ReadDefineLen64(read_stream, tid);
		for (uint64_t i = 0; i < len; i++)
			SkipToken(read_stream);
	}
	static void SkipSArray(std::istream& read_stream, ENBT::Type_ID tid) {
		uint64_t index = ReadCompressLen(read_stream);
		switch (tid.length)
		{
		case ENBT::Type_ID::LenType::Tiny:
			read_stream.seekg(read_stream.tellg() += index);
			break;
		case ENBT::Type_ID::LenType::Short:
			read_stream.seekg(read_stream.tellg() += index * 2);
			break;
		case ENBT::Type_ID::LenType::Default:
			read_stream.seekg(read_stream.tellg() += index * 4);
			break;
		case ENBT::Type_ID::LenType::Long:
			read_stream.seekg(read_stream.tellg() += index * 8);
			break;
		default:
			break;
		}
	}
	static void SkipValue(std::istream& read_stream,ENBT::Type_ID tid) {
		switch (tid.type)
		{
		case ENBT::Type_ID::Type::floating:
		case ENBT::Type_ID::Type::integer:
			switch (tid.length)
			{
			case  ENBT::Type_ID::LenType::Tiny:
				read_stream.seekg(read_stream.tellg() += 1); break;
			case  ENBT::Type_ID::LenType::Short:
				read_stream.seekg(read_stream.tellg() += 2); break;
			case  ENBT::Type_ID::LenType::Default:
				read_stream.seekg(read_stream.tellg() += 4); break;
			case  ENBT::Type_ID::LenType::Long:
				read_stream.seekg(read_stream.tellg() += 8); break;
			}
			break;
		case ENBT::Type_ID::Type::var_integer:
			switch (tid.length)
			{
			case  ENBT::Type_ID::LenType::Default:
				if (tid.is_signed)
					ReadVar<int32_t>(read_stream, std::endian::native);
				else
					ReadVar<uint32_t>(read_stream, std::endian::native);
				break;
			case  ENBT::Type_ID::LenType::Long:
				if (tid.is_signed)
					ReadVar<int64_t>(read_stream, std::endian::native);
				else
					ReadVar<uint64_t>(read_stream, std::endian::native);
			}
			break;
		case  ENBT::Type_ID::Type::uuid:	read_stream.seekg(read_stream.tellg() += 16); break;
		case ENBT::Type_ID::Type::sarray:   SkipSArray(read_stream, tid); break;
		case ENBT::Type_ID::Type::darray:	SkipDArray(read_stream, tid);  break;
		case ENBT::Type_ID::Type::compound:	SkipCompoud(read_stream, tid); break;
		case ENBT::Type_ID::Type::array:	SkipArray(read_stream, tid);   break;
		case ENBT::Type_ID::Type::optional:	
			if (tid.is_signed) 
				SkipToken(read_stream);
			break;
		}
	}
	static void SkipToken(std::istream& read_stream) {
		return SkipValue(read_stream, ReadTypeID(read_stream));
	}


	//move read stream cursor to value in compound, return true if value found
	static bool FindValueCompound(std::istream& read_stream, ENBT::Type_ID tid, uint16_t key) {
		size_t len = ReadDefineLen(read_stream, tid);
		if (tid.is_signed) {
			for (size_t i = 0; i < len; i++) {
				if (ReadDefineLen<uint16_t>(read_stream) != key)SkipValue(read_stream, ReadTypeID(read_stream));
				else return true;
			}
			return false;
		}
		else {
			std::string adaptived = ENBT::FromAliasedStr(key);
			for (size_t i = 0; i < len; i++) {
				if (ReadCompoundString(read_stream) != adaptived)SkipValue(read_stream, ReadTypeID(read_stream));
				else return true;
			}
			return false;
		}
	}
	//move read stream cursor to value in compound, return true if value found
	static bool FindValueCompound(std::istream& read_stream, ENBT::Type_ID tid, std::string key) {
		size_t len = ReadDefineLen(read_stream, tid);
		if (tid.is_signed)
		{
			uint16_t adaptived = ENBT::ToAliasedStr(key.c_str());
			for (size_t i = 0; i < len; i++) {
				if (ReadDefineLen<uint16_t>(read_stream) != adaptived)SkipValue(read_stream, ReadTypeID(read_stream));
				else return true;
			}
			return false;
		}
		else {
			for (size_t i = 0; i < len; i++) {
				if (ReadCompoundString(read_stream) != key)SkipValue(read_stream, ReadTypeID(read_stream));
				else return true;
			}
			return false;
		}
	}


	static void IndexStaticArray(std::istream& read_stream, uint64_t index,uint64_t len,ENBT::Type_ID targetId) {
		if (index >= len)
			throw EnbtException('[' + std::to_string(index) + "] out of range " + std::to_string(len));
		if (uint8_t skiper = CanFastIndex(targetId)) {
			if(targetId != ENBT::Type_ID::Type::bit)
				read_stream.seekg(read_stream.tellg() += index * skiper);
			else
				read_stream.seekg(read_stream.tellg() += index / 8);
		}
		else 
			for (uint64_t i = 0; i < index; i++)
				SkipValue(read_stream, targetId);
		
	}
	static void IndexDynArray(std::istream& read_stream,uint64_t index, uint64_t len) {
		if (index >= len)
			throw EnbtException('[' + std::to_string(index) + "] out of range " + std::to_string(len));
		for (uint64_t i = 0; i < index; i++)
			SkipToken(read_stream);
	}
	static void IndexArray(std::istream& read_stream, uint64_t index, ENBT::Type_ID arr_tid) {
		switch (arr_tid.type)
		{
		case ENBT::Type_ID::Type::array:
		{
			uint64_t len = ReadDefineLen64(read_stream, arr_tid);
			auto targetId = ReadTypeID(read_stream);
			IndexStaticArray(read_stream, index, len, targetId);
			break;
		}
		case ENBT::Type_ID::Type::darray:
			IndexDynArray(read_stream, index, ReadDefineLen64(read_stream, arr_tid));
			break;
		default:
			throw EnbtException("Invalid type id");
		}
	}
	static void IndexArray(std::istream& read_stream, uint64_t index) {
		IndexArray(read_stream, index, ReadTypeID(read_stream));
	}

	//move read_stream cursor to value,
	//value_path similar: "0/the test/4/54",
	//return succes status
	//can throw EnbtException
	static bool MoveToValuePath(std::istream& read_stream, const std::string& value_path) {
		try {
			for (auto&& tmp : SplitS(value_path, "/")) {
				auto tid = ReadTypeID(read_stream);
				switch (tid.type)
				{
				case ENBT::Type_ID::Type::array:
				case ENBT::Type_ID::Type::darray:
					IndexArray(read_stream, std::stoull(tmp), tid);
					continue;
				case ENBT::Type_ID::Type::compound:
					if (!FindValueCompound(read_stream,tid, tmp)) return false;
					continue;
				default:
					return false;
				}
			}
			return true;
		}
		catch (const std::out_of_range&) {
			throw;
		}
		catch (const std::exception&) {
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
				switch (tid.type)
				{
				case ENBT::Type_ID::Type::array:
				{
					uint64_t len = ReadDefineLen64(read_stream, tid);
					auto targetId = ReadTypeID(read_stream);
					uint64_t index = std::stoull(tmp);
					IndexStaticArray(read_stream, index, len, targetId);
					if (targetId.type == ENBT::Type_ID::Type::bit) {
						is_bit_value = true;
						uint8_t clear_value;
						read_stream >> clear_value;
						bit_value = clear_value << index % 8;
					}
					continue;
				}
				case ENBT::Type_ID::Type::darray:
					IndexArray(read_stream, std::stoull(tmp), tid);
					continue;
				case ENBT::Type_ID::Type::compound:
					if (!FindValueCompound(read_stream, tid, tmp)) return false;
					continue;
				default:
					throw std::invalid_argument("Invalid Path to Value");
				}
			}
		}
		catch (...) {
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

#endif


//ASN Example file
// 
// [0x10] version
// [5] count asociated utf8 strings
//	[3]string bytes(without zero char)		 //
//		["123"] string	 // 0
//	[4]["test"]			 // 1
//	[5]["hello"]		 // 2
//	[5]["world"]		 // 3
//	[12]["Привет"]		 // 4 
// 
// ENBT Example file
///////                       type (1 byte)                         compound len
//	[ENBT::Type_ID{ type=compound,length = tiny,is_signed=true}][5 (1byte unsigned)]
//		[4][{ type=sarray,length = tiny,is_signed=false}][7]["Мир!"]
//		[0][{ type=integer,length = long,is_signed=true}][-9223372036854775808]
//		[3][{ type=uuid}][0xD55F0C3556DA165E6F512203C78B57FF]
//		[1][{ type=darray,length = tiny,is_signed=false}] [3]
//			[{ type=sarray,length = tiny,is_signed=false}][307]["\"Pijamalı hasta yağız şoföre çabucak güvendi\"\"Victor jagt zwölf Boxkämpfer quer über den großen Sylter Deich\"\"съешь ещё этих мягких французских булок, да выпей чаю.\"\"Pranzo d'acqua fa volti sghembi\"\"Stróż pchnął kość w quiz gędźb vel fax myjń\"\"]
// 			[{ type=integer,length = long,is_signed=true}][9223372036854775807]
//			[{ type=uuid}][0xD55F0C3556DA165E6F512203C78B57FF]
//		