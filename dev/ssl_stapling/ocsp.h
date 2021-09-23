#ifndef VMI_OCSP_H_
#define VMI_OCSP_H_

struct tls_ocsp_result {
	const char *result_msg;
	int response_status;
	int cert_status;
	int crl_reason;
	time_t this_update;
	time_t next_update;
	time_t revocation_time;
};

struct tls_ocsp {
	/* responder location */
	char *ocsp_url;

	/* cert data, this struct does not own these */
	X509 *main_cert;
	STACK_OF(X509) *extra_certs;

	struct tls_ocsp_result ocsp_result;
};

struct ocsp_ctl {
	SSL     *ssl_conn;
	SSL_CTX *ssl_ctx;

	struct tls_ocsp ocsp;        
};

#define tls_set_error(ctx,fmt,...)      printf("[%s:%d] " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)
#define tls_set_errorx(ctx,fmt,...)     printf("[%s:%d] " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)
#define tls_set_info(ctx,fmt,...)       printf("[%s:%d] " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)
#define tls_set_debug(ctx,fmt,...)      printf("[%s:%d] " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)

int ocsp_initctx(SSL_CTX* ctx);
int ocsp_initssl(SSL* ssl,SSL_CTX* ctx);
void ocsp_releasessl(SSL* ssl);

#endif
