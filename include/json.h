#ifndef JSON_H
#define JSON_H

//# -*- encoding: utf-8 -*-
//'''
//@文件    :json.h
//@说明    :
//@时间    :2021/01/16 22:59:39
//@作者    :周恒
//@版本    :1.0
//'''
//
//
#pragma once
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace co_uring_web::utils {
class JsonTypeError : public std::runtime_error {
   public:
	JsonTypeError() : std::runtime_error("json类型错误") {};
	JsonTypeError(const std::string &s) : std::runtime_error(s) {};
	// virtual const char* what() const noexcept { return msg.c_str(); }
	~JsonTypeError() override = default;
};
class JsonObj {
   public:
	enum class JsonBaseType {
		JsonNull = 0,
		JsonBoolType = 1,
		JsonStringType = 2,
		JsonIntegerType,
		JsonDoubleType,
		JsonArrayType,
		JsonMapType
	};
	JsonBaseType jsonBaseType {JsonBaseType::JsonNull};
	JsonObj() = default;
	JsonObj(JsonBaseType type) : jsonBaseType(type) {};

	virtual ~JsonObj() noexcept = default;
	JsonObj &operator[](int index);
	JsonObj &operator[](const std::string &str);
	JsonObj &operator[](std::string &&str);
	void *getData();
};

class JsonString : public JsonObj {
   public:
	std::string data;
	JsonString() : JsonObj(JsonBaseType::JsonStringType) {};
	JsonString(const JsonString &s) = default;
	JsonString(JsonString &&) noexcept = default;
	JsonString &operator=(const JsonString &) = default;
	JsonString &operator=(JsonString &&other) noexcept{
		if(this==&other) [[unlikely]] return *this;
		data.swap(other.data);
		return *this;
	};
	inline JsonString &operator=(const std::string &s) {
		data = s;
		return *this;
	}
	inline JsonString &operator=(std::string &&s) {
		if (&(this->data) == &s) [[unlikely]]
			return *this;
		data = std::move(s);
		return *this;
	}
	JsonString(const std::string& s) : JsonObj(JsonBaseType::JsonStringType), data(s) {};
	JsonString(std::string &&s) : JsonObj(JsonBaseType::JsonStringType), data(std::move(s)) {};

	operator std::string() noexcept { return data; }
	operator std::string_view() noexcept { return std::string_view(data); }

	~JsonString() override = default;
};
class JsonInteger : public JsonObj {
   public:
	int64_t data {0};
	JsonInteger() : JsonObj(JsonBaseType::JsonIntegerType) {};
	JsonInteger(int64_t num) : JsonObj(JsonBaseType::JsonIntegerType), data(num) {};
	JsonInteger(const JsonInteger &) = default;
	JsonInteger(JsonInteger &&) noexcept = default;
	JsonInteger &operator=(const JsonInteger &) = default;
	JsonInteger &operator=(JsonInteger &&) noexcept = default;
	operator int64_t() { return data; }

	~JsonInteger() override = default;
};
class JsonDouble : public JsonObj {
   public:
	double data {0};
	JsonDouble() : JsonObj(JsonBaseType::JsonDoubleType) {}
	JsonDouble(double num) : JsonObj(JsonBaseType::JsonDoubleType), data(num) {};
	JsonDouble(const JsonDouble &) = default;
	JsonDouble(JsonDouble &&) noexcept = default;
	JsonDouble &operator=(const JsonDouble &) = default;
	JsonDouble &operator=(JsonDouble &&) noexcept = default;
	operator double() { return data; }
	~JsonDouble() override = default;
};
class JsonBool : public JsonObj {
   public:
	bool data {false};

	JsonBool() : JsonObj(JsonBaseType::JsonBoolType) {}
	JsonBool(bool b) : JsonObj(JsonBaseType::JsonBoolType), data(b) {};
	JsonBool(const JsonBool &) = default;
	JsonBool(JsonBool &&) = default;
	JsonBool &operator=(const JsonBool &) = default;
	JsonBool &operator=(JsonBool &&) = default;

	~JsonBool() override = default;
};
class JsonArray : public JsonObj {
   public:
	std::vector<std::unique_ptr<JsonObj>> data;
	JsonArray() : JsonObj(JsonBaseType::JsonArrayType) {}
	JsonArray(const JsonArray &o);
	JsonArray(const std::vector<std::unique_ptr<JsonObj>> &vec);
	JsonArray(JsonArray &&o) noexcept = default;
	JsonArray(std::vector<std::unique_ptr<JsonObj>> &&vec) noexcept
	    : JsonObj(JsonBaseType::JsonArrayType),
	      data(std::move(vec)) {

	      };
	JsonArray &operator=(const JsonArray &o);
	JsonArray &operator=(JsonArray &&o) noexcept;

	~JsonArray() override = default;
	// operator std::vector<std::unique_ptr<JsonObj>>&();
	// JsonArray& operator=(const JsonArray&) = default;
};
class JsonMap : public JsonObj {
   public:
	using JsonMapImpl = std::map<std::string, std::unique_ptr<JsonObj>>;
	JsonMapImpl data;
	JsonMap() : JsonObj(JsonBaseType::JsonMapType) {};
	JsonMap(const JsonMapImpl &m);
	JsonMap(JsonMapImpl &&m) noexcept : JsonObj(JsonBaseType::JsonMapType), data(std::move(m)) {
		jsonBaseType = JsonBaseType::JsonMapType;
	};
	JsonMap(const JsonMap &o);
	JsonMap(JsonMap &&) noexcept = default;
	JsonMap &operator=(const JsonMap &o);
	JsonMap &operator=(JsonMap &&o) noexcept;
	~JsonMap() override = default;
	//#TODO
};

class JsonParseError : public std::runtime_error {
   public:
	JsonParseError() : std::runtime_error("json parser error") {};
	JsonParseError(const std::string &s) : std::runtime_error(s) {};

	~JsonParseError() override = default;
};
class JsonParser {
	std::stringstream ss_;

   public:
	std::unique_ptr<JsonObj> parse(const std::string_view view);
	std::string dump(const JsonObj *obj, int indent = 4);

   private:
	inline std::unique_ptr<JsonObj> parseNum(const std::string_view view, ssize_t &startIndex);
	inline std::unique_ptr<JsonString> parseString(const std::string_view view,
	                                               ssize_t &startIndex);
	inline std::unique_ptr<JsonMap> parseMap(const std::string_view view, ssize_t &startIndex);
	std::unique_ptr<JsonArray> parseArray(const std::string_view view, ssize_t &startIndex);
	std::unique_ptr<JsonBool> parseBool(const std::string_view view, ssize_t &startIndex);

	inline void dumpNum(const JsonObj *jn, std::string &res);
	inline void dumpString(const JsonString *js, std::string &res);
	inline void dumpString(const std::string_view view, std::string &res);
	void dumpMap(const JsonMap *jm, std::string &res, int depth, int indent, bool singleLine);
	void dumpArray(const JsonArray *ja, std::string &res, int depth, int indent, bool singleLine);
	inline void dumpBool(const JsonBool *jb, std::string &res);
	inline void dumpNull(std::string &res) { res.append("null"); }
};

}  // namespace co_uring_web::utils
   // std::map<std::string, std::unique_ptr<JsonObj>> data;
#endif
