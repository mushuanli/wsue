#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <string>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

//#include "net/base/ip_endpoint.h"

#define WRAP_SYSAPI(ret,func)           do{ \
    ret = func;                             \
}while( ret == -1 && ( errno == EINTR))

#define WRAP_SYSAPI_INTR(ret,func)           do{ \
    ret = func; 							\
}while( ret == -1 &&  errno == EINTR)
#define WRAP_CLOSE_FD(fd) do{ \
    if( (fd) != -1 ){ close(fd); (fd) = -1; } }while(0)
#define WRAP_WAIT_THRID(thrid) do{ \
    if( (thrid) != 0 ){ void* p = NULL; pthread_join(thrid,&p); (thrid) = 0; } }while(0)

#define POLLIN_FLAG                (POLLIN|POLLHUP|POLLERR)
#define SET_POLLIN_FLAG(pollfds,fileid)    { (pollfds).fd	= fileid; (pollfds).events = POLLIN_FLAG; }
#define IS_POLLIN_OK(revent)  (((revent) & POLLIN) && (!((revent) & (POLLERR|POLLHUP|POLLNVAL))) )
#define TCP_DEFAULTBACK_LOG    5


#define RMXQUIC_ERRCODE_CONNFAIL            -1
#define RMXQUIC_ERRCODE_LISTENFAIL          -2
#define RMXQUIC_ERRCODE_EPOLLFAIL           -3
#define RMXQUIC_ERRCODE_EPOLLTIMEOUT        -4
#define RMXQUIC_ERRCODE_EPOLLUSRSTOP        -5
#define RMXQUIC_ERRCODE_NOTCONNECT          -6
#define RMXQUIC_ERRCODE_NOTSUPPORT          -7
#define RMXQUIC_ERRCODE_INITFAIL            -8
#define RMXQUIC_ERRCODE_NOMEM               -9
#define RMXQUIC_ERRCODE_BUFSIZE             -10      //  buf size too small


#define RMXTRACE_DEFAULT_FLAG       (  RMXTRACE_FLAG_DEBUG|RMXTRACE_FLAG_INFO | RMXTRACE_FLAG_ERROR )


typedef void* (*RMXQUIC_TASK_ROUTINE)(void *);

namespace net{

    class QuicClock;
    class IPEndPoint;

    bool NetComm_Init();
    int NetComm_IPInfo2Addr(IPEndPoint *server_address, const char *hostname,int port,bool isserv);
    void NetComm_DumpHex(const unsigned char* content, int contentlen);

    bool NetComm_WriteN(int fd,char* p, size_t size);
    bool NetComm_ReadN(int fd,char* p,size_t size,int intervaltimeout_ms);

    int NetComm_InitUDP(IPEndPoint *server_address,IPEndPoint* local_address,const char *hostname,uint16_t port,bool isserv);
    int NetComm_UDPSend(int sd,const char *buf,int len,const IPEndPoint *server_address);
    int NetComm_UDPRecv(int sd,char* buf,size_t maxsz,IPEndPoint *peer_address);

    void NetComm_CloseSocket(int sd);
    int RMXCQuic_TCPAccept(int ld,sockaddr_storage* dst);
    bool NetComm_TCPGetLocalPort(int sd,uint16_t& port);
    int NetComm_TCPCreate(const char *serv,uint16_t port,bool isbind,int connecttimeout_ms);
    bool NetComm_RecvN(int sd,char* p, size_t size,int timeout_ms,uint16_t interval_wait_ms);
    bool NetComm_SendN(int sd,const char* p, size_t size);
    bool NetComm_Poll(int sd,int timeout_ms);
    bool NetComm_PollEx(int sd,int timeout_ms,bool& haserror);

    bool NetComm_StartTask(pthread_t *thrid,void* (*start_routine)(void*),void *arg);

    bool NetComm_RSAGenKey(std::string& priv,std::string& pub);
    int NetComm_RSA256Hash(unsigned char hash[32],const char *string, int len);
    int NetComm_RSASign(std::string &signature,const char *data, int dataLen,const char *privkey, int privkeylen);
    int NetComm_RSAVerify(const std::string &signature,const char *data, int dataLen,const char *pubkey, int pubkeylen);

#if( RMXUDP_SSLMODE == 0 )
	int NetComm_GCMEncrypt(const unsigned char *plaintext, int plaintext_len, const unsigned char *aad,
		int aad_len, const unsigned char *key,const unsigned char *iv,int iv_len,
		unsigned char *ciphertext,int taglen);
	int NetComm_GCMDecrypt(const unsigned char *ciphertext, int ciphertext_len, const unsigned char *aad,
		int aad_len,const unsigned char *key, const unsigned char *iv,int iv_len,
		unsigned char *plaintext,int taglen);
#endif	
    //int NetComm_QuicConnect(int isipv6,const char *addr,int port);
}

/*  a  quic connect include:
 *          task runner
 *          proof verifiers ->  GoProofVerifier::VerifyProof
 *                              ProofVerifyJobVerifyProof_C
 *                                  -> new job, verify proof 
 *                              
 *          client writer
 */

#include <time.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h>  
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/types.h>
//#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
//#include <net/if.h>

#ifdef ANDROID
#include <unwind.h>
#else
#include <execinfo.h>

#ifdef __cplusplus
#include <execinfo.h>
#include <cxxabi.h>
#endif
#endif

#include "openssl/sha.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/ssl.h"

#include "rmxudp_common.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_clock.h"
#include "rmxquic/rmxquic_logging.h"



#ifndef TRACE_DETAIL
#define TRACE_UDP(cont,sz,addr,issnd,ret)
#else
    static int DecQuichead(const uint8_t *content, int len,const sockaddr_in *addr,int issend,int ret);
#define TRACE_UDP(cont,sz,addr,issnd,ret)   do{  \
	if(g_RmxQuicTraceFlag & RMXTRACE_FLAG_IO) DecQuichead((const uint8_t *)cont,sz,(struct sockaddr_in *)addr,issnd,ret); \
}while(0)
#endif

namespace net{


#if( RMXUDP_SSLMODE == 0 ) && ((defined __APPLE__) || (!defined RMXUDPLIBMODE))
    static void ExitCleanOpenSSL()
    {
        ERR_free_strings();
        EVP_cleanup();
    }
#endif

    bool NetComm_Init()
    {
#if( RMXUDP_SSLMODE == 0 ) && ((defined __APPLE__) || (!defined RMXUDPLIBMODE))
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        ERR_load_BIO_strings();
        ERR_load_crypto_strings();
        SSL_load_error_strings();
        atexit(ExitCleanOpenSSL);
#endif
        struct sigaction sa;
        sa.sa_handler = SIG_IGN;
        sigaction( SIGPIPE, &sa, 0 );


        return true;
    }


    void NetComm_CloseSocket(int sd)
    {
#ifdef _WIN32_
        closesocket(sd);
#else
        close(sd);
#endif
    }
	
#if 0
    bool NetComm_GetEthInfo(std::string& name,std::string& ipaddr,std::string& macaddr)
    {
        char    addrBuf[128]    = "";
        bool    ret             = false;
        struct ifaddrs *ifa_list= 0, *ifa;

        if (getifaddrs(&ifa_list)== -1) {
            return false;
        }

        int32_t sd = socket( PF_INET, SOCK_DGRAM, 0 );
        if( sd < 0 )
        {
            /// free memory allocated by getifaddrs
            freeifaddrs( ifa_list );
            return false;
        }

        for (ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr )
                continue;

            /// print MAC address
            struct ifreq req;
            strcpy( req.ifr_name, ifa->ifa_name );
            if( ioctl( sd, SIOCGIFHWADDR, &req ) != -1 ){
                uint8_t* mac = (uint8_t*)req.ifr_ifru.ifru_hwaddr.sa_data;
                char    macbuf[32]      = "";
                sprintf(macbuf, "%02X:%02X:%02X:%02X:%02X:%02X",
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
                macaddr = macbuf;
            }

            if(ifa->ifa_addr->sa_family == AF_INET) {
                const struct sockaddr_in *addr = (const struct sockaddr_in*)ifa->ifa_addr;
                if(inet_ntop(AF_INET, &addr->sin_addr, addrBuf, 127)) {
                    ret = true;
                }
            } 
            else {
                const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6*)ifa->ifa_addr;
                if(inet_ntop(AF_INET6, &addr6->sin6_addr, addrBuf, 127)) {
                    ret = true;
                }
            }
            
            if( ret ){
                if( ifa->ifa_name)
                    name = ifa->ifa_name;
                addrBuf[127]	= 0;
                ipaddr	= addrBuf;
                if( strcmp(ifa->ifa_name,"lo") )
                    break;
            }
        }

        freeifaddrs(ifa_list);
        NetComm_CloseSocket(sd);
        return ret;
    }
#endif
    bool NetComm_WriteN(int fd,const char *buf,size_t len)
   {
        int ret = -1;
        while( len > 0 ){
            WRAP_SYSAPI(ret,write(fd,buf,len));

            if( ret <= 0 ){
                return false;
            }

            len -= ret;
            buf += ret;
        }

        return true;
    }

    bool NetComm_ReadN(int fd,char* p,size_t size,int intervaltimeout_ms)
    {
        size_t rest = size;

        while( rest > 0 ){
            int ret;

            WRAP_SYSAPI( ret , read(fd,p,rest) );

            if( ret <= 0 ){
                return false;
            }

            rest    -= ret;
            p	    += ret;
            if( !NetComm_Poll(fd,intervaltimeout_ms) )
                return false;
        }

        return true;
    }
	
    int NetComm_InitUDP(IPEndPoint *server_address,IPEndPoint* local_address,const char *hostname,uint16_t port,bool isserv){
        char	service[32];
        struct addrinfo hints, *res, *ressave;
        int n;

        int sd      = -1;
        int retval  = -1;
        IPEndPoint convaddr;

        //  server input local name and port, expect output local_address
        //  client input server's name and port, expect output server_address and local_address
        if( !local_address )
            return -1;

        if( isserv ){
            if( port == 0 )
                return -1;

            if( !hostname )
                hostname    = "0.0.0.0";
        }
        else{
            if( !hostname || !hostname[0] || port == 0 || !server_address )
                return -1;
        }

        sprintf(service,"%d",port);

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags  = AI_ADDRCONFIG;


        n = getaddrinfo(hostname, service, &hints, &res);

        if (n <0 || !res) {
            fprintf(stderr,
                    "getaddrinfo error:: [%s]\n",
                    gai_strerror(n));
            return -1;
        }

        ressave = res;

        do{
            // check address
            retval = convaddr.FromSockAddr(res->ai_addr,res->ai_addrlen) ? 0 : -1;
            if( retval != 0){
                continue;
            }

            int family  = convaddr.GetFamily();
            
            if( convaddr.address() == IPAddress::IPv4Localhost()
                    || convaddr.address() == IPAddress::IPv6Localhost() ){
                if( isserv || family == ADDRESS_FAMILY_IPV6 )
                    continue;
            }
            else if( convaddr.address().IsZero() ){
                if( !isserv )
                    continue;
            }

            //  check if socket can success create
            sd  = socket( family == ADDRESS_FAMILY_IPV6 ? AF_INET6 : AF_INET,SOCK_DGRAM,0);
            if( sd == -1 )
                continue;

            struct sockaddr_storage	addr;
            socklen_t               addrlen = sizeof(addr);
            if( isserv ){
                retval = convaddr.ToSockAddr((struct sockaddr*)&addr,&addrlen) ? 0 : -1;
                if( retval == 0 )
                    retval = bind(sd,(sockaddr *)&addr,addrlen);
            }
            else{
                IPEndPoint  tmpaddr(convaddr.address(),22);

                retval = tmpaddr.ToSockAddr((struct sockaddr*)&addr,&addrlen)? 0 : -1;
                if( retval == 0 ){
					char c = 'c';
                    retval = sendto(sd,&c,1,0,(struct sockaddr *)&addr,addrlen) == 1 ? 0 : -1;
                }
                if( retval >= 0 ){
                    struct sockaddr_storage laddr;
                    socklen_t address_length   = sizeof(laddr);

                    retval = getsockname(sd,(sockaddr *)&laddr,&address_length);

                    if( retval == 0  ){
                        if( !local_address->FromSockAddr((sockaddr *)&laddr,address_length) ){
                            retval = -1;
                        }
                    }
                }
            }

            if( retval == 0 ){
                break;
            }

            NetComm_CloseSocket(sd);
            sd  = -1;
        }while((res = res->ai_next) != NULL);

        freeaddrinfo(ressave);

        if( retval == 0 ){
            if( isserv ){
                *local_address  = convaddr;
            }
            else{
                *server_address  = convaddr;
            }

            return sd;
        }
        else{
            return -1;
        }
    }




    int NetComm_UDPSend(int sd,const char *buf,int len,const IPEndPoint *peer_address)
    {
        if( peer_address ){
            sockaddr_storage   dst;
            socklen_t          dstlen  = sizeof(dst);
            if(! peer_address->ToSockAddr((sockaddr *)&dst, &dstlen) ){
                RMXQUIC_TRACE_ERROR("serv send fail, addr invalid\n");
                return -1;
            }
            int ret;
            WRAP_SYSAPI(ret,sendto(sd,buf,len,0,(struct sockaddr *)&dst,dstlen));
			//RMXQUIC_TRACE_FATAL("send %d len %d ret:%d\n",sd,len,ret);

            TRACE_UDP(buf,len,&dst,1,ret);
            return ret;
        }
        else{
            int ret;
            WRAP_SYSAPI(ret, send(sd,buf,len,0));
            TRACE_UDP(buf,len,NULL,1,ret);
            return ret;
        }
    }


    bool NetComm_Poll(int sd,int timeout_ms)
    {
        bool    haserror        = false;
        return NetComm_PollEx(sd,timeout_ms,haserror);
    }

    bool NetComm_PollEx(int sd,int timeout_ms,bool& haserror)
    {
        int ret = -1;
        struct pollfd pfds	 = {sd,POLLIN_FLAG,0};
        WRAP_SYSAPI( ret , poll(&pfds,1,timeout_ms));
        if( ret == 1 && IS_POLLIN_OK(pfds.revents ) ){
            haserror    = false;
            return true;
        }

        haserror    = (ret != 0 );  // if not timeout, consider occur error
        return false;
    }

    int NetComm_UDPRecv(int sd,char* buf,size_t maxsz,IPEndPoint *peer_address)
    {
        if( peer_address ){
            sockaddr_storage   dst;
            socklen_t          dstlen  = sizeof(dst);
            int ret;
            WRAP_SYSAPI(ret , recvfrom(sd,buf,maxsz,0,(struct sockaddr *)&dst,&dstlen));
			//RMXQUIC_TRACE_FATAL("recv %d len %d \n",sd,ret);
            if(! peer_address->FromSockAddr((sockaddr *)&dst, dstlen) ){
                RMXQUIC_TRACE_ERROR("serv recv %d fail, addr invalid\n",ret);
                return -1;
            }

            TRACE_UDP(buf,ret,&dst,0,ret);
            return ret;
        }
        else{
            int ret;
            WRAP_SYSAPI(ret , recv(sd,buf,maxsz,0));
            TRACE_UDP(buf,ret,NULL,0,ret);
            return ret;
        }
    }

    static int CreateTCP(struct addrinfo *pinfo,
            const struct sockaddr *addr,socklen_t addrlen,
            bool isbind,int connecttimeout_ms)
    {
        int sockfd = -1;
        if( pinfo ){
            sockfd = socket(pinfo->ai_family, pinfo->ai_socktype,
                    pinfo->ai_protocol);
            addr    = pinfo->ai_addr;
            addrlen = pinfo->ai_addrlen;
        }
        else{
            sockfd = socket(AF_INET, SOCK_STREAM,0);
        }

        if(sockfd < 0) {
            return -1;
        }

        int on	= 1;
        if( isbind ){
            if( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0 ){
                RMXQUIC_TRACE_WARNING( "tcp reuse port fail\n");
            }
            if(bind(sockfd, addr, addrlen) == 0
                    && listen(sockfd, TCP_DEFAULTBACK_LOG) == 0 ) {
                return sockfd;
            }

            NetComm_CloseSocket(sockfd);
            return -1;        
        }

        int sock_opt    = 1;
        if( connecttimeout_ms != -1){
            ioctl(sockfd, FIONBIO, &sock_opt);
        }

        int connret = connect(sockfd, addr, addrlen);

        if( connret == -1 && connecttimeout_ms != -1
                && (errno == EWOULDBLOCK || errno == EINPROGRESS) ){
            pollfd pfd;
            pfd.fd      = sockfd;
            pfd.events  = POLLOUT;
            int error   = 0;
            socklen_t errorlen = sizeof(error);

            int ret = -1;
            WRAP_SYSAPI(ret , poll( &pfd, 1, connecttimeout_ms ));
            if(  ret == 1
                    && getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&error, &errorlen) == 0 
                    && error == 0 ){
                connret = 0;
            }
        }

        if( connret == 0) {
            if( connecttimeout_ms != -1){
                sock_opt    = 0;
                ioctl(sockfd, FIONBIO, &sock_opt);
            }

            setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on)) ;
            return sockfd;
        }

        NetComm_CloseSocket(sockfd);
        return -1;
    }


    int RMXCQuic_TCPAccept(int ld,sockaddr_storage* dst)
    {
        if( ld == -1 )
            return -1;

        sockaddr_storage tmp;
        if( !dst ) dst = &tmp;

        int sd  = -1;
        while(1){
            socklen_t len = sizeof(tmp);
            sd = accept(ld,(struct sockaddr *)dst,&len);
            if( sd >= 0 )
                break;

            if( sd == -1 ){
                if( errno == EINTR )
                    continue;
                else
                    return -1;
            }
        }

        int flag = 1;
        int ret = setsockopt( sd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) );
        if (ret == -1) {
            RMXQUIC_TRACE_WARNING("Couldn't setsockopt(TCP_NODELAY)\n");
        }
        return sd;
    }



    bool NetComm_TCPGetLocalPort(int sd,uint16_t& port)
    {
        struct sockaddr_in addr;
        socklen_t           len = sizeof(addr);

        memset(&addr,0,sizeof(addr));
        port    = 0;
        if( getsockname(sd, (struct sockaddr*) &addr, &len) != 0) {
            return false;
        }

        port = htons(addr.sin_port);    
        return true;
    }

    int NetComm_TCPCreate(const char *serv,uint16_t port,bool isbind,int connecttimeout_ms)
    {
        int 	sockfd	= -1;
        char	portstr[32];
        struct addrinfo hints, *res, *ressave;

        if( serv && *serv && inet_addr(serv) == inet_addr("127.0.0.1")){
            struct sockaddr_in addr;
            memset(&addr,0,sizeof(addr));
            addr.sin_family      = AF_INET;
            addr.sin_port        = htons(port);
            addr.sin_addr.s_addr = inet_addr(serv);

            return CreateTCP(NULL,(const sockaddr *)&addr,sizeof(addr),isbind,connecttimeout_ms);
        }

        bzero(&hints, sizeof(struct addrinfo));
        hints.ai_family 		= PF_UNSPEC;
        hints.ai_socktype		= SOCK_STREAM;
        if( isbind ){
            hints.ai_flags		= AI_PASSIVE;
        }
        else{
            if( !serv || !serv[0] ){
                RMXQUIC_TRACE_WARNING( "SockAPI_Create	param error for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
                return -1;
            }
        }

        sprintf(portstr,"%d",port);

        int ret =  getaddrinfo(serv, portstr, &hints, &res);
        if( ret != 0) {
            RMXQUIC_TRACE_WARNING( "SockAPI_Create getaddr fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
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

            sockfd = CreateTCP(res,NULL,0,isbind,connecttimeout_ms);
            if(sockfd >= 0) {
                break;
            }
        }while((res = res->ai_next) != NULL);

        freeaddrinfo(ressave);
        if( sockfd >= 0 )
            return sockfd;

        RMXQUIC_TRACE_WARNING( "SockAPI_Create fatal create fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
        return -1;
    }





    bool NetComm_RecvN(int sd,char* p, size_t size,int timeout_ms,uint16_t interval_wait_ms){
        if( sd < 0 )
            return false;

        while( size > 0 ){
            int ret;
            if( timeout_ms != -1 ){
				if( !NetComm_Poll(sd,timeout_ms) )
                   return false;

                timeout_ms = interval_wait_ms;
            }

            WRAP_SYSAPI( ret , recv(sd,p,size,0) );

            if( ret <= 0 ){
                RMXQUIC_TRACE_WARNING( " %d recv data error, ret: %d,errno:%d\n" ,sd,ret , errno);

                return false;
            }

            size	-= ret;
            p	+= ret;
        }

        return true;
    }

    bool NetComm_SendN(int sd,const char* p, size_t size){
        if( sd < 0 )
            return false;
        while( size > 0 ){
            int ret;
            WRAP_SYSAPI( ret , send(sd,p,size,0) );

            if( ret <= 0 ){
                RMXQUIC_TRACE_WARNING( " %d send data error, ret: %d,errno:%d\n" ,sd,ret , errno);
                return false;
            }

            size	-= ret;
            p	+= ret;
        }

        return true;
    }


    bool NetComm_StartTask(pthread_t *thrid,void* (*start_routine)(void*),void *arg)
    {
        pthread_t slave_tid;
        int ret = pthread_create(thrid ? thrid : &slave_tid, NULL, start_routine, arg);
	if( ret == 0 && !thrid )
	    pthread_detach(slave_tid);
        return ret == 0;
    }




    static int Rsa2String(RSA *r,std::string& out,bool isPublic)
    {
        //bp_private = BIO_new_file(RSA_PRIV_FILE, "w+");
        BIO*	bp	= BIO_new(BIO_s_mem());
        int 	ret = isPublic ? PEM_write_bio_RSAPublicKey(bp, r) :
            PEM_write_bio_RSAPrivateKey(bp, r, NULL, NULL, 0, NULL, NULL);;
        if(ret != 1){
            out.clear();
            BIO_free_all(bp);
            return 0;
        }

        int keylen = BIO_pending(bp);	 
        char	*buf	= new char[keylen+1];

        BIO_read(bp,buf , keylen);
        buf[keylen] = 0;
        out.assign(buf,keylen+1);
        delete[] buf;

        BIO_free_all(bp);
        return 1;
    }

    bool NetComm_RSAGenKey(std::string& priv,std::string& pub){
        priv.clear();
        pub.clear();

        RSA 			*r = NULL;

        // 1. generate rsa key
        BIGNUM* bne = BN_new();
        int ret = BN_set_word(bne,RSA_F4);
        if(ret != 1){
            goto free_all;
        }

        r = RSA_new();
        ret = RSA_generate_key_ex(r, 2048, bne, NULL);
        if(ret != 1){
            goto free_all;
        }

        // 2. save public key
        // 3. save private key
        ret = Rsa2String(r,priv,false);
        if( ret == 1 ){
            ret = Rsa2String(r,pub,true);
            if( ret != 1 ){
                priv.clear();
            }
        }

        // 4. free
free_all:
        RSA_free(r);
        BN_free(bne);

	/* ////	ONLY CHECK RSA SIGN/VERIFY FUNCTION
	std::string sign;
	const char* data	= "hellpafafasfasfadsasfsdafafasdfsdagfgfafgadsfasfasdfadf";
	int ret1 = NetComm_RSASign(sign,data,strlen(data),priv.data(),priv.size());
	int ret2 = NetComm_RSAVerify(sign,data,strlen(data),pub.data(),pub.size());
	*/
        return (ret == 1);
    }

    int NetComm_RSA256Hash(unsigned char hash[32],const char *string, int len)
    {
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, string, len);
        SHA256_Final(hash, &sha256);
        return 0;
    }


    int NetComm_RSASign(std::string &sign,const char *data, int dataLen,const char *privkey, int privkeylen)
    {   
        if( !data || dataLen <= 0 || !privkey || privkeylen <= 0 )
            return -1;

	unsigned char hash[SHA256_DIGEST_LENGTH]; 
	unsigned char signature[2048/8]; 
	unsigned int signatureLength; 
	int sigret = 0; 

	SHA256( (unsigned char *)data,dataLen, hash );

	BIO *mem = BIO_new_mem_buf((void *)privkey, privkeylen);
	if (mem != NULL){
	    RSA *rsa_private = PEM_read_bio_RSAPrivateKey(mem, NULL, NULL, NULL);
	    if (rsa_private != NULL
		    && RSA_check_key(rsa_private) != 0 ){
		sigret = RSA_sign( NID_sha256, hash, SHA256_DIGEST_LENGTH, signature, &signatureLength, rsa_private );
		if( sigret == 1 )
		    sign.assign((char *)signature,signatureLength);
	    }

	    if( rsa_private )
		RSA_free(rsa_private);
	    BIO_free (mem);
	}

	return sigret == 1 ? 0 : -1;
    }

    int NetComm_RSAVerify(const std::string &sign,const char *data, int dataLen,const char *pubkey, int pubkeylen)
    {
	if( !data || dataLen <= 0 || !pubkey || pubkeylen <= 0 )
	    return -1;

	unsigned char hash[SHA256_DIGEST_LENGTH]; 
	int sigret = 0; 

	SHA256( (unsigned char *)data,dataLen, hash );

	BIO *mem = BIO_new_mem_buf((void*)pubkey, pubkeylen);
	if (mem != NULL){
	    RSA *rsa_public = PEM_read_bio_RSAPublicKey(mem, NULL, NULL, NULL);
	    if (rsa_public != NULL ){
		sigret = RSA_verify( NID_sha256, hash, SHA256_DIGEST_LENGTH, (const unsigned char *)sign.data(), sign.size(), rsa_public );
	    }

	    if( rsa_public )
		RSA_free(rsa_public);
	    BIO_free (mem);
	}

	return sigret == 1 ? 0 : -1;
    }



#if( RMXUDP_SSLMODE == 0 )
#define OPENSSL_PRN_INFO(fmt,arg...)    //RMXQUIC_TRACE_INFO(fmt,##arg)

static    int gcmdeccnt = 0;
static    int gcmenccnt = 0;
#define OPENSSL_PRN_ERROR(fmt,arg...)   {   \
    char    buf[256]    = "";   unsigned long err = ERR_get_error();    \
    RMXQUIC_TRACE_WARNING(  fmt " err:%lu dec/enc:%d/%d <%s>\n",##arg,err,gcmdeccnt,gcmenccnt,ERR_error_string(err,buf)); \
}

#define OPENSSL_PUT_ERROR(x,y)      OPENSSL_PRN_ERROR( #x " " #y "dec/enc:%d/%d \n",gcmdeccnt,gcmenccnt)



    int NetComm_GCMEncrypt(const unsigned char *plaintext, int plaintext_len, const unsigned char *aad,
            int aad_len, const unsigned char *key,const unsigned char *iv,int iv_len,
            unsigned char *ciphertext,int taglen)
    {
        EVP_CIPHER_CTX *ctx;

        int len;

        int ciphertext_len;


        /* Create and initialise the context */
        if(!(ctx = EVP_CIPHER_CTX_new())){
            OPENSSL_PRN_ERROR("create ctx fail\n");
            return -1;
        }

        /* Initialise the encryption operation. */
        if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("init ctx fail\n");
            return -1;
        }

        /* Set IV length if default 12 bytes (96 bits) is not appropriate */
        if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_SET_IVLEN fail\n");
            return -1;
        }

        /* Initialise key and IV */
        if(1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_SET_IV + key fail\n");
            return -1;
        }

        /* Provide any AAD data. This can be called zero or more times as
         * required
         */
        if( aad && aad_len > 0 ){
            if(1 != EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len)){
                EVP_CIPHER_CTX_free(ctx);
                OPENSSL_PRN_ERROR("EVP_EncryptUpdate NULL fail\n");
                return -1;
            }
        }


        OPENSSL_PRN_INFO("enc: %d out: txtlen %d + %d aad:%d \n", plaintext_len,ciphertext_len,len,aad_len);

        /* Provide the message to be encrypted, and obtain the encrypted output.
         * EVP_EncryptUpdate can be called multiple times if necessary
         */
        if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_EncryptUpdate txt fail\n");
            return -1;
        }
        ciphertext_len = len;

        /* Finalise the encryption. Normally ciphertext bytes may be written at
         * this stage, but this does not occur in GCM mode
         */
        if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_EncryptFinal_ex fail\n");
            return -1;
        }

        OPENSSL_PRN_INFO("enc:%s %d out: txtlen %d + %d\n",plaintext, plaintext_len,ciphertext_len,len);
        ciphertext_len += len;

        /* Get the tag */
        if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, taglen, ciphertext + ciphertext_len )){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_GET_TAG fail\n");
            return -1;
        }

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        gcmenccnt ++;
        return ciphertext_len + taglen;
    }

    int NetComm_GCMDecrypt(const unsigned char *ciphertext, int ciphertext_len, const unsigned char *aad,
            int aad_len,const unsigned char *key, const unsigned char *iv,int iv_len,
            unsigned char *plaintext,int taglen)
    {
        EVP_CIPHER_CTX *ctx;
        int len;
        int plaintext_len;
        int ret;

        /* Create and initialise the context */
        if(!(ctx = EVP_CIPHER_CTX_new())){
            OPENSSL_PRN_ERROR("EVP_CIPHER_CTX_new fail\n");
            return -1;
        }

        /* Initialise the decryption operation. */
        if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_DecryptInit_ex fail\n");
            return -1;
        }

        /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
        if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_SET_IVLEN fail\n");
            return -1;
        }

        /* Initialise key and IV */
        if(!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_GET_TAG fail\n");
            return -1;
        }

        /* Provide any AAD data. This can be called zero or more times as
         * required
         */
        if( aad && aad_len > 0 ){
            if(!EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len)){
                EVP_CIPHER_CTX_free(ctx);
                OPENSSL_PRN_ERROR("EVP_DecryptUpdate aad fail\n");
                return -1;
            }
        }

        /* Provide the message to be decrypted, and obtain the plaintext output.
         * EVP_DecryptUpdate can be called multiple times if necessary
         */

        if(!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len - taglen)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_DecryptUpdate txt fail\n");
            return -1;
        }
        plaintext_len = len;

        /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
        if( taglen > 0 ){
            unsigned char tag[taglen];
            memcpy(tag,ciphertext + plaintext_len,taglen);
            if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, taglen,tag)){
                EVP_CIPHER_CTX_free(ctx);
                OPENSSL_PRN_ERROR("EVP_CTRL_GCM_SET_TAG fail\n");
                return -1;
            }
        }

        /* Finalise the decryption. A positive return value indicates success,
         * anything else is a failure - the plaintext is not trustworthy.
         */
        ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
        OPENSSL_PRN_INFO("dec:%s %d in: ciphlen %d + %d ret:%d\n",plaintext, plaintext_len,ciphertext_len- taglen,len,ret);

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        if(ret > 0)
        {
            /* Success */
            plaintext_len += len;
            gcmdeccnt ++;
            return plaintext_len;
        }
        else
        {
            /* Verify failed */
            OPENSSL_PRN_ERROR("dec: %d in: ciphlen %d + %d ret:%d\n",plaintext_len,ciphertext_len- taglen,len,ret);
            return -1;
        }
    }
#endif
}



#ifndef RMXUDPLIBMODE
#define TEST_SHOW_SCREEN
#else
#undef  TEST_SHOW_SCREEN
#endif

#define RMXUDPLOG_FILENAME    "rmxudp.log"

int  g_RmxQuicTraceFlag = 0xffffffff;

void RmxQuicTraceLog(uint32_t flag,const char*file,const char *func,int lineno,const char *fmt,...)
{
    va_list arg_ptr;

    struct timespec    now;
    struct tm day;


    clock_gettime(CLOCK_REALTIME,&now);
    day   = *localtime(&now.tv_sec);

    const char* color = NULL;
    const char* title = "";
    switch(flag){
        case RMXTRACE_FLAG_SENDFRAME:
            title = "SND"; break;
        case RMXTRACE_FLAG_RECVFRAME:
            title = "RCV"; break;
        case RMXTRACE_FLAG_DEBUG:
            color = "[46m";   title = "DBG"; break;
        case RMXTRACE_FLAG_INFO:
            color = "[42m";   title = "INFO"; break;
        case RMXTRACE_FLAG_WARNING:
            color = "[45m";   title = "WARN"; break;
        case RMXTRACE_FLAG_ERROR:
            color = "[41m";   title = "ERR"; break;
        case RMXTRACE_FLAG_FATAL:
            color = "[41m";   title = "FATAL"; break;
        case RMXTRACE_FLAG_TIMER:
            title = "TIMER"; break;
        case RMXTRACE_FLAG_CERT:
            title = "CERT"; break;
        case RMXTRACE_FLAG_IO:
            title = "IO"; break;
        case RMXTRACE_FLAG_SESSION:
            title = "SESS"; break;

        case RMXTRACE_FLAG_CRYPT:
            title = "CRYPT";    break;

        default:
            break;
    }

    const char* pfile = strrchr(file,'/');
    if( !pfile )
        pfile   = file;
    else
        pfile ++;

#ifdef TEST_SHOW_SCREEN
    va_start(arg_ptr, fmt);
    if( color ){
        printf("\e%s %02d:%02d:%02d.%06ld %ld %s %s:%d(%s) \e[0m",
                color,day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000,syscall(__NR_gettid),title,
                pfile,lineno,func);
    }
    else{
        printf("%02d:%02d:%02d.%06ld %ld %s %s:%d(%s) ",
                day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000,syscall(__NR_gettid),title,
                pfile,lineno,func);
    }
    vprintf(fmt,arg_ptr);
    va_end(arg_ptr);
#endif

#if 0
    static int fd          = -1;
    if( fd == -1 ){
#ifndef RMXUDPLIBMODE
        size_t buflen = 1024;
        char buff[buflen];

        ssize_t len = readlink("/proc/self/exe", buff, buflen);

        if (len != -1){
            strcpy(buff+len,".log");
        }
        else{
            strcpy(buff,RMXUDPLOG_FILENAME);
        }

#ifdef TRACE_DETAIL
        fd = openat(AT_FDCWD, buff, O_CREAT|O_TRUNC| O_WRONLY| O_CLOEXEC,0666);
#else
        fd = openat(AT_FDCWD, buff, O_CREAT|O_APPEND| O_CLOEXEC,0666);
#endif
#else
        fd = openat(AT_FDCWD, RMXUDPLOG_FILENAME, O_CREAT| O_APPEND| O_CLOEXEC,0666);
#endif
        if( fd == -1 ){
            return ;
        }
    }

    dprintf(fd,"%02d:%02d:%02d.%06ld %ld %s %s:%d(%s) ",
            day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000,syscall(__NR_gettid),title,
            pfile,lineno,func);

    va_start(arg_ptr, fmt);
    vdprintf(fd,fmt,arg_ptr);
    va_end(arg_ptr);
#endif
}


#define RAIN_DBGTRACE(fmt,arg...)       RainTrace( "[%s:%d] " fmt ,__func__,__LINE__,##arg)
void RainTrace(const char *fmt,...) __attribute__ ((format (gnu_printf, 1, 2)));
#ifdef ANDROID
struct BacktraceState
{
    void** current;
    void** end;
};

static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg)
{
    BacktraceState* state = static_cast<BacktraceState*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = reinterpret_cast<void*>(pc);
        }
    }
    return _URC_NO_REASON;
}



size_t captureBacktrace(void** buffer, size_t max)
{
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(unwindCallback, &state);

    return state.current - buffer;
}


void RainTrace(const char *fmt,...)
{
#if 0
    void *callstack[128];
    const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
    va_list arg_ptr;
    int i = 1;          //  up one call level
    int nFrames;
    char **symbols  = NULL;
    struct timespec    now;
    struct tm day;

    int fd = openat(AT_FDCWD, RMXUDPLOG_FILENAME, O_CREAT|O_APPEND| O_WRONLY,0666);
    if( fd == -1 ){
        return ;
    }

    va_start(arg_ptr, fmt);

    clock_gettime(CLOCK_REALTIME,&now);
    day   = *localtime(&now.tv_sec);
#ifdef TEST_SHOW_SCREEN
    printf("%02d:%02d:%02d.%06ld ",day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000);
    vprintf(fmt,arg_ptr);
#else
    dprintf(fd,"%02d:%02d:%02d.%06ld ",day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000);
    vdprintf(fd,fmt,arg_ptr);
#endif
    va_end(arg_ptr);


    dprintf(fd,"backtrace:\n");
#ifdef ANDROID
    nFrames         = captureBacktrace(callstack, nMaxFrames);
#else
    nFrames         = backtrace(callstack, nMaxFrames);
    symbols         = backtrace_symbols(callstack, nFrames);
#endif

    for (; i < nFrames; i++) {
        /*  simple mode,only print address
            dprintf(fd,"\t<%s>\t", symbols[i]);
            */
        //  complex mode, try to resolve address name,need -ldl link option
        Dl_info info;
        char *demangled     = NULL;
        const char* symbol  = symbols ? symbols[i] : NULL;

        if (!dladdr(callstack[i], &info) || !info.dli_sname)
        {
            dprintf(fd, "%-3d %*p %s\n",
                    i, (int)(2 + sizeof(void*) * 2), callstack[i], symbol);
            continue;
        }

        symbol      = info.dli_sname;
#if ( defined __cplusplus)&& (!defined ANDROID)
        if (info.dli_sname[0] == '_'){
            int status          = -1;
            demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
            //  android version, __cxxabiv1::__cxa_demangle(info.dli_sname, NULL, 0, &status);
            if( status == 0 ){
                symbol  = demangled;
            }
        }
#endif

        dprintf(fd, "%-3d %*p %s + %zd\n",
                i, (int)(2 + sizeof(void*) * 2), callstack[i],
                symbol,
                (char *)callstack[i] - (char *)info.dli_saddr);
        if( demangled )
            free(demangled);
    }

#ifndef ANDROID
    free(symbols);
#endif
    if (nFrames == nMaxFrames)
        dprintf(fd, "[truncated]\n");
    else
        write(fd,"\n",1);
    close(fd);
#endif
}
#endif

#ifdef TRACE_DETAIL
static int DecQuichead(const uint8_t *content, int len,const sockaddr_in *addr,int issend,int ret)
{
    int     cidlen      = 0;
    char    cidstr[32]  = "";
    int     pnumlen     = 1;
    char    pnumstr[32] = "";
    char    addrstr[32] = "";
    uint8_t flag        = content[0];

    switch( flag & 0xC ){
        case 0xC:        cidlen     = 8;     break;
        case 0x8:        cidlen     = 4;     break;
        case 0x4:        cidlen     = 1;     break;
        default:        break;
    }

    switch( flag & 0x30 ){
        case 0x30:      pnumlen     = 6;     break;
        case 0x20:      pnumlen     = 4;     break;
        case 0x10:      pnumlen     = 2;     break;
        default:        break;
    }

    content ++;
    if( cidlen ){
        switch( cidlen ){
            case 8:         sprintf(cidstr," CID-%02X%02X%02X%02X%02X%02X%02X%02X ",
                                    content[0],content[1],content[2],content[3],
                                    content[4],content[5],content[6],content[7]
                                   );     
                            break;
            case 4:         sprintf(cidstr," CID-%02X%02X%02X%02X ",
                                    content[0],content[1],content[2],content[3]);     
                            break;
            case 1:         sprintf(cidstr," CID-%02X ",content[0]);     break;
            default:        break;
        }
        content += cidlen;
    }

    switch( pnumlen ){
        case 1:     sprintf(pnumstr," PKGNUM-%02X ",content[0]);     break;
        case 2:     sprintf(pnumstr," PKGNUM-%02X%02X ",
                            content[0], content[1]);     
                    break;
        case 4:     sprintf(pnumstr," PKGNUM-%02X%02X%02X%02X ",
                            content[0], content[1],content[2],content[3]);     
                    break;
        case 6:     sprintf(pnumstr," PKGNUM-%02X%02X%02X%02X%02X%02X ",
                            content[0], content[1],content[2],content[3],content[4],content[5]);     
                    break;
    }

    content += pnumlen;

    if( addr ){
        const uint8_t    *p  = (const uint8_t    *)&addr->sin_addr.s_addr;
        sprintf(addrstr,"%s %d.%d.%d.%d:%d ",
                issend ? "TO": "FROM",
                p[0],p[1],p[2],p[3],htons(addr->sin_port));
    }

    RMXQUIC_TRACE_IO("\e[36mD %s %slen:%d flag: %s%s%s%sbodylen:%d "
            "ret:%d/%d "
            "data:%02x%02x%02x%02x %02x%02x%02x%02x "
            "%02x%02x%02x%02x %02x%02x%02x%02x \e[0m\n",
            issend ? "SEND" : "RECV",
            addrstr,
            len, flag & 1 ? "VER ":"", flag & 2 ? "RST ":"", cidstr,pnumstr,
            len - (1 + cidlen + pnumlen) ,ret,errno,
            content[0],content[1],content[2],content[3],
            content[4],content[5],content[6],content[7],
            content[8],content[9],content[10],content[11],
            content[12],content[13],content[14],content[15]
            );

    return 0;
}
#endif

static inline char tochar(unsigned char c)
{
    return  isprint(c) ? (char )c : '.';
}
void NetComm_DumpHex(const unsigned char* content, int contentlen)
{
    int                 needtab         = 0;
    int                 offset          = 0;
    const unsigned char *p              = content ;
    const char          *pstr           = (char *)p;

    if( contentlen <= 0 || !content )
        return ;

    printf("\n\tHEX: \t");
    for( ; offset +4< contentlen ; ){
        if( needtab ){
            needtab = 0;
            printf("\t\t");
        }
        printf("%02x%02x%02x%02x ",p[0],p[1],p[2],p[3]);
        offset += 4;
        p      += 4;

        if( offset != 0 && (offset %16 == 0 ) ){
            printf(": %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
                    tochar(pstr[0]),tochar(pstr[1]),tochar(pstr[2]),tochar(pstr[3]), tochar(pstr[4]),tochar(pstr[5]),tochar(pstr[6]),tochar(pstr[7]), tochar(pstr[8]),
                    tochar(pstr[9]),tochar(pstr[10]),tochar(pstr[11]), tochar(pstr[12]),tochar(pstr[13]),tochar(pstr[14]),tochar(pstr[15])
                  );
            pstr    = (char *)p;
            needtab = 1;
        }
        /*
           PRN_SHOWBUF("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x :%c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c\n",
           p[0],p[1],p[2],p[3], p[4],p[5],p[6],p[7], p[8],p[9],p[10],p[11], p[12],p[13],p[14],p[15],
           tochar(p[0]),tochar(p[1]),tochar(p[2]),tochar(p[3]), tochar(p[4]),tochar(p[5]),tochar(p[6]),tochar(p[7]), tochar(p[8]),
           tochar(p[9]),tochar(p[10]),tochar(p[11]), tochar(p[12]),tochar(p[13]),tochar(p[14]),tochar(p[15])
           );
           */
    }


    if( offset < contentlen ){
        if( needtab ){
            needtab = 0;
            printf("\t\t");
        }

        for( ;offset < contentlen ; offset ++, p ++ ){
            printf("%02x",*p);
        }

        if( contentlen%16 ){
            int     restnum         = 16 - (contentlen%16);
            char    spacebuf[32]    = "";
            if( restnum >= 12 ){
                restnum += 2;
            }
            else if ( restnum >= 8 ){
                restnum += 1;
            }
            /*else if ( restnum > 4 ){
              restnum += 0;
              }*/

            if( restnum %4 == 0 ){
                restnum --;
            }

            memset(spacebuf,' ',sizeof(spacebuf)-1);
            spacebuf[ restnum*2 ] = 0;

            printf("%s",spacebuf);
        }
    }

    if( (void*)pstr != (void *)p ){
        printf(" : ");
        for( ; (void *)pstr != (void *)p ; pstr ++ ){
            printf("%c",tochar(*pstr));
        }
    }

    printf("\t\t\n");
}
