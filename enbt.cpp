#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#include "enbt.hpp"
#include <sstream>
#include <boost/uuid/uuid_io.hpp>
#include <locale>
#include <codecvt>
ENBT& ENBT::operator[](size_t index){
	switch (data_type_id.type) {
	case Type::array:
		return ((std::vector<ENBT>*)data)->operator[](index);
	case Type::structure:
		return ((ENBT*)data)[index];
	case Type::compound:
		return ((std::unordered_map<std::string, ENBT>*)data)->operator[](std::to_string(index));
	default:
		throw std::invalid_argument("Invalid tid, cannont index array");
	}
}
ENBT& ENBT::operator[](const std::string& index) {
	if (data_type_id.type == Type::compound) 
		return ((std::unordered_map<std::string, ENBT>*)data)->operator[](index);
	else
		throw std::invalid_argument("Invalid tid, cannont index compound");
}
ENBT& ENBT::operator[](const char* index) {
	return operator[](std::string(index));
}
size_t ENBT::remove(std::string name) {
	if(data_type_id.type == Type::compound)
		return ((std::unordered_map<std::string, ENBT>*)data)->erase(name);
	else
		throw std::invalid_argument("Invalid tid, cannont index compound");
}

const ENBT& ENBT::operator[](size_t index) const {
	switch (data_type_id.type) {
	case Type::array:
		return ((std::vector<ENBT>*)data)->operator[](index);
	case Type::structure:
		return ((ENBT*)data)[index];
	case Type::compound:
		return ((std::unordered_map<std::string, ENBT>*)data)->operator[](std::to_string(index));
	default:
		throw std::invalid_argument("Invalid tid, cannont index array");
	}
}
const ENBT& ENBT::operator[](const std::string& index) const {
	if (data_type_id.type == Type::compound)
		return ((std::unordered_map<std::string, ENBT>*)data)->operator[](index);
	else
		throw std::invalid_argument("Invalid tid, cannont index compound");
}
const ENBT& ENBT::operator[](const char* index) const {
	return operator[](std::string(index));
}

ENBT::operator std::unordered_map<std::string, ENBT>() const {
	if (data_type_id.type == Type::compound)
		return *(std::unordered_map<std::string, ENBT>*)data;
	else
		throw std::invalid_argument("Invalid tid, cannont convert compound");
}


std::string normalize_string_SENBT(const std::string& str) {
	std::string res = "\"";
	for (char it : str) {
		switch (it)
		{
		case '"':
			res += "\\\"";
			break;
		case '\\':
			res += "\\";
			break;
		case '\n':
			res += "\\n";
			break;
		case '\t':
			res += "\\t";
			break;
		case '\b':
			res += "\\b";
			break;
		default:
			res += it;
		}
	}
	return res + '"';
}

std::string stringize_denormalized_string_SENBT(const ENBT& enbt) {
	std::string res = "`";
	auto ibegin = enbt.cbegin();
	auto iend = enbt.cend();
	while (ibegin != iend) {
		res += (*ibegin).second.to_senbt() + ",";
		++ibegin;
	}
	if (res.size() == 1)
		return "``";
	res[res.size() - 1] = '`';
	return res;
}
std::string ENBT::to_senbt() const {
	switch (data_type_id.type)
	{
	case ENBT::Type::none: return {};
		case ENBT::Type::integer:
			switch (data_type_id.length)
			{
			case ENBT::TypeLen::Tiny:
				if (data_type_id.is_signed)
					return "i8" + std::to_string((int8_t)*this);
				else
					return "ui8" + std::to_string((uint8_t)*this);
			case ENBT::TypeLen::Short:
				if (data_type_id.is_signed)
					return "i16" + std::to_string((int16_t)*this);
				else
					return "ui16" + std::to_string((uint16_t)*this);
			case ENBT::TypeLen::Default:
				if (data_type_id.is_signed)
					return "i32" + std::to_string((int32_t)*this);
				else
					return "ui32" + std::to_string((uint32_t)*this);
			case ENBT::TypeLen::Long:
				if (data_type_id.is_signed)
					return "i64" + std::to_string((int64_t)*this);
				else
					return "ui64" + std::to_string((uint64_t)*this);
			default:
				assert(false && "Dear programmer fix this bug");
				return {};
			}
		case ENBT::Type::floating:
			switch (data_type_id.length)
			{
			case ENBT::TypeLen::Default:
					return "f" + std::to_string((float)*this);
			case ENBT::TypeLen::Long:
					return "d" + std::to_string((double)*this);
			default:
				throw EnbtException("Unsuported floating length");
			}
		case ENBT::Type::ex_integer:
			switch (data_type_id.length)
			{
			case ENBT::TypeLen::Tiny:
				if (data_type_id.is_signed)
					return "vt" + std::to_string((int64_t)*this);
				else
					return "vut" + std::to_string((uint64_t)*this);
			case ENBT::TypeLen::Short:
				throw EnbtException("Not implemented short ex_integer");
			case ENBT::TypeLen::Default:
				if (data_type_id.is_signed)
					return "vi" + std::to_string((int32_t)*this);
				else
					return "vui" + std::to_string((uint32_t)*this);
			case ENBT::TypeLen::Long:
				if (data_type_id.is_signed)
					return "vl" + std::to_string((int64_t)*this);
				else
					return "vul" + std::to_string((uint64_t)*this);
			default:
				assert(false && "Dear programmer fix this bug");
				return {};
			}
		case ENBT::Type::uuid:	
			return "uuid" + boost::uuids::to_string((ENBT::UUID)*this);
		case ENBT::Type::string:
			if(data_type_id.is_signed)
				return stringize_denormalized_string_SENBT(*this);
			else {
				try {
					return normalize_string_SENBT(*this);
				}
				catch (const std::range_error&) {
					return stringize_denormalized_string_SENBT(*this);
				}
			}
		case ENBT::Type::compound: {
			std::string res = "{";
			for (auto it : *this) {
				res += normalize_string_SENBT(it.first) + ":";
				res += it.second.to_senbt() + ";";
			}
			return res + '}';
		}
		case ENBT::Type::array: {
			std::string res = "[";
			for (auto it : *this)
				res += it.second.to_senbt() + ",";
			res[res.size() - 1] = ']';
			return res;
		}
		case ENBT::Type::structure: {
			std::string res = "(";
			for (auto it : *this)
				res += it.second.to_senbt() + ",";
			res[res.size() - 1] = ')';
			return res ;
		}
		case ENBT::Type::optional:
			return contains() ? "!{" + get_optional()->to_senbt() + '}' : "?";
		case ENBT::Type::bit:
			return data_type_id.is_signed ? "true" : "false";
		case ENBT::Type::domain:
			return "domain{}";
	default:
		assert(false && "Dear programmer fix this bug");
		return "";
	}
}

ENBT ENBT::from_senbt(const std::string&) {
	return{};
}


template<class Target>
Target simple_int_convert(const ENBT::EnbtValue& val) {

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

	else
		throw EnbtException("Invalid type for convert");
}

ENBT::operator bool() const { return simple_int_convert<bool>(content()); }
ENBT::operator int8_t() const { return simple_int_convert<int8_t>(content()); }
ENBT::operator int16_t() const { return simple_int_convert<int16_t>(content()); }
ENBT::operator int32_t() const { return simple_int_convert<int32_t>(content()); }
ENBT::operator int64_t() const { return simple_int_convert<int64_t>(content()); }
ENBT::operator uint8_t() const { return simple_int_convert<uint8_t>(content()); }
ENBT::operator uint16_t() const { return simple_int_convert<uint16_t>(content()); }
ENBT::operator uint32_t() const { return simple_int_convert<uint32_t>(content()); }
ENBT::operator uint64_t() const { return simple_int_convert<uint64_t>(content()); }
ENBT::operator float() const { return simple_int_convert<float>(content()); }
ENBT::operator double() const { return simple_int_convert<double>(content()); }




void ENBT::shrink_to_fit() {
	switch (data_type_id.type)
	{
	case ENBT::Type::compound: {
		auto& compund = *((std::unordered_map<std::string, ENBT>*)data);
		for (auto it = compund.begin(); it != compund.end(); )
		{
			if (it->second.data_type_id.type == Type::none) { it = compund.erase(it); }
			else { ++it; }
		}
		break;
	}
	case ENBT::Type::array:
		((std::vector<ENBT>*)data)->shrink_to_fit();
		break;
	}
}
ENBT::operator std::string() const {
	if (data_type_id.type == Type::string) {
		switch (data_type_id.length)
		{
		case TypeLen::Tiny:
			return (char*)std::get<uint8_t*>(content());
		case TypeLen::Short: {
			std::wstring_convert<std::codecvt_utf8<uint16_t>, uint16_t> converterX;
			return converterX.to_bytes(std::get<uint16_t*>(content()));
		}
		case TypeLen::Default: {
			std::wstring_convert<std::codecvt_utf8<uint32_t>, uint32_t> converterX;
			return converterX.to_bytes(std::get<uint32_t*>(content()));
		}
		case TypeLen::Long: {
			std::wstring_convert<std::codecvt_utf8<uint64_t>, uint64_t> converterX;
			return converterX.to_bytes(std::get<uint64_t*>(content()));
		}
		default:
			assert(false && "Dear programmer fix this bug");
			return "";
		}
	}
}