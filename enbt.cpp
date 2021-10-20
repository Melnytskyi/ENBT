#include "enbt.hpp"
#include <sstream>
const char** ENBT::global_strings;
uint16_t ENBT::total_strings = 0;
ENBT& ENBT::operator[](size_t index){
	if (is_array())
		return ((std::vector<ENBT>*)data)->operator[](index);
	if (data_type_id.type == Type_ID::Type::structure)
		return ((ENBT*)data)[index];
	throw std::invalid_argument("Invalid tid, cannont index array");
}
ENBT& ENBT::operator[](const char* index) {
	if (is_compoud()) {
		if (is_tiny_compoud())
			return ((std::unordered_map<uint16_t, ENBT>*)data)->operator[](ToAliasedStr(index));
		else
			return ((std::unordered_map<std::string, ENBT>*)data)->operator[](index);
	}
	throw std::invalid_argument("Invalid tid, cannont index compound");
}
void ENBT::remove(std::string name) {
	if (is_tiny_compoud())
		((std::unordered_map<uint16_t, ENBT>*)data)->erase(ToAliasedStr(name.c_str()));
	else
		((std::unordered_map<std::string, ENBT>*)data)->erase(name);
}

const ENBT& ENBT::operator[](size_t index) const {
	if (is_array())
		return ((std::vector<ENBT>*)data)->operator[](index);

	if (data_type_id.type == Type_ID::Type::structure)
		return ((ENBT*)data)[index];
	throw std::invalid_argument("Invalid tid, cannont index array");
}
const ENBT& ENBT::operator[](const char* index) const {
	if (is_compoud()) {
		if(is_tiny_compoud())
			return ((std::unordered_map<uint16_t, ENBT>*)data)->operator[](ToAliasedStr(index));
		else
			return ((std::unordered_map<std::string, ENBT>*)data)->operator[](index);
	}
	throw std::invalid_argument("Invalid tid, cannont index compound");
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
	else throw std::invalid_argument("Invalid tid, cannont index array");
}


ENBT::operator std::unordered_map<std::string, ENBT>() const {
	if (is_compoud()) {
		if (is_tiny_compoud())
		{
			auto& tmp = *(std::unordered_map<uint16_t, ENBT>*)data;
			std::unordered_map<std::string, ENBT> res;
			for (auto& temp : tmp) 
				res[FromAliasedStr(temp.first)] = temp.second;
			return res;
		}
		else
			return *(std::unordered_map<std::string, ENBT>*)data;
	}
	throw std::invalid_argument("Invalid tid, cannont convert compound");
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

	else if (std::holds_alternative<ENBT*>(val))
		return (Target)std::get<ENBT*>(val);

	else
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
