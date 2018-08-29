#pragma once

#include <ESP8266HTTPClient.h>

typedef struct http_reply {
	t_http_codes code = HTTP_CODE_NOT_FOUND;
	String body = "";
};

class http_execute
{
public:
	http_reply http_get(String ip, String path);
	http_reply http_post(String ip, String path, JsonObject& jsonObject);
	http_reply http_put(String ip, String path, JsonObject& jsonObject);
};

