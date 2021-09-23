#ifndef OCSP_SERVER_H_
#define OCSP_SERVER_H_

#ifndef TMMS_ERROR
#define Debug(title,fmt,...)    printf("[%s:%d] " title ":" fmt "\n", __func__,__LINE__, ##__VA_ARGS__)
#define Error(fmt,...)          {   char sslerrorbuf[240];int sslerrorcode= ERR_get_error();\
    printf("[%s:%d] SSL Error:%d/%s\n" fmt "\n", __func__,__LINE__, sslerrorcode,ERR_error_string(sslerrorcode,sslerrorbuf),##__VA_ARGS__);   \
}
#define Note(fmt,...)           printf("[%s:%d] " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)
#else
#define Error(fmt,...)          TMMS_ERROR(fmt,##__VA_ARGS__)
#define Debug(fmt,...)          TMMS_DEBUG(fmt,##__VA_ARGS__)
#define Note(fmt,...)           TMMS_INFO(fmt,##__VA_ARGS__)
#endif

int ocsp_initsvr(SSL_CTX *ctx, X509 *cert, const char *certname);
int ocsp_releasesvr();

#endif
