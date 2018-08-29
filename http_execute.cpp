#include "http_execute.h"

http_reply http_execute::http_get(String ip, String path)
{
	String url = "http://" + ip + ":80" + path;

	HTTPClient http;
	http.begin(url);

	int code = http.GET();

	http_reply reply;

	//Code will be 0 or below during an internal error
	if (code > 0) {
		reply.body = http.getString();
		reply.code = (t_http_codes)code;
	}
	else {
		reply.code = HTTP_CODE_INTERNAL_SERVER_ERROR;
	}

	http.end();

	return reply;
}

http_reply http_execute::http_post(String ip, String path, JsonObject& jsonObject)
{
	String url = "http://" + ip + ":80" + path;

	HTTPClient http;
	http.begin(url);

	String payload;
	jsonObject.printTo(payload);

	int code = http.POST(payload);

	http_reply reply;

	//Code will be 0 or below during an internal error
	if (code > 0) {
		reply.body = http.getString();
		reply.code = (t_http_codes)code;
	}
	else {
		reply.code = HTTP_CODE_INTERNAL_SERVER_ERROR;
	}

	http.end();

	return reply;
}

http_reply http_execute::http_put(String ip, String path, JsonObject& jsonObject)
{
	String url = "http://" + ip + ":80" + path;

	HTTPClient http;
	http.begin(url);

	String payload;
	jsonObject.printTo(payload);

	int code = http.PUT(payload);

	http_reply reply;

	//Code will be 0 or below during an internal error
	if (code > 0) {
		reply.body = http.getString();
		reply.code = (t_http_codes)code;
	}
	else {
		reply.code = HTTP_CODE_INTERNAL_SERVER_ERROR;
	}

	http.end();

	return reply;
}