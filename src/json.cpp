#include "json.h"

#include <sys/types.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#include "utils.h"
namespace co_uring_web::utils {
JsonObj &JsonObj::operator[](int index) {
	switch (this->jsonBaseType) {
		case JsonBaseType::JsonArrayType: {
			return *(static_cast<JsonArray *>(this)->data[index].get());
		}
		default:
			throw JsonTypeError("不是JsonArray类型");
	}
};
JsonObj &JsonObj::operator[](const std::string &str) {
	switch (this->jsonBaseType) {
		case JsonBaseType::JsonMapType: {
			return *(((JsonMap *)this)->data[str].get());
		}
		default: {
			throw JsonTypeError("不是JsonMap类型");
		}
	}
};  // namespace utils
JsonObj &JsonObj::operator[](std::string &&str) {
	switch (this->jsonBaseType) {
		case JsonBaseType::JsonBoolType:
		case JsonBaseType::JsonDoubleType:
		case JsonBaseType::JsonIntegerType:
		case JsonBaseType::JsonNull:
		case JsonBaseType::JsonArrayType:
		case JsonBaseType::JsonStringType: {
			throw JsonTypeError("不是JsonMap类型");
		}
		case JsonBaseType::JsonMapType: {
			return *(((JsonMap *)this)->data[std::move(str)].get());
		}
	}
};
void *JsonObj::getData() {
	switch (this->jsonBaseType) {
		case JsonBaseType::JsonNull: {
			return nullptr;
		}
		case JsonBaseType::JsonDoubleType: {
			return &(((JsonDouble *)this)->data);
		}
		case JsonBaseType::JsonIntegerType: {
			return &(((JsonInteger *)this)->data);
		}
		case JsonBaseType::JsonBoolType: {
			return &(((JsonBool *)this)->data);
		}
		case JsonBaseType::JsonMapType: {
			return &(((JsonMap *)this)->data);
		}
		case JsonBaseType::JsonArrayType: {
			return &(((JsonArray *)this)->data);
		}
		case JsonBaseType::JsonStringType: {
			return &(((JsonString *)this)->data);
		}
		default: {
			throw std::invalid_argument("错误的json对象类型" +
			                            std::to_string((int)this->jsonBaseType));
		}
	}
}
JsonArray::JsonArray(const std::vector<std::unique_ptr<JsonObj>> &vec)
    : JsonObj(JsonBaseType::JsonArrayType) {
	using namespace std;

	data.clear();
	data.reserve(vec.size());
	for (auto &&p : vec) {
		switch (p->jsonBaseType) {
			case JsonBaseType::JsonIntegerType: {
				data.push_back(make_unique<JsonInteger>(((JsonInteger *)p.get())->data));
				break;
			}
			case JsonBaseType::JsonDoubleType: {
				data.push_back(make_unique<JsonDouble>(((JsonDouble *)p.get())->data));
				break;
			}
			case JsonBaseType::JsonStringType: {
				data.push_back(make_unique<JsonString>(((JsonString *)p.get())->data));
				break;
			}
			case JsonBaseType::JsonArrayType: {
				data.push_back(make_unique<JsonArray>(((JsonArray *)p.get())->data));
				break;
			}
			case JsonBaseType::JsonMapType: {
				data.push_back(make_unique<JsonMap>(((JsonMap *)p.get())->data));
				break;
			}
			case JsonBaseType::JsonNull: {
				data.push_back(make_unique<JsonObj>());
				break;
			}
			case JsonBaseType::JsonBoolType: {
				data.push_back(make_unique<JsonBool>(((JsonBool *)p.get())->data));
				break;
			}
		}
	}
}
JsonArray::JsonArray(const JsonArray &o) : JsonArray(o.data) {}
JsonArray &JsonArray::operator=(const JsonArray &o) {
	if (&o == this) return *this;
	using namespace std;
	const auto &vec = o.data;
	jsonBaseType = JsonBaseType::JsonArrayType;
	data.clear();
	data.reserve(vec.size());
	for (auto &&p : vec) {
		switch (p->jsonBaseType) {
			case JsonBaseType::JsonIntegerType: {
				data.push_back(move(make_unique<JsonInteger>(((JsonInteger *)p.get())->data)));
				break;
			}
			case JsonBaseType::JsonDoubleType: {
				data.push_back(make_unique<JsonDouble>(((JsonDouble *)p.get())->data));
				break;
			}
			case JsonBaseType::JsonStringType: {
				data.push_back(make_unique<JsonString>(((JsonString *)p.get())->data));
				break;
			}
			case JsonBaseType::JsonArrayType: {
				data.push_back(make_unique<JsonArray>(((JsonArray *)p.get())->data));
				break;
			}
			case JsonBaseType::JsonMapType: {
				data.push_back(make_unique<JsonMap>(((JsonMap *)p.get())->data));
				break;
			}
			default:
				break;
		}
	}
	return *this;
}
JsonArray &JsonArray::operator=(JsonArray &&o) noexcept {
	if (&o == this) return *this;
	this->data = std::move(o.data);
	return *this;
}
JsonMap::JsonMap(const JsonMapImpl &m) : JsonObj(JsonBaseType::JsonMapType) {
	using namespace std;

	data.clear();
	for (auto &&[k, v] : m) {
		switch (v->jsonBaseType) {
			case (JsonBaseType::JsonIntegerType): {
				data.insert_or_assign(k, make_unique<JsonInteger>(((JsonInteger *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonDoubleType): {
				data.insert_or_assign(k, make_unique<JsonDouble>(((JsonDouble *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonStringType): {
				data.insert_or_assign(k, make_unique<JsonString>(((JsonString *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonArrayType): {
				data.insert_or_assign(k, make_unique<JsonArray>(((JsonArray *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonMapType): {
				data.insert_or_assign(k, make_unique<JsonMap>(((JsonMap *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonBoolType): {
				data.insert_or_assign(k, make_unique<JsonBool>(((JsonBool *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonNull): {
				data.insert_or_assign(k, make_unique<JsonObj>());
				break;
			}
			default:
				break;
		}
	}
}
JsonMap::JsonMap(const JsonMap &m) : JsonMap(m.data) {
	// this->jsonBaseType = JsonBaseType::JsonMapType;
}
JsonMap &JsonMap::operator=(const JsonMap &o) {
	if (&o == this) return *this;
	jsonBaseType = JsonBaseType::JsonMapType;
	data.clear();
	using namespace std;
	for (auto &&[k, v] : o.data) {
		switch (v->jsonBaseType) {
			case (JsonBaseType::JsonIntegerType): {
				data.insert_or_assign(k, make_unique<JsonInteger>(((JsonInteger *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonDoubleType): {
				data.insert_or_assign(k, make_unique<JsonDouble>(((JsonDouble *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonStringType): {
				data.insert_or_assign(k, make_unique<JsonString>(((JsonString *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonArrayType): {
				data.insert_or_assign(k, make_unique<JsonArray>(((JsonArray *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonMapType): {
				data.insert_or_assign(k, make_unique<JsonMap>(((JsonMap *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonBoolType): {
				data.insert_or_assign(k, make_unique<JsonBool>(((JsonBool *)v.get())->data));
				break;
			}
			case (JsonBaseType::JsonNull): {
				data.insert_or_assign(k, make_unique<JsonObj>());
				break;
			}
			default:
				break;
		}
	}
	return *this;
}
JsonMap &JsonMap::operator=(JsonMap &&o) noexcept {
	if (&o == this) return *this;
	this->jsonBaseType = JsonObj::JsonBaseType::JsonMapType;
	this->data = std::move(o.data);
	return *this;
}
std::unique_ptr<JsonObj> JsonParser::parse(const std::string_view view) {
	using namespace std;
	ssize_t startIndex = 0;
	auto res = make_unique<JsonObj>();
	while (startIndex < view.size()) {
		switch (view[startIndex]) {
			case '{': {
				res = parseMap(view, startIndex);
				while (startIndex < view.size()) {
					switch (view[startIndex]) {
						case ' ':
						case '\r':
						case '\t':
						case '\n': {
							startIndex++;
							break;
						}
						default:
							throw JsonParseError("Json解析错误 : 末尾非法字符");
					}
				}
				return res;
			}
			case '[': {
				res = parseArray(view, startIndex);
				while (startIndex < view.size()) {
					cout << view.size() << endl;
					switch (view[startIndex]) {
						case ' ':
						case '\r':
						case '\t':
						case '\n': {
							startIndex++;
							break;
						}
						default:
							throw JsonParseError("Json解析错误 : 末尾非法字符");
					}
				}
				return res;
			}
			case ' ':
			case '\r':
			case '\t':
			case '\n': {
				startIndex++;
				break;
			}
			default:
				throw JsonParseError("Json解析错误 : 非法字符");
		}
	}
	return res;
};

std::unique_ptr<JsonObj> JsonParser::parseNum(const std::string_view view, ssize_t &startIndex) {
	using namespace std::string_literals;
	bool isDouble = false;
	auto getNum = [&]() -> void {
		ss_.str("");
		ss_.clear();
		for (ssize_t i = startIndex; i < view.size(); ++i) {
			switch (view[i]) {
				case '.': {
					isDouble = true;
					ss_.put('.');
					break;
				}
				case 'e': {
					isDouble = true;
					ss_.put('e');
					break;
				}
				case 'E': {
					isDouble = true;
					ss_.put('E');
					break;
				}
				case '-': {
					ss_.put('-');
					break;
				}
				case '+': {
					ss_.put('+');
					break;
				}
				case '0': {
					ss_.put('0');
					break;
				}
				case '1': {
					ss_.put('1');
					break;
				}
				case '2': {
					ss_.put('2');
					break;
				}
				case '3': {
					ss_.put('3');
					break;
				}
				case '4': {
					ss_.put('4');
					break;
				}
				case '5': {
					ss_.put('5');
					break;
				}
				case '6': {
					ss_.put('6');
					break;
				}
				case '7': {
					ss_.put('7');
					break;
				}
				case '8': {
					ss_.put('8');
					break;
				}
				case '9': {
					ss_.put('9');
					break;
				}
				default: {
					startIndex = i;
					return;
				}
			}
		}
		throw JsonParseError("字符串解析错误 : "s + std::string(view));
	};
	// size_t len = getLength();
	getNum();
	if (isDouble) {
		double num = 0;
		ss_ >> num;
		return std::make_unique<JsonDouble>(num);
	}
	int64_t num = 0;
	ss_ >> num;
	return std::make_unique<JsonInteger>(num);
}
std::unique_ptr<JsonString> JsonParser::parseString(const std::string_view view,
                                                    ssize_t &startIndex) {
	using namespace std;
	char ch = view[startIndex];
	assert(view[startIndex] == '\"');
	bool isEscape = false;

	string s;
	if (view.size() - startIndex <= 3) throw JsonParseError("字符串解析错误 : "s + string(view));
	startIndex++;

	auto unicodeToWchar = [](const char *ptr) -> wchar_t {
		wchar_t res = 0;
		for (int i = 0; i < 4; ++i) {
			char c = ptr[i];
			if ('0' <= c && c <= '9') {
				res |= ((c - '0') << (4 * (3 - i)));
			} else if ('a' <= c && c <= 'f') {
				res |= ((c - 'a' + 10) << (4 * (3 - i)));
			} else if ('A' <= c && c <= 'F') {
				res |= ((c - 'A' + 10) << (4 * (3 - i)));
			} else {
				throw JsonParseError("字符串解析错误 : \\u后为 "s + string(ptr, 4));
			}
		}
		return res;
	};
	auto getString = [&]() -> void {
		while (startIndex < view.size()) {
			char c = view[startIndex];
			if (c >= 0 && c <= 31) {
				throw JsonParseError("json字符串中不能含有控制字符");
			}
			switch (c) {
				case '\\': {
					startIndex++;
					if (startIndex >= view.size() - 2) {
						throw JsonParseError("字符串解析错误 : "s + string(view));
					}
					c = view[startIndex];
					if (c >= 0 && c <= 31) {
						throw JsonParseError("json字符串中不能含有控制字符");
					}
					switch (c) {
						case '\"': {
							s.push_back('\"');
							startIndex++;
							break;
						}
						case '\\': {
							s.push_back('\\');
							startIndex++;
							break;
						}
						case '/': {
							s.push_back('/');
							startIndex++;
							break;
						}
						case 'b': {
							s.push_back('\b');
							startIndex++;
							break;
						}
						case 'f': {
							s.push_back('\f');
							startIndex++;
							break;
						}
						case 'n': {
							s.push_back('\n');
							startIndex++;
							break;
						}
						case 'r': {
							s.push_back('\r');
							startIndex++;
							break;
						}
						case 't': {
							s.push_back('\t');
							startIndex++;
							break;
						}
						case 'u': {
							if (startIndex + 5 >= view.size() - 1) {
								throw JsonParseError("字符串解析错误 : "s + string(view));
							}

							auto utf8 = co_uring_web::utils::fromUnicode(
							    unicodeToWchar(&view[startIndex + 1]));
							for (int i = 0; i < utf8.bytes[0]; ++i) {
								s.push_back(utf8.bytes[i + 1]);
							}
							startIndex += 5;
							break;
						}
						default: {
							throw JsonParseError("字符串解析错误 : "s + string(view));
						}
					}
					break;
				}
				case '\"': {
					startIndex++;
					return;
				};
				default: {
					s.push_back(c);
					startIndex++;
					break;
				}
			}
		}
	};
	getString();

	return make_unique<JsonString>(s);
};
std::unique_ptr<JsonBool> JsonParser::parseBool(const std::string_view view, ssize_t &startIndex) {
	using namespace std;
	if (startIndex + 3 >= view.size() - 1)
		throw JsonParseError("bool类型解析错误 : "s + string(view));

	if (string_view {&(view[startIndex]), 4} == "true") {
		startIndex += 4;
		return make_unique<JsonBool>(true);
	}
	if (startIndex + 5 <= view.size() - 1 && string_view {&(view[startIndex]), 5} == "false") {
		startIndex += 5;
		return make_unique<JsonBool>(false);
	}
	throw JsonParseError("bool类型解析错误 : "s + string(view));

}  // namespace utils

std::unique_ptr<JsonArray> JsonParser::parseArray(const std::string_view view,
                                                  ssize_t &startIndex) {
	using namespace std;
	assert(view[startIndex] == '[');
	if (startIndex + 1 > view.size() - 1) throw JsonParseError("列表解析错误 : 方括号错误");
	startIndex++;
	auto res = make_unique<JsonArray>();
	vector<unique_ptr<JsonObj>> &vec = res->data;
	bool endByComma = false;
	while (startIndex < view.size()) {
		char c = view[startIndex];
		switch (c) {
			case ',': {
				if (endByComma) throw JsonParseError("列表解析错误 : 逗号错误");
				endByComma = true;
				startIndex++;
				break;
			}
			case '[': {
				vec.push_back(parseArray(view, startIndex));
				endByComma = false;
				startIndex++;
				break;
			}
			case ']': {
				startIndex++;
				return res;
			}
			case '{': {
				vec.push_back(parseMap(view, startIndex));
				endByComma = false;
				break;
			}
			case 'n': {
				if (startIndex + 4 < view.size() && string_view(&(view[startIndex]), 4) == "null") {
					vec.emplace_back();
					startIndex += 4;
					endByComma = false;
					break;
				}
				throw JsonParseError("列表解析错误 : 非法字符 " +
				                     string(view.data() + startIndex, view.size() - startIndex));
			}
			case 't':
			case 'f': {
				vec.push_back(parseBool(view, startIndex));
				endByComma = false;
				break;
			}
			case '\"': {
				vec.push_back(parseString(view, startIndex));
				endByComma = false;
				break;
			}
			case ' ':
			case '\r':
			case '\t':
			case '\n': {
				startIndex++;
				break;
			}
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9': {
				vec.push_back(parseNum(view, startIndex));
				endByComma = false;
				break;
			}
			default:
				throw JsonParseError("列表解析错误 : 非法字符");
		}
	}
	throw JsonParseError("列表解析错误 : 非法字符");
}
std::unique_ptr<JsonMap> JsonParser::parseMap(const std::string_view view, ssize_t &startIndex) {
	using namespace std;
	assert(view[startIndex] == '{');
	if (startIndex + 1 > view.size() - 1) throw JsonParseError("列表解析错误 : 大括号错误");
	startIndex++;
	auto res = make_unique<JsonMap>();
	enum parseState { keyStart = 0, keyEnd = 1, valueStart = 2, valueEnd = 3 };
	parseState state;
	state = keyStart;
	string str;
	while (startIndex < view.size()) {
		char c = view[startIndex];

		switch (c) {
			case ' ':
			case '\r':
			case '\t':
			case '\n': {
				startIndex++;
				break;
			}
			case ',': {
				if (state != valueEnd) {
					throw JsonParseError("Map解析错误 : 逗号错误");
				};
				state = keyStart;
				startIndex++;
				break;
			}
			case '\"': {
				if (state == keyStart) {
					swap(str, parseString(view, startIndex)->data);
					state = keyEnd;
					break;
				}
				if (state == valueStart) {
					res->data.insert_or_assign(move(str), move(parseString(view, startIndex)));
					state = valueEnd;
					break;
				}
				throw JsonParseError("Map解析错误 : 格式错误");
			}
			case ':': {
				if (state != keyEnd) {
					throw JsonParseError("Map解析错误 : 格式错误");
				}
				startIndex++;
				state = valueStart;
				break;
			}
			case 't':
			case 'f': {
				if (state != valueStart) {
					throw JsonParseError("Map解析错误 : 格式错误");
				}
				res->data.insert_or_assign(move(str), move(parseBool(view, startIndex)));
				state = valueEnd;
				break;
			}
			case 'n': {
				if (state != valueStart) {
					throw JsonParseError("Map解析错误 : 格式错误");
				}
				if (startIndex + 4 < view.size() && string_view(&(view[startIndex]), 4) == "null") {
					res->data.insert_or_assign(move(str), make_unique<JsonObj>());
					state = valueEnd;
					startIndex += 4;
					break;
				}
				throw JsonParseError("Map解析错误 : 非法字符");
			}
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9': {
				if (state != valueStart) {
					throw JsonParseError("Map解析错误 : 格式错误");
				}
				res->data.insert_or_assign(move(str), move(parseNum(view, startIndex)));
				state = valueEnd;
				break;
			}
			case '[': {
				if (state != valueStart) {
					throw JsonParseError("Map解析错误 : 格式错误");
				}
				res->data.insert_or_assign(move(str), parseArray(view, startIndex));
				state = valueEnd;
				break;
			}
			case '{': {
				if (state != valueStart) {
					throw JsonParseError("Map解析错误 : 格式错误");
				}
				res->data.insert_or_assign(move(str), parseMap(view, startIndex));
				state = valueEnd;
				break;
			}
			case '}': {
				if (state != valueEnd) {
					throw JsonParseError("Map解析错误 : 格式错误");
				}
				startIndex++;
				return res;
			}
			default: {
				throw JsonParseError("Map解析错误 : 非法字符");
			}
		}
	}
	throw JsonParseError("Map解析错误 : 格式错误");
}

std::string JsonParser::dump(const JsonObj *obj, int indent) {
	using namespace std;
	string res;
	res.reserve(2048);
	switch (obj->jsonBaseType) {
		case JsonObj::JsonBaseType::JsonArrayType: {
			dumpArray((const JsonArray *)obj, res, 0, indent, true);
			return res;
		}
		case JsonObj::JsonBaseType::JsonMapType: {
			dumpMap((const JsonMap *)obj, res, 0, indent, true);
			return res;
		}
		default: {
			throw JsonParseError("dump错误 : 不是array或map类型");
		}
	}
};
void JsonParser::dumpNum(const JsonObj *jn, std::string &res) {
	assert(jn->jsonBaseType == JsonObj::JsonBaseType::JsonIntegerType ||
	       jn->jsonBaseType == JsonObj::JsonBaseType::JsonDoubleType);
	if (jn->jsonBaseType == JsonObj::JsonBaseType::JsonIntegerType) {
		ss_.str("");
		ss_.clear();
		ss_ << static_cast<const JsonInteger *>(jn)->data;
		res.append(ss_.str());
	} else {
		ss_.str("");
		ss_.clear();
		ss_ << static_cast<const JsonDouble *>(jn)->data;
		res.append(ss_.str());
	}
	return;
};

void JsonParser::dumpString(const JsonString *js, std::string &res) { dumpString(js->data, res); }
void JsonParser::dumpString(const std::string_view view, std::string &res) {
	res.push_back('\"');
	for (const char c : view) {
		switch (c) {
			case '\"': {
				res.append("\\\"");
				break;
			}
			case '\\': {
				res.append("\\\\");
				break;
			}
			// case '/': {
			//     res.append("\\/");
			//     break;
			// }
			case '\b': {
				res.append("\\b");
				break;
			}
			case '\f': {
				res.append("\\f");
				break;
			}
			case '\n': {
				res.append("\\n");
				break;
			}
			case '\r': {
				res.append("\\r");
				break;
			}
			case '\t': {
				res.append("\\t");
				break;
			}
			default: {
				res.push_back(c);
				break;
			}
		}
	}
	res.push_back('\"');
}
void JsonParser::dumpBool(const JsonBool *jb, std::string &res) {
	if (jb->data) {
		res.append("true");

	} else {
		res.append("false");
	}
}
void JsonParser::dumpArray(const JsonArray *ja, std::string &res, int depth, int indent,
                           bool singleLine) {
	res.push_back('[');
	if (ja->data.empty()) {
		res.push_back(']');
		return;
	}
	for (auto it = ja->data.begin(); it != ja->data.end(); ++it) {
		const auto &o = *it;
		res.push_back('\n');
		for (int i = 0; i < depth + 1; ++i) {
			for (int j = 0; j < indent; ++j) {
				res.push_back(' ');
			}
		}
		switch (o->jsonBaseType) {
			case JsonObj::JsonBaseType::JsonStringType: {
				dumpString((const JsonString *)o.get(), res);
				break;
			}
			case JsonObj::JsonBaseType::JsonBoolType: {
				dumpBool((const JsonBool *)o.get(), res);
				break;
			}
			case JsonObj::JsonBaseType::JsonDoubleType:
			case JsonObj::JsonBaseType::JsonIntegerType: {
				dumpNum((const JsonObj *)o.get(), res);
				break;
			}
			case JsonObj::JsonBaseType::JsonNull: {
				dumpNull(res);
				break;
			}
			case JsonObj::JsonBaseType::JsonArrayType: {
				dumpArray((const JsonArray *)o.get(), res, depth + 1, indent, true);
				break;
			}
			case JsonObj::JsonBaseType::JsonMapType: {
				dumpMap((const JsonMap *)o.get(), res, depth + 1, indent, true);
				break;
			}
		}
		if (std::distance(it, ja->data.end()) != 1) {
			res.push_back(',');
		}
	}
	res.push_back('\n');
	for (int i = 0; i < depth; ++i) {
		for (int j = 0; j < indent; ++j) {
			res.push_back(' ');
		}
	}
	res.push_back(']');
}
void JsonParser::dumpMap(const JsonMap *jm, std::string &res, int depth, int indent,
                         bool singleLine) {
	res.push_back('{');
	if (jm->data.empty()) {
		res.push_back('}');
		return;
	}
	for (auto it = jm->data.begin(); it != jm->data.end(); ++it) {
		const auto &o = *it;
		res.push_back('\n');
		for (int i = 0; i < depth + 1; ++i) {
			for (int j = 0; j < indent; ++j) {
				res.push_back(' ');
			}
		}
		dumpString(o.first, res);
		res.append(" : ");
		const auto &v = o.second;
		switch (v->jsonBaseType) {
			case JsonObj::JsonBaseType::JsonStringType: {
				dumpString((const JsonString *)v.get(), res);
				break;
			}
			case JsonObj::JsonBaseType::JsonBoolType: {
				dumpBool((const JsonBool *)v.get(), res);
				break;
			}
			case JsonObj::JsonBaseType::JsonDoubleType:
			case JsonObj::JsonBaseType::JsonIntegerType: {
				dumpNum((const JsonObj *)v.get(), res);
				break;
			}
			case JsonObj::JsonBaseType::JsonNull: {
				dumpNull(res);
				break;
			}
			case JsonObj::JsonBaseType::JsonArrayType: {
				dumpArray((const JsonArray *)v.get(), res, depth + 1, indent, false);
				break;
			}
			case JsonObj::JsonBaseType::JsonMapType: {
				dumpMap((const JsonMap *)v.get(), res, depth + 1, indent, false);
				break;
			}
		}
		if (std::distance(it, jm->data.end()) != 1) {
			res.push_back(',');
		}
	}
	res.push_back('\n');
	for (int i = 0; i < depth; ++i) {
		for (int j = 0; j < indent; ++j) {
			res.push_back(' ');
		}
	}
	res.push_back('}');
}

}  // namespace co_uring_web::utils