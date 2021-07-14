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
	if ( lastUrl == "" ) {
		return 0;
	}

	#ifdef _WIN32
    INT rc;
    WSADATA wsaData;

    rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
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

	return currentWebSocket->getReadyState() != easywsclient::WebSocket::CLOSED ? 1 : 0;
}

int easywsclient_sendMessage(const char * fmt, ...) {	
	char buf[1000];     // this should really be sized appropriately
                       // possibly in response to a call to vsnprintf()
    va_list vl;
    va_start(vl, fmt);

    vsnprintf_s( buf, sizeof( buf), fmt, vl);

    va_end( vl);

	std::string message(buf);

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

int easywsclient_sendMessage4000(const char * fmt, ...) {	
	char buf[4000];     // this should really be sized appropriately
                       // possibly in response to a call to vsnprintf()
    va_list vl;
    va_start(vl, fmt);

    vsnprintf_s( buf, sizeof( buf), fmt, vl);

    va_end( vl);

	std::string message(buf);

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