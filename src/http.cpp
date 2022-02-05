#include "http.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

#include "logger.h"
#include "utils.h"
namespace co_uring_web {
std::string_view getStatusText(HttpResponseStatusCodes code) {
	using namespace std;
	switch (code) {
		case HttpResponseStatusCodes::C100:
			return "Continue"sv;
		case HttpResponseStatusCodes::C101:
			return "Switching Protocols"sv;
		case HttpResponseStatusCodes::C102:
			return "Processing"sv;
		case HttpResponseStatusCodes::C103:
			return "Early Hints"sv;
		case HttpResponseStatusCodes::C200:
			return "OK"sv;
		case HttpResponseStatusCodes::C201:
			return "Created"sv;
		case HttpResponseStatusCodes::C202:
			return "Accepted"sv;
		case HttpResponseStatusCodes::C203:
			return "Non-Authoritative Information"sv;
		case HttpResponseStatusCodes::C204:
			return "No Content"sv;
		case HttpResponseStatusCodes::C205:
			return "Reset Content"sv;
		case HttpResponseStatusCodes::C206:
			return "Partial Content"sv;
		case HttpResponseStatusCodes::C207:
			return "Multi-Status"sv;
		case HttpResponseStatusCodes::C208:
			return "Already Reported"sv;
		case HttpResponseStatusCodes::C226:
			return "IM Used (RFC 3229)"sv;
		case HttpResponseStatusCodes::C300:
			return "Multiple Choices"sv;
		case HttpResponseStatusCodes::C301:
			return "Moved Permanently"sv;
		case HttpResponseStatusCodes::C302:
			return "Moved temporarily"sv;
		case HttpResponseStatusCodes::C303:
			return "See Other"sv;
		case HttpResponseStatusCodes::C304:
			return "Not Modified"sv;
		case HttpResponseStatusCodes::C305:
			return "Use Proxy"sv;
		case HttpResponseStatusCodes::C306:
			return "Switch Proxy"sv;
		case HttpResponseStatusCodes::C307:
			return "Temporary Redirect"sv;
		case HttpResponseStatusCodes::C308:
			return "Permanent Redirect"sv;
		case HttpResponseStatusCodes::C400:
			return "Bad Request"sv;
		case HttpResponseStatusCodes::C401:
			return "Unauthorized"sv;
		case HttpResponseStatusCodes::C402:
			return "Payment Required"sv;
		case HttpResponseStatusCodes::C403:
			return "Forbidden"sv;
		case HttpResponseStatusCodes::C404:
			return "Not Found"sv;
		case HttpResponseStatusCodes::C405:
			return "Method Not Allowed"sv;
		case HttpResponseStatusCodes::C406:
			return "Not Acceptable"sv;
		case HttpResponseStatusCodes::C407:
			return "Proxy Authentication Required"sv;
		case HttpResponseStatusCodes::C408:
			return "Request Timeout"sv;
		case HttpResponseStatusCodes::C409:
			return "Conflict"sv;
		case HttpResponseStatusCodes::C410:
			return "Gone"sv;
		case HttpResponseStatusCodes::C411:
			return "Length Required"sv;
		case HttpResponseStatusCodes::C412:
			return "Precondition Failed"sv;
		case HttpResponseStatusCodes::C413:
			return "Payload Too Large"sv;
		case HttpResponseStatusCodes::C414:
			return "URI Too Long"sv;
		case HttpResponseStatusCodes::C415:
			return "Unsupported Media Type"sv;
		case HttpResponseStatusCodes::C416:
			return "Range Not Satisfiable"sv;
		case HttpResponseStatusCodes::C417:
			return "Expectation Failed"sv;
		case HttpResponseStatusCodes::C418:
			return "I'm a teapot"sv;
		case HttpResponseStatusCodes::C421:
			return "Misdirected Request"sv;
		case HttpResponseStatusCodes::C422:
			return "Unprocessable Entity"sv;
		case HttpResponseStatusCodes::C423:
			return "Locked"sv;
		case HttpResponseStatusCodes::C424:
			return "Failed Dependency"sv;
		case HttpResponseStatusCodes::C425:
			return "Too Early"sv;
		case HttpResponseStatusCodes::C426:
			return "Upgrade Required"sv;
		case HttpResponseStatusCodes::C428:
			return "Precondition Required"sv;
		case HttpResponseStatusCodes::C429:
			return "Too Many Requests"sv;
		case HttpResponseStatusCodes::C431:
			return "Request Header Fields Too Large"sv;
		case HttpResponseStatusCodes::C451:
			return "Unavailable For Legal Reasons"sv;
		case HttpResponseStatusCodes::C500:
			return "Internal Server Error"sv;
		case HttpResponseStatusCodes::C501:
			return "Not Implemented"sv;
		case HttpResponseStatusCodes::C502:
			return "Bad Gateway"sv;
		case HttpResponseStatusCodes::C503:
			return "Service Unavailable"sv;
		case HttpResponseStatusCodes::C504:
			return "Gateway Timeout"sv;
		case HttpResponseStatusCodes::C505:
			return "HTTP Version Not Supported"sv;
		case HttpResponseStatusCodes::C506:
			return "Variant Also Negotiates"sv;
		case HttpResponseStatusCodes::C507:
			return "Insufficient Storage"sv;
		case HttpResponseStatusCodes::C508:
			return "Loop Detected"sv;
		case HttpResponseStatusCodes::C510:
			return "Not Extended"sv;
		case HttpResponseStatusCodes::C511:
			return "Network Authentication Required"sv;
		default:
			return ""sv;
	}
};
std::string_view getContentType(std::string_view fileExt) {
	using namespace std;
	using namespace utils;

	uint64_t hashVal = stringHash(fileExt);
	switch (hashVal) {
		case stringHashConstexpr("jpg"sv):[[fallthrough]];
		case stringHashConstexpr("jpeg"sv):{
			return "image/jpeg"sv;
		}
		case stringHashConstexpr("png"sv):{
			return "image/png"sv;
		}
		case stringHashConstexpr("gif"sv):{
			return "image/gif"sv;
		}
		case stringHashConstexpr("htm"sv):[[fallthrough]];
		case stringHashConstexpr("html"sv):{
			return "text/html"sv;
		}
		case stringHashConstexpr("js"sv):{
			return "application/javascript"sv;
		}
		case stringHashConstexpr("css"sv):{
			return "text/css"sv;
		}
		case stringHashConstexpr("txt"sv):{
			return "text/plain"sv;
		}
		default:{
			return {};
		}
	}
};
std::optional<HttpRequest> HttpRequest::fromRawData(std::string_view raw) {
	using namespace std;
	HttpRequest res;

	int index = 0;
	try {
		//解析请求方法
		switch (raw.at(index)) {
			case 'G': {
				if (raw.substr(index, 3) == "GET"sv) {
					res.method_ = GET;
					index = index + 3;
					break;
				}
				throw invalid_argument("http请求方法错误");
			}
			case 'P': {
				if (raw.substr(index, 4) == "POST"sv) {
					res.method_ = POST;
					index += 4;
				} else if (raw.substr(index, 3) == "PUT"sv) {
					res.method_ = PUT;
					index += 3;
				} else if (raw.substr(index, 5) == "PATCH"sv) {
					res.method_ = PATCH;
					index += 5;
				} else {
					throw invalid_argument("http请求方法错误");
				}
				break;
			}
			case 'D': {
				if (raw.substr(index, 6) == "DELETE"sv) {
					res.method_ = DELETE;
					index += 6;
				} else {
					throw invalid_argument("http请求方法错误");
				}
				break;
			}
			case 'H': {
				if (raw.substr(index, 4) == "HEAD"sv) {
					res.method_ = HEAD;
					index += 4;
				} else {
					throw invalid_argument("http请求方法错误");
				}
				break;
			}
			default: {
				throw invalid_argument("http请求方法错误");
			}
		}
		if (raw.at(index) != ' ') {
			throw invalid_argument("http请求方法后不是空格");
		}
		index++;
		int index_begin = index;
		while (raw.at(index) != ' ') {
			res.url_.push_back(raw[index]);
			index++;
		}
		index++;
		if (raw.substr(index, 5) != "HTTP/"sv) {
			throw invalid_argument("http版本错误");
		}
		index += 5;
		if (raw.substr(index, 3) == "0.9"sv) {
			res.version_ = V9;
		} else if (raw.substr(index, 3) == "1.0"sv) {
			res.version_ = V10;
		} else if (raw.substr(index, 3) == "1.1"sv) {
			res.version_ = V11;
		} else {
			throw invalid_argument("http版本错误");
		}
		index += 3;
		if (raw.substr(index, 2) != "\r\n"sv) {
			throw invalid_argument("http请求格式错误");
		}
		index += 2;
		while (raw.substr(index, 2) != "\r\n"sv) {
			index_begin = index;
			while (raw.at(index) != ':') {
				index++;
			}
			string key(raw.substr(index_begin, index - index_begin));
			index++;
			while (raw.at(index) == ' ') {
				index++;
			}
			index_begin = index;
			while (raw.at(index) != '\r') {
				index++;
			}
			res.headers_.insert({move(key), string(raw.substr(index_begin, index - index_begin))});
			if (raw.substr(index, 2) != "\r\n"sv) {
				throw invalid_argument("http请求头格式错误");
			}
			index += 2;
		}
		index += 2;
		if (index < raw.size() - 1) {
			res.body_ = raw.substr(index, raw.size() - index);
		}
		return res;
	} catch (const out_of_range &oor) {
		LOG_WARN << oor << " : index:" << index << " ; request:" << raw;
		return {};
	} catch (const invalid_argument &ia) {
		LOG_WARN << ia << " : index:" << index << " ; request:" << raw;
		return {};
	}
}
std::string HttpResponse::toRawData() {
	using namespace std;
	//先确定长度
	size_t resSize = 0;
	//"HTTP/1.0 200 "
	resSize += 13;
	// status text
	resSize += getStatusText(statusCode_).size();
	//\r\n
	resSize += 2;  // request line长度

	for (auto &header : headers_) {
		// key长度+冒号+空格+value长度+\r\n
		resSize += header.first.size() + 1 + 1 + header.second.size() + 2;
	}
	//空行
	resSize += 2;
	resSize += body_.size();
	string res(resSize, 0);
	try {
		int index = 0;
		memcpy(res.data() + index, "HTTP/"sv.data(), "HTTP/"sv.size());
		index += "HTTP/"sv.size();
		switch (version_) {
			case HttpVersion::VERSION_NOTHING: {
				throw std::invalid_argument("http response version 为 NOTHING");
			}
			case HttpVersion::V10: {
				memcpy(res.data() + index, "1.0"sv.data(), "1.0"sv.size());
				index += "1.0"sv.size();
				break;
			}
			case HttpVersion::V11: {
				memcpy(res.data() + index, "1.1"sv.data(), "1.1"sv.size());
				index += "1.1"sv.size();
				break;
			}
			case HttpVersion::V9: {
				memcpy(res.data(), "0.9"sv.data(), "0.9"sv.size());
				index += "0.9"sv.size();
				break;
			}
			default: {
				throw std::invalid_argument("未知的 http response version");
			}
		}
		res.at(index) = ' ';  //空格
		index++;
		memcpy(res.data() + index, to_string(version_).data(), 3);  //状态码
		index += 3;
		res.at(index) = ' ';  //空格
		index++;
		auto statusText = getStatusText(statusCode_);
		if (statusText.empty()) {
			throw std::invalid_argument("未知的 http response status code");
		}
		memcpy(res.data() + index, statusText.data(), statusText.size());  //状态文字
		index += statusText.size();
		memcpy(res.data() + index, "\r\n", 2);  //\r\n
		index += 2;
		for (auto &[key, value] : headers_) {
			memcpy(res.data() + index, key.data(), key.size());
			index += key.size();
			res.at(index) = ':';
			index++;
			res.at(index) = ' ';
			index++;
			memcpy(res.data() + index, value.data(), value.size());
			index += value.size();
			res.at(index) = '\r';
			index++;
			res.at(index) = '\n';
			index++;
		}
		res.at(index) = '\r';
		index++;
		res.at(index) = '\n';
		index++;
		memcpy(res.data() + index, body_.data(), body_.size());
		index += body_.size();
		if (index != res.size()) {
			throw out_of_range("http response 转化时越界");
		}
		return res;

	} catch (const exception &e) {
		LOG_WARN << e;
		return {};
	}
};
};  // namespace co_uring_web