/**
 *@文件    :http.h
 *@时间    :2022/01/27 16:01:19
 *@作者    :周恒
 *@版本    :1.0
 *@说明    :
 **/

#pragma once
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <zlib.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <experimental/coroutine>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "config.h"
#include "core.h"
#include "logger.h"
#include "utils.h"
namespace co_uring_web {
static constexpr std::string_view ServerString = "co_uring_httpd/0.1";
static constexpr char content400[] =
    "HTTP/1.0 400 Bad Request\r\n"
    "Content-type: text/html\r\n"
    "\r\n"
    "<html>"
    "<head>"
    "<title>co_uring_httpd: Unimplemented</title>"
    "</head>"
    "<body>"
    "<h1>Bad Request (Unimplemented)</h1>"
    "<p>Your client sent a request co_uring_httpd did not understand and it is probably not your "
    "fault.</p>"
    "</body>"
    "</html>";
static constexpr char content404[] =
    "HTTP/1.0 404 Not Found\r\n"
    "Content-type: text/html\r\n"
    "\r\n"
    "<html>"
    "<head>"
    "<title>co_uring_httpd: Not Found</title>"
    "</head>"
    "<body>"
    "<h1>Not Found (404)</h1>"
    "<p>Your client is asking for an object that was not found on this server.</p>"
    "</body>"
    "</html>";
enum HttpMethod { METHOD_NOTHING = 0, GET, POST, HEAD, PUT, PATCH, DELETE };
enum HttpVersion { VERSION_NOTHING = 0, V9, V10, V11 };
enum HttpResponseStatusCodes {
	C000 = 0,
	C100 = 100,  // Continue
	C101 = 101,  // Switching Protocols
	C102 = 102,  // Processing (WebDAV; RFC 2518)
	C103 = 103,  // Early Hints (RFC 8297)
	C200 = 200,  // OK
	C201 = 201,  // Created
	C202 = 202,  // Accepted
	C203 = 203,  // Non-Authoritative Information (since HTTP/1.1)
	C204 = 204,  // No Content
	C205 = 205,  // Reset Content
	C206 = 206,  // Partial Content (RFC 7233)
	C207 = 207,  // Multi-Status (WebDAV; RFC 4918)
	C208 = 208,  // Already Reported (WebDAV; RFC 5842)
	C226 = 226,  // IM Used (RFC 3229)
	C300 = 300,  // Multiple Choices
	C301 = 301,  // Moved Permanently
	C302 = 302,  // Found (Previously "Moved temporarily")
	C303 = 303,  // See Other (since HTTP/1.1)
	C304 = 304,  // Not Modified (RFC 7232)
	C305 = 305,  // Use Proxy (since HTTP/1.1)
	C306 = 306,  // Switch Proxy
	C307 = 307,  // Temporary Redirect (since HTTP/1.1)
	C308 = 308,  // Permanent Redirect (RFC 7538)
	C400 = 400,  // Bad Request
	C401 = 401,  // Unauthorized (RFC 7235)
	C402 = 402,  // Payment Required
	C403 = 403,  // Forbidden
	C404 = 404,  // Not Found
	C405 = 405,  // Method Not Allowed
	C406 = 406,  // Not Acceptable
	C407 = 407,  // Proxy Authentication Required (RFC 7235)
	C408 = 408,  // Request Timeout
	C409 = 409,  // Conflict
	C410 = 410,  // Gone
	C411 = 411,  // Length Required
	C412 = 412,  // Precondition Failed (RFC 7232)
	C413 = 413,  // Payload Too Large (RFC 7231)
	C414 = 414,  // URI Too Long (RFC 7231)
	C415 = 415,  // Unsupported Media Type (RFC 7231)
	C416 = 416,  // Range Not Satisfiable (RFC 7233)
	C417 = 417,  // Expectation Failed
	C418 = 418,  // I'm a teapot (RFC 2324, RFC 7168)
	C421 = 421,  // Misdirected Request (RFC 7540)
	C422 = 422,  // Unprocessable Entity (WebDAV; RFC 4918)
	C423 = 423,  // Locked (WebDAV; RFC 4918)
	C424 = 424,  // Failed Dependency (WebDAV; RFC 4918)
	C425 = 425,  // Too Early (RFC 8470)
	C426 = 426,  // Upgrade Required
	C428 = 428,  // Precondition Required (RFC 6585)
	C429 = 429,  // Too Many Requests (RFC 6585)
	C431 = 431,  // Request Header Fields Too Large (RFC 6585)
	C451 = 451,  // Unavailable For Legal Reasons (RFC 7725)
	C500 = 500,  // Internal Server Error
	C501 = 501,  // Not Implemented
	C502 = 502,  // Bad Gateway
	C503 = 503,  // Service Unavailable
	C504 = 504,  // Gateway Timeout
	C505 = 505,  // HTTP Version Not Supported
	C506 = 506,  // Variant Also Negotiates (RFC 2295)
	C507 = 507,  // Insufficient Storage (WebDAV; RFC 4918)
	C508 = 508,  // Loop Detected (WebDAV; RFC 5842)
	C510 = 510,  // Not Extended (RFC 2774)
	C511 = 511   // Network Authentication Required (RFC 6585)
};
static constexpr int defaultHttpRequestReadBufferSize = 8192;
std::string_view getStatusText(HttpResponseStatusCodes code);
/**
 * @brief Get the Content Type object 如果是未知类型返回空字符串
 *
 * @param fileExt
 * @return std::string_view
 */
std::string_view getContentType(std::string_view fileExt);
class HttpRequest {
	std::string url_;
	std::map<std::string, std::string> headers_;
	std::string body_;
	HttpMethod method_ {METHOD_NOTHING};
	HttpVersion version_ {VERSION_NOTHING};

   public:
	[[nodiscard]] inline std::string getUrl() const {
		using namespace std;
		if (url_ == "/"sv) {
			return "index.html";
		}
		if (url_.front() == '/') {
			return url_.substr(1);
		}
		return url_;
	};
	inline void setUrl(const std::string &url) { url_ = url; }
	inline void setUrl(std::string &&url) { url_.swap(url); }

	[[nodiscard]] inline HttpMethod getMethod() const { return method_; }
	inline void setMethod(HttpMethod method) { method_ = method; };

	[[nodiscard]] inline std::string_view getBody() const { return body_; }
	inline void setBody(const std::string &body) { body_ = body; }
	inline void setBody(std::string &&body) { body_.swap(body); }

	[[nodiscard]] inline std::map<std::string, std::string> &getHeaders() { return headers_; }
	inline void setHeaders(const std::map<std::string, std::string> &headers) {
		headers_ = headers;
	}
	inline void setHeaders(std::map<std::string, std::string> &&headers) { headers_.swap(headers); }
	/**
	 * @brief Set the Header object 增加或设置一个header键值对
	 *
	 * @param key
	 * @param value
	 */
	inline void setHeader(const std::string &key, const std::string &value) {
		headers_.insert_or_assign(key, value);
	}
	/**
	 * @brief Get the Header Value object 如果不存在，返回 ""
	 *
	 * @param key
	 * @return std::string
	 */
	[[nodiscard]] inline std::string getHeaderValue(const std::string &key) const {
		auto it = headers_.find(key);
		if (it == headers_.end()) {
			return {};
		}
		return it->second;
	}
	static std::optional<HttpRequest> fromRawData(std::string_view raw);
};
class HttpResponse {
	HttpVersion version_;
	HttpResponseStatusCodes statusCode_;
	std::map<std::string, std::string> headers_;
	std::string body_;

   public:
	HttpResponse(HttpVersion version = V10, HttpResponseStatusCodes status_code = C200)
	    : version_(version), statusCode_(status_code) {};
	HttpResponse(const HttpResponse &) = default;
	HttpResponse(HttpResponse &&) = default;
	HttpResponse &operator=(const HttpResponse &other) {
		if (this == &other) [[unlikely]]
			return *this;
		using namespace std;
		version_ = other.version_;
		statusCode_ = other.statusCode_;
		headers_ = other.headers_;
		body_ = other.body_;
		return *this;
	};
	HttpResponse &operator=(HttpResponse &&other) {
		if (this == &other) [[unlikely]]
			return *this;
		version_ = other.version_;
		statusCode_ = other.statusCode_;
		headers_.swap(other.headers_);
		body_.swap(other.body_);
		return *this;
	};
	inline void setVersion(HttpVersion version) { version_ = version; }
	[[nodiscard]] inline HttpVersion getVersion() const { return version_; }
	inline void setStatusCode(HttpResponseStatusCodes status_code) { statusCode_ = status_code; }
	[[nodiscard]] inline HttpResponseStatusCodes getStatusCode() const { return statusCode_; }
	inline void setHeaders(const std::map<std::string, std::string> &headers) {
		headers_ = headers;
	}
	inline void setHeaders(std::map<std::string, std::string> &&headers) { headers_.swap(headers); }
	[[nodiscard]] std::map<std::string, std::string> &getHeaders() { return headers_; }
	inline void setHeader(std::string_view key, std::string_view value) {
		headers_.insert_or_assign(std::string(key), std::string(value));
	}
	[[nodiscard]] inline std::string getHeaderValue(const std::string &key) const {
		auto it = headers_.find(key);
		if (it == headers_.end()) {
			return {};
		}
		return it->second;
	}
	inline void setBody(std::string &&body) { body_.swap(body); }
	inline void setBody(const std::string &body) { body_ = body; }
	[[nodiscard]] inline std::string_view getBody() const { return body_; }
	std::string toRawData();
	int getRawDataSizeWithoutBody();
	void toRawDataWithoutBody(char *buffer);
};
struct HttpTask {
	struct promise_type {
		HttpTask get_return_object() {
			return std::experimental::coroutine_handle<promise_type>::from_promise(*this);
		}
		auto initial_suspend() noexcept { return std::experimental::suspend_never {}; }
		auto final_suspend() noexcept { return std::experimental::suspend_never {}; }
		void unhandled_exception() {};
		void return_void() {};
	};
	std::experimental::coroutine_handle<promise_type> handle;
	HttpTask(std::experimental::coroutine_handle<promise_type> h) : handle(h) {};
};

template <class SchedulerImpl>
using HttpScheduler = typename core::Scheduler<SchedulerImpl, HttpTask>;

template <class SchedulerImpl>
static HttpTask static_web_http(core::TcpConnection conn, HttpScheduler<SchedulerImpl> *scheduler) {
	using namespace std;
	using namespace core;
	namespace fs = filesystem;
	//先读
	IoRequest req = {};
	unique_ptr<char[]> reqBuff = std::make_unique<char[]>(defaultHttpRequestReadBufferSize);
	req.data = reqBuff.get();
	req.capicaty = defaultHttpRequestReadBufferSize;
	req.fd = conn.fd;

	co_await scheduler->asyncRead(&req);
	if (req.retCode <= 0) {
		LOG_WARN << "http 读异常,来自 " << utils::addr2str(conn.remoteAddr) << ':'
		         << ntohs(conn.remoteAddr.sin_port);
		close(conn.fd);
		co_return;
	}
	//解析http参数
	auto httpRequest = HttpRequest::fromRawData(string_view {req.data, (size_t)req.retCode});

	if (!httpRequest.has_value()) {
		LOG_WARN << "http request解析错误,来自 " << utils::addr2str(conn.remoteAddr) << ':'
		         << ntohs(conn.remoteAddr.sin_port) << ' '
		         << string_view {req.data, (size_t)req.retCode};
		close(conn.fd);
		co_return;
	}
	auto url = httpRequest->getUrl();
	fs::path abspath = fs::path(Config::getInstance().get_http_dir()) / url;
	// fprintf(stderr, "%s",abspath.c_str());
	int fd = open(abspath.c_str(), O_RDONLY);
	if (fd == -1) {
		int err = errno;
		char errBuff[64];

		LOG_INFO << "http打开文件路径失败: " << url << " " << strerror_r(err, errBuff, 64);

		IoRequest req;
		req.fd = conn.fd;
		req.data = (char *)content404;
		req.size = sizeof(content404);
		co_await scheduler->asyncWrite(&req);
		close(conn.fd);
		co_return;
	}
	int fileSize = utils::getFileSizeByFd(fd);

	if (fileSize == -1) {
		int err = errno;
		char errBuff[64];
		close(fd);
		LOG_INFO << "http打开文件路径失败: " << url << " " << strerror_r(err, errBuff, 64);

		IoRequest req;
		req.fd = conn.fd;
		req.data = (char *)content404;
		req.size = sizeof(content404);
		co_await scheduler->asyncWrite(&req);
		close(conn.fd);
		co_return;
	}
	auto response = HttpResponse();

	auto fileExtnameStart = url.find_last_of('.');
	if (fileExtnameStart != string_view::npos) {
		fileExtnameStart += 1;
		auto fileExtname = url.substr(fileExtnameStart);
		auto contentType = getContentType(fileExtname);
		if (!contentType.empty()) {
			response.setHeader("Content-Type"sv, contentType);
		}
	}

	response.setHeader("server"sv, ServerString);
	response.setHeader("Content-Length"sv, to_string(fileSize));
	int headersSize = response.getRawDataSizeWithoutBody();
	auto res = std::make_unique<char[]>(headersSize + fileSize);

	response.toRawDataWithoutBody(res.get());
	int ret = read(fd, res.get() + headersSize, fileSize);
	if (ret == -1) {
		int err = errno;
		char errBuff[64];
		close(fd);
		LOG_INFO << "http读取文件失败: " << url << " " << strerror_r(err, errBuff, 64);

		IoRequest req;
		req.fd = conn.fd;
		req.data = (char *)content404;
		req.size = sizeof(content404);
		co_await scheduler->asyncWrite(&req);
		close(conn.fd);
		co_return;
	}
	close(fd);

	// response
	req = IoRequest {};
	req.fd = conn.fd;
	req.data = res.get();
	req.size = headersSize + fileSize;

	co_await scheduler->asyncWrite(&req);
	close(conn.fd);
	co_return;
};
}  // namespace co_uring_web