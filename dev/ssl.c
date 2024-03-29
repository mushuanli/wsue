#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <ctype.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

//#include "imapfilter.h"
//#include "session.h"


#define TCP_DEFAULTBACK_LOG 15
#define HTTP_RESPON_TIMEOUT 15
//#define FORMAT_HTTPCHECK_REQ(buf,serv,port) snprintf(buf,sizeof(buf)-1,"GET /api/v1/echo/ HTTP/1.1\r\nHost: %s:%d\r\n\r\n",serv,port)
#define FORMAT_HTTPCHECK_REQ(buf,serv,port) snprintf(buf,sizeof(buf)-1,"CONNECT 192.168.1.1 HTTP/1.1\r\nHost: %s:%d\r\n\r\n",serv,port)

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
SSL_CTX *sslctx = NULL;
#else
SSL_CTX *ssl23ctx = NULL;
#ifndef OPENSSL_NO_SSL3_METHOD
SSL_CTX *ssl3ctx = NULL;
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
SSL_CTX *tls1ctx = NULL;
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
SSL_CTX *tls11ctx = NULL;
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
SSL_CTX *tls12ctx = NULL;
#endif
#endif



/* IMAP session. */
typedef struct session {
	int socket;		/* Socket. */
	SSL *sslconn;		/* SSL connection. */
} session;
#define error  printf

static int
open_connection(session *ssn,const char* serv,uint16_t port,const char* sslproto);
static int
close_connection(session *ssn);
static ssize_t
socket_read(session *ssn, char *buf, size_t len, long timeout, int timeoutfail, int *interrupt);
static ssize_t
socket_write(session *ssn, const char *buf, size_t len);

static int
open_secure_connection(session *ssn,const char* serv,const char* sslproto);
static int
close_secure_connection(session *ssn);
static ssize_t
socket_secure_read(session *ssn, char *buf, size_t len);
static ssize_t
socket_secure_write(session *ssn, const char *buf, size_t len);



/*
 * Connect to mail server.
 */
static int
open_connection(session *ssn,const char* serv,uint16_t port,const char* sslproto)
{
	struct addrinfo hints, *res, *ressave;
	int n, sockfd;

        char    portstr[32];
        sprintf(portstr,"%d",portstr);
	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	n = getaddrinfo(serv, portstr, &hints, &res);

	if (n < 0) {
		error("gettaddrinfo; %s\n", gai_strerror(n));
		return -1;
	}

	ressave = res;

	sockfd = -1;

	while (res) {
		sockfd = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);

		if (sockfd >= 0) {
			if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
				break;

			sockfd = -1;
		}
		res = res->ai_next;
	}

	if (ressave)
		freeaddrinfo(ressave);

	if (sockfd == -1) {
		error("error while initiating connection to %s at port %d\n",
		    serv, port);
		return -1;
	}

	ssn->socket = sockfd;

	if (sslproto) {
		if (open_secure_connection(ssn,serv,sslproto) == -1) {
			close_connection(ssn);
			return -1;
		}
	}

	return ssn->socket;
}


/*
 * Initialize SSL/TLS connection.
 */
static int
open_secure_connection(session *ssn,const char* serv,const char* sslproto)
{
	int r, e;
	SSL_CTX *ctx = NULL;

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
	if (sslctx)
		ctx = sslctx;
#else
	if (ssl23ctx)
		ctx = ssl23ctx;

	if (sslproto) {
#ifndef OPENSSL_NO_SSL3_METHOD
		if (ssl3ctx && !strcasecmp(sslproto, "ssl3"))
			ctx = ssl3ctx;
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
		if (tls1ctx && !strcasecmp(sslproto, "tls1"))
			ctx = tls1ctx;
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
		if (tls11ctx && !strcasecmp(sslproto, "tls1.1"))
			ctx = tls11ctx;
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
		if (tls12ctx && !strcasecmp(sslproto, "tls1.2"))
			ctx = tls12ctx;
#endif
	}
#endif

	if (ctx == NULL) {
		error("initiating SSL connection to %s; protocol version "
		      "not supported by current build", serv);
		goto fail;
	}

	if (!(ssn->sslconn = SSL_new(ctx)))
		goto fail;

#if OPENSSL_VERSION_NUMBER >= 0x1000000fL
	r = SSL_set_tlsext_host_name(ssn->sslconn, serv);
	if (r == 0) {
		error("failed setting the Server Name Indication (SNI) to "
		    "%s; %s\n", serv,
		    ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
#endif

	SSL_set_fd(ssn->sslconn, ssn->socket);

	for (;;) {
		if ((r = SSL_connect(ssn->sslconn)) > 0)
			break;

		switch (SSL_get_error(ssn->sslconn, r)) {
		case SSL_ERROR_ZERO_RETURN:
			error("initiating SSL connection to %s; the "
			    "connection has been closed cleanly\n",
			    serv);
			goto fail;
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_WANT_X509_LOOKUP:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			break;
		case SSL_ERROR_SYSCALL:
			e = ERR_get_error();
			if (e == 0 && r == 0)
				error("initiating SSL connection to %s; EOF in "
				    "violation of the protocol\n", serv);
			else if (e == 0 && r == -1)
				error("initiating SSL connection to %s; %s\n",
				    serv, strerror(errno));
			else
				error("initiating SSL connection to %s; %s\n",
				    serv, ERR_error_string(e, NULL));
			goto fail;
		case SSL_ERROR_SSL:
			error("initiating SSL connection to %s; %s\n",
			    serv, ERR_error_string(ERR_get_error(),
			    NULL));
			goto fail;
		default:
			break;
		}
	}
	// TODO: ignore cert if (get_option_boolean("certificates") && get_cert(ssn) == -1)
	//	goto fail;

	return 0;

fail:
	ssn->sslconn = NULL;

	return -1;
}


/*
 * Disconnect from mail server.
 */
static int
close_connection(session *ssn)
{
	int r;

	r = 0;

	close_secure_connection(ssn);

	if (ssn->socket != -1) {
		r = close(ssn->socket);
		ssn->socket = -1;

		if (r == -1)
			error("closing socket; %s\n", strerror(errno));
	}
	return r;
}


/*
 * Shutdown SSL/TLS connection.
 */
static int
close_secure_connection(session *ssn)
{

	if (ssn->sslconn) {
		SSL_shutdown(ssn->sslconn);
		SSL_free(ssn->sslconn);
		ssn->sslconn = NULL;
	}

	return 0;
}



/*
 * Read data from socket.
 */
static ssize_t
socket_read(session *ssn, char *buf, size_t len, long timeout, int timeoutfail, int *interrupt)
{
	int s;
	ssize_t r;
	fd_set fds;

	struct timeval tv;

	struct timeval *tvp;

	r = 0;
	s = 1;
	tvp = NULL;

	memset(buf, 0, len + 1);

	if (timeout > 0) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		tvp = &tv;
	}

	FD_ZERO(&fds);
	FD_SET(ssn->socket, &fds);

	if (ssn->sslconn) {
		if (SSL_pending(ssn->sslconn) > 0) {
			r = socket_secure_read(ssn, buf, len);
			if (r <= 0)
				goto fail;
		} else {
			// TODO: ignore signals if (interrupt != NULL)
			//	catch_user_signals();
			if ((s = select(ssn->socket + 1, &fds, NULL, NULL, tvp)) > 0) {
				//TODO: ignore signales if (interrupt != NULL)
				//	ignore_user_signals();
				if (FD_ISSET(ssn->socket, &fds)) {
					r = socket_secure_read(ssn, buf, len);
					if (r <= 0)
						goto fail;
				}
			}
		}
	} else {
		//TODO: ignore signals if (interrupt != NULL)
		//	catch_user_signals();
		if ((s = select(ssn->socket + 1, &fds, NULL, NULL, tvp)) > 0) {
		//TODO: ignore signals	if (interrupt != NULL)
	//			ignore_user_signals();
			if (FD_ISSET(ssn->socket, &fds)) {
				r = read(ssn->socket, buf, len);
				if (r == -1) {
					error("reading data; %s\n", strerror(errno));
					goto fail;
				} else if (r == 0) {
					goto fail;
				}
			}
		}
	}

	if (s == -1) {
		if (interrupt != NULL && errno == EINTR) {
			*interrupt = 1;
			return -1;
		}
		error("waiting to read from socket; %s\n", strerror(errno));
		goto fail;
	} else if (s == 0 && timeoutfail) {
		error("timeout period expired while waiting to read data\n");
		goto fail;
	}

	return r;
fail:
	close_connection(ssn);

	return -1;

}


/*
 * Read data from a TLS/SSL connection.
 */
static ssize_t
socket_secure_read(session *ssn, char *buf, size_t len)
{
	int r, e;

	for (;;) {
		if ((r = (ssize_t) SSL_read(ssn->sslconn, buf, len)) > 0)
			break;

		switch (SSL_get_error(ssn->sslconn, r)) {
		case SSL_ERROR_ZERO_RETURN:
			error("reading data through SSL; the connection has "
			    "been closed cleanly\n");
			goto fail;
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_WANT_X509_LOOKUP:
			break;
		case SSL_ERROR_SYSCALL:
			e = ERR_get_error();
			if (e == 0 && r == 0)
				error("reading data through SSL; EOF in "
				    "violation of the protocol\n");
			else if (e == 0 && r == -1)
				error("reading data through SSL; %s\n",
				    strerror(errno));
			else
				error("reading data through SSL; %s\n",
				    ERR_error_string(e, NULL));
			goto fail;
		case SSL_ERROR_SSL:
			error("reading data through SSL; %s\n",
			    ERR_error_string(ERR_get_error(), NULL));
			goto fail;
		default:
			break;
		}
	}

	return r;
fail:
	SSL_set_shutdown(ssn->sslconn, SSL_SENT_SHUTDOWN |
	    SSL_RECEIVED_SHUTDOWN);

	return -1;

}


/*
 * Write data to socket.
 */
static ssize_t
socket_write(session *ssn, const char *buf, size_t len)
{
	int s;
	ssize_t r, t;
	fd_set fds;

	r = t = 0;
	s = 1;

	FD_ZERO(&fds);
	FD_SET(ssn->socket, &fds);

	while (len) {
		if ((s = select(ssn->socket + 1, NULL, &fds, NULL, NULL) > 0 &&
		    FD_ISSET(ssn->socket, &fds))) {
			if (ssn->sslconn) {
				r = socket_secure_write(ssn, buf, len);

				if (r <= 0)
					goto fail;
			} else {
				r = write(ssn->socket, buf, len);

				if (r == -1) {
					error("writing data; %s\n",
					    strerror(errno));
					goto fail;
				} else if (r == 0) {
					goto fail;
				}
			}

			if (r > 0) {
				len -= r;
				buf += r;
				t += r;
			}
		}
	}

	if (s == -1) {
		error("waiting to write to socket; %s\n", strerror(errno));
		goto fail;
	} else if (s == 0) {
		error("timeout period expired while waiting to write data\n");
		goto fail;
	}

	return t;
fail:
	close_connection(ssn);

	return -1;
}


/*
 * Write data to a TLS/SSL connection.
 */
static ssize_t
socket_secure_write(session *ssn, const char *buf, size_t len)
{
	int r, e;

	for (;;) {
		if ((r = (ssize_t) SSL_write(ssn->sslconn, buf, len)) > 0)
			break;

		switch (SSL_get_error(ssn->sslconn, r)) {
		case SSL_ERROR_ZERO_RETURN:
			error("writing data through SSL; the connection has "
			    "been closed cleanly\n");
			goto fail;
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
		case SSL_ERROR_WANT_X509_LOOKUP:
			break;
		case SSL_ERROR_SYSCALL:
			e = ERR_get_error();
			if (e == 0 && r == 0)
				error("writing data through SSL; EOF in "
				    "violation of the protocol\n");
			else if (e == 0 && r == -1)
				error("writing data through SSL; %s\n",
				    strerror(errno));
			else
				error("writing data through SSL; %s\n",
				    ERR_error_string(e, NULL));
			goto fail;
		case SSL_ERROR_SSL:
			error("writing data through SSL; %s\n",
			    ERR_error_string(ERR_get_error(), NULL));
			goto fail;
		default:
			break;
		}
	}

	return r;
fail:
	SSL_set_shutdown(ssn->sslconn, SSL_SENT_SHUTDOWN |
	    SSL_RECEIVED_SHUTDOWN);

	return -1;
}


static int TCPCreate(const char *serv,uint16_t port,int isbind)
{
    int 	sockfd	= -1;
    char	portstr[32];
    struct addrinfo hints, *res, *ressave;

    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family 		= PF_UNSPEC;
    hints.ai_socktype		= SOCK_STREAM;
    if( isbind ){
        hints.ai_flags		= AI_PASSIVE;
    }
    else{
        if( !serv || !serv[0] ){
            error( "SockAPI_Create	param error for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
            return -1;
        }
    }

    sprintf(portstr,"%d",port);

    int ret =  getaddrinfo(serv, portstr, &hints, &res);
    if( ret != 0) {
        error( "SockAPI_Create getaddr fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
        return -1;
    }

    ressave = res;

    do {
        struct sockaddr_in* paddr = (struct sockaddr_in*)res->ai_addr;
        if( isbind ){
            if( paddr->sin_addr.s_addr == htonl(0x7f000001))
                continue;
        }
        else{
            if( paddr->sin_addr.s_addr == 0)
                continue;
        }

        sockfd = socket(res->ai_family, res->ai_socktype,
                res->ai_protocol);
        if(sockfd < 0) {
            error( "SockAPI_Create getsock fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);

            continue;
        }

        int on	= 1;
        if( isbind ){
            if( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0 ){
                error( "SockAPI_Create reuse port fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
            }
            if(bind(sockfd, res->ai_addr, res->ai_addrlen) == 0
                    && listen(sockfd, TCP_DEFAULTBACK_LOG) == 0 ) {
                break;
            }
        }
        else{
            if(connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
                break;
            }
            setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on)) ;
        }
        close(sockfd);
        sockfd = -1;
        error( "SockAPI_Create init sock fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
    }while((res = res->ai_next) != NULL);

    freeaddrinfo(ressave);
    if( sockfd >= 0 )
        return sockfd;

    error( "SockAPI_Create fatal create fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
    return -1;
}
int VMISSL_Init(const char* capath, const char* cafile)
{
    SSL_library_init();
    SSL_load_error_strings();
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
    sslctx = SSL_CTX_new(TLS_method());
    if( !sslctx )
        return -1;
#else
    ssl23ctx = SSL_CTX_new(SSLv23_client_method());
    if( !ssl23ctx )
        return -1;
#ifndef OPENSSL_NO_SSL3_METHOD
    ssl3ctx = SSL_CTX_new(SSLv3_client_method());
    if( !ssl3ctx )
        return -1;
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
    tls1ctx = SSL_CTX_new(TLSv1_client_method());
    if( !tls1ctx )
        return -1;
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
    tls11ctx = SSL_CTX_new(TLSv1_1_client_method());
    if( !tls11ctx )
        return -1;
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
    tls12ctx = SSL_CTX_new(TLSv1_2_client_method());
    if( !tls11ctx )
        return -1;
#endif
#endif

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
    if (sslctx)
        SSL_CTX_load_verify_locations(sslctx, cafile, capath);
#else
    if (ssl23ctx)
        SSL_CTX_load_verify_locations(ssl23ctx, cafile, capath);
#ifndef OPENSSL_NO_SSL3_METHOD
    if (ssl3ctx)
        SSL_CTX_load_verify_locations(ssl3ctx, cafile, capath);
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
    if (tls1ctx)
        SSL_CTX_load_verify_locations(tls1ctx, cafile, capath);
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
    if (tls11ctx)
        SSL_CTX_load_verify_locations(tls11ctx, cafile, capath);
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
    if (tls12ctx)
        SSL_CTX_load_verify_locations(tls12ctx, cafile, capath);
#endif
#endif
    return 0;
}

void VMISSL_Release()
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
	if (sslctx)
		SSL_CTX_free(sslctx);
#else
	if (ssl23ctx)
		SSL_CTX_free(ssl23ctx);
#ifndef OPENSSL_NO_SSL3_METHOD
	if (ssl3ctx)
		SSL_CTX_free(ssl3ctx);
#endif
#ifndef OPENSSL_NO_TLS1_METHOD
	if (tls1ctx)
		SSL_CTX_free(tls1ctx);
#endif
#ifndef OPENSSL_NO_TLS1_1_METHOD
	if (tls11ctx)
		SSL_CTX_free(tls11ctx);
#endif
#ifndef OPENSSL_NO_TLS1_2_METHOD
	if (tls12ctx)
		SSL_CTX_free(tls12ctx);
#endif
#endif
}

int VMISSL_Connect(session *ssn,const char* serv,uint16_t port)
{
    memset(ssn,0,sizeof(*ssn));
    ssn->socket = -1;

    ssn->socket = TCPCreate(serv,port,0);
    if( ssn->socket < 0 ) 
        return -1;

    if (open_secure_connection(ssn,serv,"tls1.2") == -1) {
        close_connection(ssn);
        return -1;
    }

    return 0;
    //return open_connection(ssn, serv,port,"tls1.2");
}

void VMISSL_Close(session *ssn)
{
    close_connection(ssn);
}

ssize_t
VMISSL_Read(session *ssn, char *buf, size_t len, long timeout){
    ssize_t ret = 0;
    while( ret == 0 ){
        int interrupt = 0;
        ret = socket_read(ssn, buf, len, timeout,1,&interrupt);
        if( ret == -1 && interrupt )
            ret = 0;
    }

    return ret;
}

int
VMISSL_ReadN(session *ssn, char *buf, size_t len, long timeout){
    int offset  = 0;
    ssize_t ret = 0;
    while( ret == 0 && offset < len ){
        int interrupt = 0;
        ret = socket_read(ssn, buf + offset , len - offset, timeout,1,&interrupt);
        if( ret == -1 && interrupt )
            ret = 0;
        if( ret > 0 )
            offset  += ret;
    }

    return offset < len ? -1 : 0;
}

int
VMISSL_ReadLine(session *ssn, char *buf, size_t len, long timeout){
    int offset  = 0;
    ssize_t ret = 0;
    while( ret == 0 && offset < len ){
        int interrupt = 0;
        ret = socket_read(ssn, buf + offset , len - offset, timeout,1,&interrupt);
        if( ret == -1 && interrupt )
            ret = 0;
        if( ret < 0 ){
            break;
        }
        else if(ret >0 ){
            buf[offset+ret] = 0;
            char* p = strchr(buf,'\n');
            offset  += ret;
            if( p )
                break;
        }
    }

    return ret < 0 ? ret : offset;
}

ssize_t
VMISSL_Write(session *ssn, const char *buf, size_t len){
    return socket_write(ssn, buf, len);
}


int
VMISSL_WriteN(session *ssn, const char *buf, size_t len){
    int offset  = 0;
    while(offset < len ){
        ssize_t ret = socket_write(ssn, buf+offset, len -offset);
        if( ret <= 0 ){
            error("write %d fail");
            break;
        }
        offset  += ret;
    }

    return offset < len ? -1 :0;
}

int VMISSL_HTTPReq(session *ssn,const char* serv,int port,const char* writebuf,size_t writelen,char* readbuf,size_t readlen)
{
    error("will connect to %s:%d\n",serv,port);
    int ret = VMISSL_Connect(ssn,serv,port);
    if( ret != 0 ){
        error("connect to %s:%d failed %d\n",serv,port,ret);
        return -1;
    }

    ret = VMISSL_WriteN(ssn,writebuf,writelen) ;
    error("write to %s:%d %d:%s\n",serv,port,writelen,writebuf);
    if( ret != 0 ){
        VMISSL_Close(ssn);
        error("write to %s:%d failed %d\n",serv,port,ret);
        return -1;
    }

    if( !readbuf || readlen <= 0 )
        return 0;

    ret = VMISSL_ReadLine(ssn,readbuf,readlen,HTTP_RESPON_TIMEOUT);
    if( ret < 0 ){
        VMISSL_Close(ssn);
        error("read http response from %s:%d failed %d\n",serv,port,ret);
        return -1;
    }

    return ret;
}



int VMISSL_CheckAlive(const char* serv,int port)
{
    char        readbuf[128] = "";
    session     ssn;
    FORMAT_HTTPCHECK_REQ(readbuf,serv,port);
    int         ret          = VMISSL_HTTPReq(&ssn,serv,port,readbuf,strlen(readbuf),readbuf,sizeof(readbuf)-1);
    VMISSL_Close(&ssn);

    if( ret <= 0 )
        return -1;

    readbuf[sizeof(readbuf)-1]    = 0;
    char *p = strchr(readbuf,'\n');
    if( !p || p == readbuf || p[-1] !='\r' || memcmp(readbuf,"HTTP/1.1",8) || !isspace(readbuf[8]) ){
        return 0;
    }
    
    p[-1]   = 0;

    int status  = atoi(readbuf+9);
    error("read %d %d:%s from %s:%d\n",status,ret,readbuf,serv,port);
    return status;
}

#if 1 //def TEST_
int VMISSL_Req(const char* serv,int port,const char* writebuf,size_t writelen,char* readbuf,size_t readlen)
{
    session ssn;
    int ret = VMISSL_Connect(&ssn,serv,port);
    if( ret != 0 ){
        error("connect to %s:%d failed %d\n",serv,port,ret);
        return -1;
    }

    if( VMISSL_WriteN(&ssn,writebuf,writelen) == 0 ){
        if( readbuf && readlen > 0 )
        ret = VMISSL_Read(&ssn,readbuf,readlen,15);
        error("read %d ret:%d:%s\n",readlen,ret,readbuf);
    }

    VMISSL_Close(&ssn);
    return ret;
}

int main(int argc, char** argv)
{
    if( argc < 2 ){
        printf("%s servip [servport]\n",argv[0]);
        return 0;
    }

    int port    = 443;
    if( argc > 2 ){
        port    = strtoull(argv[2],NULL,0);
        if( port < 1 )
            port = 443;
    }

    if( VMISSL_Init(NULL,NULL) != 0 ){
        error("init failed\n");
    }
    else{
        printf("init ok\n");
    }
    VMISSL_CheckAlive(argv[1],port);
#if 0
    const char* writestr = "CONNECT 192.168.1.1 HTTP/1.1\r\n\r\n";
    size_t      writelen = strlen(writestr);
    char        readbuf[128] = "";
    int         ret = VMISSL_Req(argv[1],port,writestr,writelen,readbuf,128-1);
    readbuf[127]    = 0;
    printf("ssl to %s:%d ret %d:%s\n",argv[1],port,ret,readbuf);
#endif
    VMISSL_Release(NULL,NULL);
    return 0;
}
#endif
