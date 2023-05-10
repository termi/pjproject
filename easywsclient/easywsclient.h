#ifndef EASYWSCLIENT_H
#define EASYWSCLIENT_H

#ifdef __cplusplus
extern "C" {
#endif
		
    typedef void log_func_cb(const char *data);

    void easywsclient_setLogCB(log_func_cb* log_cb);

    int easywsclient_startsWith(const char *, const char *);
//__declspec( dllimport )
	void easywsclient_setUrl(const char *);
//__declspec( dllimport )
	void easywsclient_setPrefix(const char *);
//__declspec( dllimport )
	int easywsclient_createServer();
//__declspec( dllimport )
	int easywsclient_sendMessage(const char *, ...);
//__declspec( dllimport )
	int easywsclient_sendMessage4000(const char *, ...);

#ifdef __cplusplus
}
#endif

#endif /* EASYWSCLIENT_H */