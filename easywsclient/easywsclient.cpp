#include "easywsclient_cpp.hpp"
#include "easywsclient.h"

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <stdarg.h>


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

	std::cout << "\n lastUrl = " << lastUrl;

	std::size_t found = lastUrl.find("--ipc-socket-uri=");
	if (found != std::string::npos) {
		lastUrl.erase(0, 17);//"--ipc-socket-uri=".length
	}

	std::cout << "\n lastUrl = " << lastUrl;
}

void easywsclient_setPrefix(const char * prefix) {
	serverPrefix = std::string(prefix);

	std::size_t found = serverPrefix.find("--ipc-prefix=");
	if (found != std::string::npos) {
		serverPrefix.erase(0, 13);//"--ipc-prefix=".length
	}
}

int easywsclient_createServer() {
    doLog("easywsclient_createServer | ENTER");
    
    if ( lastUrl == "" ) {
        doLog("easywsclient_createServer | FAST EXIT");
        return 0;
	}

	#ifdef _WIN32
    INT rc;
    WSADATA wsaData;

    doLog("easywsclient_createServer | L1");
    rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    doLog("easywsclient_createServer | L2 | rc: " + std::to_string(rc));
    if (rc) {
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
    doLog("easywsclient_sendMessage | ENTER");

    int res_len = 0;
    va_list vl;
    va_start(vl, fmt);

    res_len = vsnprintf_s(&message[0], message.size(), message.size() - 1, fmt, vl);

    va_end(vl);

    doLog("easywsclient_sendMessage | L1 | vsnprintf_s: " + std::to_string(res_len));
    if (res_len > message.size()) {
        doLog("WARNING !!!!!!!! easywsclient_sendMessage | L1.1 | res_len > message.size");
    }

    if (res_len > 0)
        message.resize(res_len);

    doLog("easywsclient_sendMessage | L2 | message: " + message);
    
    //std::string message(buf);

	//std::cout << "\n ---- try to send " << message << " ----- \n";

	if ( currentWebSocket != NULL ) {
		if ( currentWebSocket->getReadyState() == easywsclient::WebSocket::CLOSED && lastUrl != "" ) {
            doLog("easywsclient_sendMessage | L3 | re-create Server");
            easywsclient_createServer();
            doLog("easywsclient_sendMessage | L4 | Server re-created");
		}

        doLog("easywsclient_sendMessage | L5");
        currentWebSocket->poll();
        doLog("easywsclient_sendMessage | L6");
        currentWebSocket->dispatch([](const std::string & smessage) {
			std::cout << "\nreceiving " << smessage;
            doLog("easywsclient_sendMessage | L6.1 msg: " + smessage);
		});

        doLog("easywsclient_sendMessage | L7 | serverPrefix + message: " + serverPrefix + message);
        currentWebSocket->send(serverPrefix + message);
        doLog("easywsclient_sendMessage | L8");

		currentWebSocket->poll();
        doLog("easywsclient_sendMessage | L9");
        currentWebSocket->dispatch([](const std::string & smessage) {
			std::cout << "\nreceiving " << smessage;
            doLog("easywsclient_sendMessage | L9.1 msg: " + smessage);
        });

        doLog("easywsclient_sendMessage | L10");
        message.clear();

        doLog("easywsclient_sendMessage | EXIT 1");
        //std::cout << "\n ---- SENDED " << " ----- \n";
		return 1;
	}

	message.clear();

	//std::cout << "\n ---- NOT SS " << " ----- \n";
    doLog("easywsclient_sendMessage | EXIT 0");
    return 0;
}

int easywsclient_sendMessage4000(const char * fmt, ...) {	
	/*
    char buf[4000];     // this should really be sized appropriately
                       // possibly in response to a call to vsnprintf()
    */
    doLog("easywsclient_sendMessage4000 | ENTER");

    std::string message(4096, 0);

    int res_len = 0;
    va_list vl;
    va_start(vl, fmt);

    res_len = vsnprintf_s(&message[0], message.size(), message.size() - 1, fmt, vl);

    va_end(vl);

    doLog("easywsclient_sendMessage4000 | L1 | vsnprintf_s: " + std::to_string(res_len));

    if (res_len > 0)
        message.resize(res_len);

    //std::string message(buf);

    //std::cout << "\n ---- try to send " << message << " ----- \n";

    doLog("easywsclient_sendMessage4000 | L2 | message: " + message);
    
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
        doLog("easywsclient_sendMessage4000 | EXIT 1");
        return 1;
	}

	message.clear();

	//std::cout << "\n ---- NOT SS " << " ----- \n";
    doLog("easywsclient_sendMessage4000 | EXIT 0");

    return 0;
}

#ifdef __cplusplus
}
#endif
