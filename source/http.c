#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/libssl.h>
#include <psp2/io/fcntl.h>

#include "common.h"
#include "saves.h"

#define HTTP_SUCCESS 	1
#define HTTP_FAILED	 	0
#define HTTP_USER_AGENT "Mozilla/5.0 (PLAYSTATION VITA; 1.00)"

#define NET_POOLSIZE 	(16 * 1024)


static int skipSslCallback(unsigned int verifyErr, void * const sslCert[], int certNum, void *userArg)
{
	LOG("sslCtx=%x (%X)", certNum, verifyErr);
	return HTTP_SUCCESS;
}

int http_init()
{
	int ret;
	SceNetInitParam netInitParam;

	if(sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS) < 0)
		return HTTP_FAILED;

	LOG("sceNet init");
	netInitParam.memory = malloc(NET_POOLSIZE);
	netInitParam.size = NET_POOLSIZE;
	netInitParam.flags = 0;

	ret = sceNetInit(&netInitParam);
	if (ret < 0 && ret != SCE_NET_ERROR_EBUSY) {
		LOG("sceNetInit() error: 0x%08X\n", ret);
		return HTTP_FAILED;
	}

	ret = sceNetCtlInit();
	if (ret < 0 && ret != 0x80412102) {
		LOG("sceNetCtlInit() error: 0x%08X\n", ret);
		return HTTP_FAILED;
	}

	ret = sceSslInit(512*1024);
	if (ret < 0) {
		LOG("sceSslInit() error: 0x%08X\n", ret);
		return HTTP_FAILED;
	}

	ret = sceHttpInit(64*1024);
	if (ret < 0) {
		LOG("sceHttpInit() error: 0x%08X\n", ret);
		return HTTP_FAILED;
	}

	return HTTP_SUCCESS;
}

int http_download(const char* url, const char* filename, const char* local_dst, int show_progress)
{
	int ret, tpl = 0, conn = 0, req = 0;
	int http_res = HTTP_FAILED;
	int contentLengthType;
	uint64_t contentLength;
	int32_t statusCode;
	char full_url[1024];

	snprintf(full_url, sizeof(full_url), "%s%s", url, filename);

	tpl = sceHttpCreateTemplate(HTTP_USER_AGENT, SCE_HTTP_VERSION_1_1, 1);
	if (tpl < 0) {
		LOG("sceHttpCreateTemplate() error: 0x%08X\n", tpl);
		return HTTP_FAILED;
	}

	ret = sceHttpsSetSslCallback(tpl, skipSslCallback, NULL);
	if (ret < 0) {
		LOG("sceHttpsSetSslCallback() error: 0x%08X\n", ret);
	}

	conn = sceHttpCreateConnectionWithURL(tpl, full_url, 1);
	if (conn < 0) {
		LOG("sceHttpCreateConnectionWithURL() error: 0x%08X\n", conn);
		goto close_http;
	}

	req = sceHttpCreateRequestWithURL(conn, SCE_HTTP_METHOD_GET, full_url, 0);
	if (req < 0) {
		LOG("sceHttpCreateRequestWithURL() error: 0x%08X\n", req);
		goto close_http;
	}

	LOG("Sending Request to '%s'\n", full_url);
	ret = sceHttpSendRequest(req, NULL, 0);
	if (ret < 0) {
		LOG("sceHttpSendRequest (%X)", ret);
		goto close_http;
	}

	ret = sceHttpGetStatusCode(req, &statusCode);
	if (ret < 0) {
		LOG("sceHttpGetStatusCode (%X)", ret);
		goto close_http;
	}

	ret = sceHttpGetResponseContentLength(req, &contentLength);
	if (ret < 0) {
		LOG("sceHttpGetContentLength() error: 0x%08X\n", ret);
		//goto close_http;
	}
	else LOG("Content-Length = %lld\n", contentLength);

	switch (statusCode)
	{
		case 200:	// OK
		case 203:	// Non-Authoritative Information
		case 206:	// Partial Content
		case 301:	// Moved Permanently
		case 302:	// Found
		case 307:	// Temporary Redirect
		case 308:	// Permanent Redirect
			LOG("HTTP Response (%d)", statusCode);
			break;
		
		default:
			LOG("HTTP Error (%d)", statusCode);
			goto close_http;
	}

	uint8_t dl_buf[64 * 1024];
	uint64_t total_read = 0;
	FILE* fd = fopen(local_dst, "wb");

	if (!fd) {
		LOG("fopen Error: File path '%s'", local_dst);
		goto close_http;
	}

	if (show_progress)
		init_progress_bar("Downloading...");

	while (1) {
		int read = sceHttpReadData(req, dl_buf, sizeof(dl_buf));
		if (read < 0)
		{
			LOG("HTTP read error! (0x%08X)", read);
			break;
		}

		if (read == 0)
		{
			http_res = HTTP_SUCCESS;
			break;
		}

		ret = fwrite(dl_buf, 1, read, fd);
		if (ret < 0 || ret != read)
		{
			LOG("File write error! (%d)", ret);
			break;
		}

		total_read += read;

		if (show_progress)
			update_progress_bar(total_read, contentLength, "Downloading...");

		LOG("Downloaded %ld/%ld\n", total_read, contentLength);
	}

	fclose(fd);

	if (show_progress)
		end_progress_bar();

close_http:
	if (req > 0) {
		ret = sceHttpDeleteRequest(req);
		if (ret < 0) {
			LOG("sceHttpDeleteRequest() error: 0x%08X\n", ret);
		}
	}

	if (conn > 0) {
		ret = sceHttpDeleteConnection(conn);
		if (ret < 0) {
			LOG("sceHttpDeleteConnection() error: 0x%08X\n", ret);
		}
	}

	if (tpl > 0) {
		ret = sceHttpDeleteTemplate(tpl);
		if (ret < 0) {
			LOG("sceHttpDeleteTemplate() error: 0x%08X\n", ret);
		}
	}

	return (http_res);
}

void http_end(void)
{
	sceHttpTerm();
	sceSslTerm();
	sceNetCtlTerm();
	sceNetTerm();
}
