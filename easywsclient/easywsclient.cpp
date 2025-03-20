#define __STDC_WANT_LIB_EXT1__ 1

#include "easywsclient_cpp.hpp"
#include "easywsclient.h"

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <stdarg.h>
#include <cstring>


#ifdef _WIN32
#pragma comment( lib, "ws2_32" )
#include <WinSock2.h>
#endif

easywsclient::WebSocket::pointer currentWebSocket = NULL;

#ifdef __cplusplus
extern "C" {
#endif

std::string lastUrl;
std::string serverPrefix;
unsigned int counterBeforePing;

static log_func_cb *g_log_func = nullptr;

void doLog(const std::string& log_data)
{
    if (g_log_func)
        (*g_log_func)(log_data.c_str());
}


void easywsclient_setLogCB(log_func_cb* log_cb)
{
    g_log_func = log_cb;
}

int easywsclient_startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? -1 : strncmp(pre, str, lenpre);
}

void easywsclient_setUrl(const char * uri) {
	lastUrl = std::string(uri);

	std::size_t found = lastUrl.find("--ipc-socket-uri=");
	if (found != std::string::npos) {
		lastUrl.erase(found, 17);//"--ipc-socket-uri=".length
	}
}

void easywsclient_setPrefix(const char * prefix) {
	serverPrefix = std::string(prefix);

	std::size_t found = serverPrefix.find("--ipc-prefix=");
	if (found != std::string::npos) {
		serverPrefix.erase(found, 13);//"--ipc-prefix=".length
	}
}

int easywsclient_createServer() {
    doLog("easywsclient_createServer | ENTER | lastUrl: " + lastUrl);
    
    if ( lastUrl == "" ) {
        return 0;
	}

	#ifdef _WIN32
    INT rc;
    WSADATA wsaData;

    rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc) {
        doLog("easywsclient_createServer | L1 | rc: " + std::to_string(rc));
        return 0;
    }
	#endif

	if ( currentWebSocket != NULL ) {
		currentWebSocket->close();
	}

	currentWebSocket = easywsclient::WebSocket::from_url(lastUrl);

	// Отправляем приветственное сообщение с сессией
	currentWebSocket->send(serverPrefix + "ready");

	if ( currentWebSocket->getReadyState() == easywsclient::WebSocket::CLOSED ) {
		//std::cout << "\n" << "error while message sending";
		return 0;
	}
	
	//std::cout << "\nreceiving...";

	counterBeforePing++;

	// TODO:: Это сейчас работать не будет, потому что должно вызываться из main-функции в бесконечном цикле
	if (counterBeforePing != 0 && counterBeforePing % 600 == 0) {// Каждые 30 секунд, (poll(50ms) * 600) = 30sec
		counterBeforePing = 0;
		// Отправляем PING
		currentWebSocket->send(serverPrefix + "ping");
	}

	currentWebSocket->poll(50);// 50 ms
	currentWebSocket->dispatch([](const std::string & smessage) {
		//std::cout << "\nreceiving " << smessage;
	});

    doLog("easywsclient_createServer | EXIT");
    
    return currentWebSocket->getReadyState() != easywsclient::WebSocket::CLOSED ? 1 : 0;
}

int easywsclient_sendMessage(const char * fmt, ...) {	
	/*
    char buf[1000];     // this should really be sized appropriately
                       // possibly in response to a call to vsnprintf()
    */

    std::string message(4096, 0);

    int res_len = 0;
    va_list vl;
    va_start(vl, fmt);

	#ifdef _WIN32
    res_len = vsnprintf_s(&message[0], message.size(), message.size() - 1, fmt, vl);
    #else
    res_len = vsnprintf(&message[0], message.size(), fmt, vl);
    #endif

    va_end(vl);

    if (res_len > 0)
        message.resize(res_len);

    doLog("easywsclient_sendMessage | L1 | message: " + message);
    
    //std::string message(buf);

	//std::cout << "\n ---- try to send " << message << " ----- \n";

	if ( currentWebSocket != NULL ) {
		if ( currentWebSocket->getReadyState() == easywsclient::WebSocket::CLOSED && lastUrl != "" ) {
            doLog("easywsclient_sendMessage | L3 | re-create Server");
            easywsclient_createServer();
            doLog("easywsclient_sendMessage | L4 | Server re-created");
		}

		currentWebSocket->poll();
		currentWebSocket->dispatch([](const std::string & smessage) {
			std::cout << "\nreceiving " << smessage;
		});

		currentWebSocket->send(serverPrefix + message);

		currentWebSocket->poll();
		currentWebSocket->dispatch([](const std::string & smessage) {
			std::cout << "\nreceiving " << smessage;
		});

        doLog("easywsclient_sendMessage | EXIT | 1");

		//std::cout << "\n ---- SENDED " << " ----- \n";
		return 1;
	}

	//std::cout << "\n ---- NOT SS " << " ----- \n";
    doLog("easywsclient_sendMessage | EXIT | 0");
    return 0;
}

int easywsclient_sendMessage4000(const char * fmt, ...) {	
	/*
    char buf[4000];     // this should really be sized appropriately
                       // possibly in response to a call to vsnprintf()
    */
    std::string message(4096, 0);

    int res_len = 0;
    va_list vl;
    va_start(vl, fmt);

	#ifdef _WIN32
    res_len = vsnprintf_s(&message[0], message.size(), message.size() - 1, fmt, vl);
    #else
    res_len = vsnprintf(&message[0], message.size(), fmt, vl);
    #endif

    va_end(vl);

    if (res_len > 0)
        message.resize(res_len);

    //std::string message(buf);

	//std::cout << "\n ---- try to send " << message << " ----- \n";

	if ( currentWebSocket != NULL ) {
		if ( currentWebSocket->getReadyState() == easywsclient::WebSocket::CLOSED && lastUrl != "" ) {
			easywsclient_createServer();
		}

		currentWebSocket->poll();
		currentWebSocket->dispatch([](const std::string & smessage) {
			std::cout << "\nreceiving " << smessage;
		});

		currentWebSocket->send(serverPrefix + message);

		currentWebSocket->poll();
		currentWebSocket->dispatch([](const std::string & smessage) {
			std::cout << "\nreceiving " << smessage;
		});

		message.clear();

		//std::cout << "\n ---- SENDED " << " ----- \n";
		return 1;
	}

	message.clear();

	//std::cout << "\n ---- NOT SS " << " ----- \n";
	return 0;
}

#ifdef __cplusplus
}
#endif