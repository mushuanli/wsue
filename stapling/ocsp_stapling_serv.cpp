#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#include <pthread.h>
#include <sys/prctl.h>

#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <openssl/ocsp.h>

#include "ocsp_server.h"

// Maxiumum OCSP stapling response size.
// This should be the response for a single certificate and will typically include the responder certificate chain,
// so 10K should be more than enough.
#define MAX_STAPLING_DER 10240
#define OCSP_MAX_RESPONSE_TIME_SKEW 300

//#define OCSP_STAPLING_FILE  "/n/tmp/ssl/2a/vmi/domain.crt.ocsp"
#undef OCSP_STAPLING_FILE

struct der_info{
    pthread_mutex_t     stapling_mutex;
    bool                is_expire;
    time_t              expire_time;
    unsigned int        resp_derlen;
    unsigned char       resp_der[MAX_STAPLING_DER];
};

struct stapling_info{
    int         update_timeout;
    X509        *cert;
    char        *uri;             // Responder details

    unsigned char idx[20]; // Index in session cache SHA1 hash of certificate
    OCSP_CERTID *cid;      // Certificate ID for OCSP requests or NULL if ID cannot be determined
};

// Cached info stored in SSL_CTX ex_info
struct certinfo {
    SSL_CTX             *ssl_ctx;
    pthread_t           thrid;

    struct stapling_info stap;
    struct der_info     der;
};

static void* ocsp_update_task(void*);

static const int ssl_ocsp_cache_timeout     = 3600;
static const int ssl_ocsp_update_timeout    = 600;
static const int ssl_ocsp_request_timeout   = 10;
static int       ssl_ocsp_revoked_cert_stat = 0;
static int       ssl_ocsp_unknown_cert_stat = 0;
struct certinfo  ssl_ocspctl                = {
    NULL,
    0,
    {ssl_ocsp_update_timeout},
    {PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP}
};


//--------------------------------------------------------------------------------------
/*
 *  This function returns the number of seconds  elapsed
 *  since the Epoch, 1970-01-01 00:00:00 +0000 (UTC) and the
 *  date presented un ASN1_GENERALIZEDTIME.
 *
 *  In parsing error case, it returns -1.
 */
static long asn1_generalizedtime_to_epoch(ASN1_GENERALIZEDTIME *d)
{
	long epoch;
	char *p, *end;
	const unsigned short month_offset[12] = {
		0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
	};
	int year, month;

	if (!d || (d->type != V_ASN1_GENERALIZEDTIME)) return -1;

	p = (char *)d->data;
	end = p + d->length;

	if (end - p < 4) return -1;
	year = 1000 * (p[0] - '0') + 100 * (p[1] - '0') + 10 * (p[2] - '0') + p[3] - '0';
	p += 4;
	if (end - p < 2) return -1;
	month = 10 * (p[0] - '0') + p[1] - '0';
	if (month < 1 || month > 12) return -1;
	/* Compute the number of seconds since 1 jan 1970 and the beginning of current month
	   We consider leap years and the current month (<marsh or not) */
	epoch = (  ((year - 1970) * 365)
		 + ((year - (month < 3)) / 4 - (year - (month < 3)) / 100 + (year - (month < 3)) / 400)
		 - ((1970 - 1) / 4 - (1970 - 1) / 100 + (1970 - 1) / 400)
		 + month_offset[month-1]
		) * 24 * 60 * 60;
	p += 2;
	if (end - p < 2) return -1;
	/* Add the number of seconds of completed days of current month */
	epoch += (10 * (p[0] - '0') + p[1] - '0' - 1) * 24 * 60 * 60;
	p += 2;
	if (end - p < 2) return -1;
	/* Add the completed hours of the current day */
	epoch += (10 * (p[0] - '0') + p[1] - '0') * 60 * 60;
	p += 2;
	if (end - p < 2) return -1;
	/* Add the completed minutes of the current hour */
	epoch += (10 * (p[0] - '0') + p[1] - '0') * 60;
	p += 2;
	if (p == end) return -1;
	/* Test if there is available seconds */
	if (p[0] < '0' || p[0] > '9')
		goto nosec;
	if (end - p < 2) return -1;
	/* Add the seconds of the current minute */
	epoch += 10 * (p[0] - '0') + p[1] - '0';
	p += 2;
	if (p == end) return -1;
	/* Ignore seconds float part if present */
	if (p[0] == '.') {
		do {
			if (++p == end) return -1;
		} while (p[0] >= '0' && p[0] <= '9');
	}

nosec:
	if (p[0] == 'Z') {
		if (end - p != 1) return -1;
		return epoch;
	}
	else if (p[0] == '+') {
		if (end - p != 5) return -1;
		/* Apply timezone offset */
		return epoch - ((10 * (p[1] - '0') + p[2] - '0') * 60 * 60 + (10 * (p[3] - '0') + p[4] - '0')) * 60;
	}
	else if (p[0] == '-') {
		if (end - p != 5) return -1;
		/* Apply timezone offset */
		return epoch + ((10 * (p[1] - '0') + p[2] - '0') * 60 * 60 + (10 * (p[3] - '0') + p[4] - '0')) * 60;
	}

	return -1;
}

static void stapinfo_init(struct stapling_info* pinfo)
{
    memset(pinfo,0,sizeof(*pinfo));
    pinfo->update_timeout   = ssl_ocsp_update_timeout;
}

static void stapinfo_free(struct stapling_info* pinfo)
{
    pinfo->cert     = NULL;
    if (pinfo->uri){
        OPENSSL_free(pinfo->uri);
        pinfo->uri   = NULL;
    }
    if (pinfo->cid){
        OCSP_CERTID_free(pinfo->cid);
        pinfo->cid   = NULL;
    }

}

static void derinfo_init(struct der_info* pinfo)
{
    pinfo->resp_derlen       = 0;
    pinfo->is_expire         = true;
    pinfo->expire_time       = 0;
}

static void derinfo_set(struct der_info* pinfo,unsigned char *resp_der,unsigned int resp_derlen)
{
    pthread_mutex_lock(&pinfo->stapling_mutex);
    memcpy(pinfo->resp_der, resp_der, resp_derlen);
    pinfo->resp_derlen = resp_derlen;
    pinfo->is_expire = false;
    pinfo->expire_time = time(NULL) + ssl_ocsp_cache_timeout;
    pthread_mutex_unlock(&pinfo->stapling_mutex);
}

static void derinfo_free(struct der_info* pinfo)
{
    pthread_mutex_destroy(&pinfo->stapling_mutex);
}

static unsigned char* derinfo_dump(struct der_info* pinfo,unsigned int *len)
{
    int current_time = time(NULL);
    pthread_mutex_lock(&pinfo->stapling_mutex);
    if (pinfo->resp_derlen == 0 || pinfo->is_expire || pinfo->expire_time < current_time) {
	    pinfo->is_expire = true;
        pthread_mutex_unlock(&pinfo->stapling_mutex);
        return NULL;
    } 

    *len             = pinfo->resp_derlen;
    unsigned char *p = (unsigned char *)OPENSSL_malloc(pinfo->resp_derlen);
    if( p ){
        memcpy(p, pinfo->resp_der, pinfo->resp_derlen);
    }
    pthread_mutex_unlock(&pinfo->stapling_mutex);
    return p;
}

    static void
certinfo_init(certinfo *cinf)
{
    if (!cinf )
        return;
    cinf->ssl_ctx   = NULL;

    stapinfo_init(&cinf->stap);
    derinfo_init(&cinf->der);
}

    static void
certinfo_free(certinfo *cinf)
{
    if (!cinf )
        return;
    cinf->ssl_ctx   = NULL;

    if( cinf->thrid != 0 ){
        pthread_cancel(cinf->thrid);
        cinf->thrid = 0;
    }
    stapinfo_free(&cinf->stap);
    derinfo_free(&cinf->der);
}



//--------------------------------------------------------------------------------------


    static X509 *
stapling_get_issuer(SSL_CTX *ssl_ctx, X509 *x,const char* issuefile )
{
#if 1
    X509 *issuer = NULL;
    int i;
    X509_STORE *st = SSL_CTX_get_cert_store(ssl_ctx);
    X509_STORE_CTX *inctx = NULL;
    STACK_OF(X509) *extra_certs = NULL;

#ifdef SSL_CTX_get_extra_chain_certs
    SSL_CTX_get_extra_chain_certs(ssl_ctx, &extra_certs);
#else
    extra_certs = ssl_ctx->extra_certs;
#endif

    if (sk_X509_num(extra_certs) == 0){
        Error("certs num zero");
        return NULL;
    }

    for (i = 0; i < sk_X509_num(extra_certs); i++) {
        issuer = sk_X509_value(extra_certs, i);
        if (X509_check_issued(issuer, x) == X509_V_OK) {
            // TODO: CRYPTO_add(&issuer->references, 1, CRYPTO_LOCK_X509);
            return issuer;
        }
    }

    inctx = X509_STORE_CTX_new();
    if( !inctx ){
        Error("create store ctx fail");
        return NULL;
    }

    if (X509_STORE_CTX_init(inctx, st, NULL, NULL) != 1){
        Error("init store ctx fail");
        X509_STORE_CTX_free(inctx);
        return NULL;
    }
    int rc = X509_STORE_CTX_get1_issuer(&issuer, inctx, x);
    X509_STORE_CTX_cleanup(inctx);
    X509_STORE_CTX_free(inctx);

    if ( rc < 0){
        Error("load store ctx issuer fail");
        issuer = NULL;
    }
    else if( rc == 0 ){
        Error("load store ctx issuer zero, ignore stapling");
        issuer = NULL;
    }

    return issuer;
#else
    BIO *in = NULL;
    X509 *issuer = NULL;

    /* reading from a file */
    in = BIO_new(BIO_s_file());
    if (in == NULL)
        goto end;

    if (BIO_read_filename(in, issuefile) <= 0)
        goto end;

    issuer = PEM_read_bio_X509_AUX(in, NULL, NULL, NULL);
    if (!issuer) {
        Error("issue cannot be read or parsed'.\n",
                issuefile);
        goto end;
    }

end:

    ERR_clear_error();
    if (in)
        BIO_free(in);

    return issuer;    
#endif
}

    static bool
stapinfo_set(stapling_info  *cinf,SSL_CTX *ctx, X509 *cert, const char *issuefile)
{
    X509      *issuer;
    STACK_OF(OPENSSL_STRING) *aia = NULL;

    if (!cert) {
        Error("Null cert passed in");
        return false;
    }

    // Initialize certinfo
    cinf->cid = NULL;
    cinf->uri = NULL;


    issuer = stapling_get_issuer(ctx, cert,issuefile);
    if (issuer == NULL) {
        Error( "cannot get issuer certificate from !");
        return false;
    }

    cinf->cid = OCSP_cert_to_id(NULL, cert, issuer);
    if (!cinf->cid){
        Error( "cannot get issuer id failed!");
        return false;
    }
    X509_digest(cert, EVP_sha1(), cinf->idx, NULL);

    aia = X509_get1_ocsp(cert);
    if (aia){
        cinf->uri = sk_OPENSSL_STRING_pop(aia);
        X509_email_free(aia);
    }
    if (!cinf->uri) {
        Error("no responder URI");
        stapinfo_free(cinf);
        return false;
    }
    Debug("ssl_ocsp", "success to init certinfo into SSL_CTX: %p", ctx);
    return true;
}

    static bool
stapling_cache_response(OCSP_RESPONSE *rsp, certinfo *cinf)
{
    unsigned char buf[MAX_STAPLING_DER];
    unsigned char *p    = buf;

    int resp_derlen = i2d_OCSP_RESPONSE(rsp, &p);

    if (resp_derlen == 0) {
        Error("can not encode OCSP stapling response");
        return false;
    }

    if (resp_derlen > MAX_STAPLING_DER) {
        Error("OCSP stapling response too big (%u bytes)", resp_derlen);
        return false;
    }

    derinfo_set(&cinf->der,buf,resp_derlen);

    Debug("ssl_ocsp", "success to cache response");
    return true;
}

    static bool
stapling_parse_response(certinfo *cinf, OCSP_RESPONSE *resp)
{
	OCSP_BASICRESP *bs = NULL;
	OCSP_SINGLERESP *sr;
	OCSP_CERTID *id;
	int rc , count_sr;
	ASN1_GENERALIZEDTIME *revtime, *thisupd, *nextupd = NULL;
	int reason,status;
	bool ret = false;
	
	rc = OCSP_response_status(resp);
	if (rc != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		Error("OCSP response status not successful");
		goto out;
	}

	bs = OCSP_response_get1_basic(resp);
	if (!bs) {
		Error("Failed to get basic response from OCSP Response");
		goto out;
	}

	count_sr = OCSP_resp_count(bs);
	if (count_sr > 1) {
		Error("OCSP response ignored because contains multiple single responses (%d)", count_sr);
		goto out;
	}

	sr = OCSP_resp_get0(bs, 0);
	if (!sr) {
		Error("Failed to get OCSP single response");
		goto out;
	}

#if 0   // high version openssl       
        if (cid) {
            id = (OCSP_CERTID*)OCSP_SINGLERESP_get0_id(sr);
            if (OCSP_id_cmp(id, cid)) {
                Error("OCSP single response: Certificate ID does not match certificate and issuer");
                goto out;
            }
        }
	rc = OCSP_single_get0_status(sr, &reason, &revtime, &thisupd, &nextupd);
        status  = rc;
        rc      = 1;
#else
        rc = OCSP_resp_find_status(bs, cinf->stap.cid, &status, &reason, &revtime, &thisupd, &nextupd);
        switch (status) {
            case V_OCSP_CERTSTATUS_GOOD:
                break;
            case V_OCSP_CERTSTATUS_REVOKED:
                ssl_ocsp_revoked_cert_stat ++;
                break;
            case V_OCSP_CERTSTATUS_UNKNOWN:
                ssl_ocsp_unknown_cert_stat ++;
                break;
            default:
                break;
        }        
#endif   
	if (rc != 1 || (status != V_OCSP_CERTSTATUS_GOOD && status != V_OCSP_CERTSTATUS_REVOKED) ) {
		Error("OCSP single response: certificate status is unknown");
		goto out;
	}

	if (!nextupd) {
		Error("OCSP single response: missing nextupdate");
		goto out;
	}

	rc = OCSP_check_validity(thisupd, nextupd, OCSP_MAX_RESPONSE_TIME_SKEW, -1);
        if (!rc) {
            Error("OCSP single response: no longer valid.");
            goto out;
        }


        stapling_cache_response(resp,cinf);
#if 0
    int status, reason;
    OCSP_BASICRESP *bs = NULL;
    ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;
    int response_status = OCSP_response_status(rsp);

    // Check to see if response is an error.
    // If so we automatically accept it because it would have expired from the cache if it was time to retry.
    if (response_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
	    Error("error stat: %d",response_status);
        return SSL_TLSEXT_ERR_NOACK;
    }

    bs = OCSP_response_get1_basic(rsp);
    if (bs == NULL) {
        // If we can't parse response just pass it back to client
        Error("can not parsing response");
        return SSL_TLSEXT_ERR_OK;
    }
    if (!OCSP_resp_find_status(bs, cinf->cid, &status, &reason, &rev, &thisupd, &nextupd)) {
        // If ID not present just pass it back to client
        Error("certificate ID not present in response");
    } else {
        OCSP_check_validity(thisupd, nextupd, 300, -1);
    }

    switch (status) {
        case V_OCSP_CERTSTATUS_GOOD:
            break;
        case V_OCSP_CERTSTATUS_REVOKED:
            ssl_ocsp_revoked_cert_stat ++;
            break;
        case V_OCSP_CERTSTATUS_UNKNOWN:
            ssl_ocsp_unknown_cert_stat ++;
            break;
        default:
            break;
    }
#endif
        cinf->stap.update_timeout = asn1_generalizedtime_to_epoch(nextupd) - OCSP_MAX_RESPONSE_TIME_SKEW;
	ret = true;
out:
	ERR_clear_error();

	if (bs)
		 OCSP_BASICRESP_free(bs);

    return ret;
}

    static OCSP_RESPONSE *
query_responder(BIO *b, char *host, char *path, OCSP_REQUEST *req, int req_timeout)
{
    time_t end;
    OCSP_RESPONSE *resp = NULL;
    OCSP_REQ_CTX *ctx;
    int rv;

    end = time(NULL) + req_timeout;

    ctx = OCSP_sendreq_new(b, path, NULL, -1);
    OCSP_REQ_CTX_add1_header(ctx, "Host", host);
    OCSP_REQ_CTX_set1_req(ctx, req);

    do {
        rv = OCSP_sendreq_nbio(&resp, ctx);
        usleep(1000);
    } while ((rv == -1) && BIO_should_retry(b) && (time(NULL) < end));

    OCSP_REQ_CTX_free(ctx);

    if (rv)
        return resp;

    return NULL;
}

    static OCSP_RESPONSE *
process_responder(OCSP_REQUEST *req, char *host, char *path, char *port, int req_timeout)
{
    BIO *cbio = NULL;
    OCSP_RESPONSE *resp = NULL;
    cbio = BIO_new_connect(host);
    if (!cbio) {
        goto end;
    }
    if (port)
        BIO_set_conn_port(cbio, port);

    BIO_set_nbio(cbio, 1);
    if (BIO_do_connect(cbio) <= 0 && !BIO_should_retry(cbio)) {
        Error("ssl_ocsp", "process_responder: fail to connect to OCSP respond server");
        goto end;
    }
    resp = query_responder(cbio, host, path, req, req_timeout);

end:
    if (cbio)
        BIO_free_all(cbio);
    return resp;
}

    static bool
stapling_refresh_response(certinfo *cinf, OCSP_RESPONSE **prsp)
{
    bool rv = false;
    OCSP_REQUEST *req = NULL;
    OCSP_CERTID *id = NULL;
    char *host, *path, *port;
    int ssl_flag = 0;
    int req_timeout = -1;

    Debug("ssl_ocsp", "stapling_refresh_response: querying responder");
    *prsp = NULL;

    if (!OCSP_parse_url(cinf->stap.uri, &host, &port, &path, &ssl_flag)) {
        goto done;
    }

    req = OCSP_REQUEST_new();
    if (!req)
        goto done;
    id = OCSP_CERTID_dup(cinf->stap.cid);
    if (!id)
        goto done;
    if (!OCSP_request_add0_id(req, id))
        goto done;

    req_timeout = ssl_ocsp_request_timeout;
    *prsp = process_responder(req, host, path, port, req_timeout);

    if (*prsp == NULL) {
        goto done;
    }

    if (stapling_parse_response(cinf, *prsp) ) {
        Debug("ssl_ocsp", "update stapling response success,timeout: %d",cinf->stap.update_timeout);
    } else {
        Error("stapling_refresh_response: responder error");
    }


    rv  = true;
done:
    if (*prsp)
        OCSP_RESPONSE_free(*prsp);
    if (req)
        OCSP_REQUEST_free(req);

    if( !rv )
        Error("ssl_ocsp", "stapling_refresh_response: fail to refresh response");
    return rv;
}

#ifdef OCSP_STAPLING_FILE
    static bool
stapling_refresh_response_file(certinfo *cinf)
{
    unsigned char buf[MAX_STAPLING_DER];
    int fd  = open(OCSP_STAPLING_FILE,O_RDONLY);
    if( fd < 0 ){
        Error("read ocsp file failed: %d\n",errno);
        return false;
    }
    int ret         = 0;
    int resp_derlen = 0;
    unsigned char* p         = buf;
    while(1){
        ret = read(fd,p,MAX_STAPLING_DER - resp_derlen);
        if( ret < 0 ){
            if( errno == EINTR )
                continue;
            break;
        }
        if( ret == 0 )
            break;

        resp_derlen += ret;
        p           += ret;
        if( resp_derlen >= MAX_STAPLING_DER )
            break;
    }

    close(fd);
    if( ret < 0 ){
        Error("read ocsp file failed: %d\n",errno);
        return false;
    }

    if (resp_derlen == 0) {
        Error("stapling_refresh_response_file: can not encode OCSP stapling response");
        return false;
    }

    if ( resp_derlen >= MAX_STAPLING_DER ) {
        Error("stapling_refresh_response_file: can not encode OCSP stapling response too big");
        return false;
    }

    derinfo_set(&cinf->der,buf,resp_derlen);

    Debug("ssl_ocsp", "stapling_refresh_response_file: querying responder succ");
    return true;
}
#endif


    static void*
ocsp_update_task(void* )
{
    OCSP_RESPONSE *resp = NULL;
    time_t current_time;

    prctl(PR_SET_NAME,"stapling");

    while (ssl_ocspctl.ssl_ctx) {
        certinfo *cinf = &ssl_ocspctl;
        current_time = time(NULL);
        if (cinf->der.resp_derlen == 0 || cinf->der.is_expire || cinf->der.expire_time < current_time) {
#ifndef OCSP_STAPLING_FILE
            bool ret    = stapling_refresh_response(cinf, &resp);
#else
            bool ret    = stapling_refresh_response_file(cinf);
#endif
            if (ret) {
                Note("Success to refresh OCSP response for 1 certificate.");
            } else {
                Note("Fail to refresh OCSP response for 1 certificate.");
            }
        } 

        sleep(cinf->der.is_expire ? 60 : ssl_ocsp_update_timeout);
    }
}





// RFC 6066 Section-8: Certificate Status Request
    static int
ssl_callback_ocsp_stapling(SSL *ssl)
{
    certinfo *cinf = NULL;
    time_t current_time;

    // Assume SSL_get_SSL_CTX() is the same as reaching into the ssl structure
    // Using the official call, to avoid leaking internal openssl knowledge
    // originally was, cinf = stapling_get_cert_info(ssl->ctx);
    cinf = &ssl_ocspctl;
    if (cinf == NULL) {
        Error("ssl_ocsp", "fail to get certificate information");
        return SSL_TLSEXT_ERR_NOACK;
    }

    unsigned int len = 0;
    unsigned char *p = derinfo_dump(&cinf->der,&len);
    if ( !p ) {
        Error("ssl_ocsp", "fail to get certificate status");
        return SSL_TLSEXT_ERR_NOACK;
    } else {
        SSL_set_tlsext_status_ocsp_resp(ssl, p, len);
        Debug("ssl_ocsp", "success to get certificate status");
        return SSL_TLSEXT_ERR_OK;
    }
}


int ocsp_initsvr(SSL_CTX *ctx, X509 *cert, const char *issuefile)
{
    certinfo_init(&ssl_ocspctl);
#ifndef OCSP_STAPLING_FILE
    if( !stapinfo_set(&ssl_ocspctl.stap,ctx,cert,issuefile) )
        return -1;
#endif

    ssl_ocspctl.ssl_ctx   = ctx;
    if( pthread_create(&ssl_ocspctl.thrid,NULL,ocsp_update_task,NULL) != 0 ){
        Error("create ocsp stapling update thread failed!");
        return -1;
    }

    if (SSL_CTX_set_tlsext_status_cb(ctx, ssl_callback_ocsp_stapling) != 1) {
        Error( "ssl OCSP verification setup failure");
        return -1;
    }

    return 0;
}

int ocsp_releasesvr()
{
    certinfo_free(&ssl_ocspctl);
}


#ifdef  TEST_SSL_
#include <linux/tcp.h>
#include <openssl/pkcs12.h>

static X509* initCtxByRSA(SSL_CTX *ctx,const char* certfile,const char* keyfile)
{
    BIO *bio   = BIO_new_file(certfile, "r");
    if( !bio ){
       Error("load cert file %s failed",certfile);
        return NULL;
    }

    X509 *cert = PEM_read_bio_X509(bio, NULL, 0, NULL);
    BIO_free(bio);

    if( !cert ){
        Error("load cert bio from %s failed",certfile);
        return NULL;
    }

    if (!SSL_CTX_use_certificate(ctx, cert)) {
        Error("Failed to assign cert from %s to SSL_CTX", (const char *)certfile);
        X509_free(cert);
        return NULL;
    }


    if (SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM) <= 0 ) {
        Error("Failed to use cert from %s to SSL_CTX", (const char *)certfile);
        //ERR_print_errors_fp(stderr);
        X509_free(cert);
        return NULL;
    }

    return cert;
}

static X509_STORE* ca_store_ = NULL;
static X509 *initCtxByP12(SSL_CTX *ctx,const char* certfile)
{
    FILE *p12_file;
    PKCS12 *p12_cert = NULL;
    EVP_PKEY *pkey;
    X509 *x509_cert;
    STACK_OF(X509) *additional_certs = NULL;

    p12_file = fopen(certfile, "rb");
    if( !p12_file ){
        Error("Failed to open %s ", (const char *)certfile);
        return NULL;
    }
    d2i_PKCS12_fp(p12_file, &p12_cert);
    fclose(p12_file);

    if( !p12_cert ){
        Error("Failed to load %s ", (const char *)certfile);
        return NULL;
    }

    PKCS12_parse(p12_cert, NULL, &pkey, &x509_cert, &additional_certs);
    PKCS12_free(p12_cert);
    
  
    if( !x509_cert || !pkey ){
        Error("Failed to parse %s ", (const char *)certfile);
        X509_free(x509_cert);
        EVP_PKEY_free(pkey);
        return NULL;
    }
    int ret = ::SSL_CTX_use_certificate(ctx, x509_cert);
    if ( ret != 1) {
        Error("Failed to load cert %s ", (const char *)certfile);
        X509_free(x509_cert);
        EVP_PKEY_free(pkey);
        return NULL;
    }

    if (::SSL_CTX_use_PrivateKey(ctx,pkey) != 1) {
        Error("Failed to load key %s ", (const char *)certfile);
        X509_free(x509_cert);
        EVP_PKEY_free(pkey);
        return NULL;
    }

    // set extra certs
    while (X509* x509 = sk_X509_pop(additional_certs)) {
        if (!ca_store_) {
            ca_store_ = X509_STORE_new();
            SSL_CTX_set_cert_store(ctx, ca_store_);
        }

        X509_STORE_add_cert(ca_store_, x509);
        SSL_CTX_add_client_CA(ctx, x509);
        X509_free(x509);
    }

    EVP_PKEY_free(pkey);
    //X509_free(cert);  -- used by issuer
    sk_X509_free(additional_certs);
    // end set extra certs


    return x509_cert;
}

static X509* initCtxByPem(SSL_CTX *ctx,const char* certfile,STACK_OF(X509) **chain)
{
    return NULL;
}

SSL_CTX* SSLServer_init(const char* certfile,const char* keyfile, const char *issuefile)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx    = NULL;
    X509    *cert   = NULL;

    SSL_load_error_strings();
    ERR_load_crypto_strings();

    OpenSSL_add_ssl_algorithms();

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    SSL_CTX_set_ecdh_auto(ctx, 1);

    if( keyfile ){
        /* Set the key and cert */
        cert  = initCtxByRSA(ctx,certfile,keyfile);

    }
    else{
        cert  = initCtxByP12(ctx,certfile);
    }

    if ( !cert ) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }

    if( ocsp_initsvr(ctx,cert,issuefile) != 0){
        X509_free(cert);
        SSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}

void SSLServer_release()
{
    ocsp_releasesvr();
    EVP_cleanup();
}






/////////////////////// INTERNAL TEST
#define TCP_DEFAULTBACK_LOG 5
int TCPCreate(const char *serv,uint16_t port,int isbind)
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
            Error( "SockAPI_Create	param error for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
            return -1;
        }
    }

    sprintf(portstr,"%d",port);

    int ret =  getaddrinfo(serv, portstr, &hints, &res);
    if( ret != 0) {
        Error( "SockAPI_Create getaddr fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
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
            Error( "SockAPI_Create getsock fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);

            continue;
        }

        int on	= 1;
        if( isbind ){
            if( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0 ){
                Error( "SockAPI_Create reuse port fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
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
        Error( "SockAPI_Create init sock fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
    }while((res = res->ai_next) != NULL);

    freeaddrinfo(ressave);
    if( sockfd >= 0 )
        return sockfd;

    Error( "SockAPI_Create fatal create fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
    return -1;
}



int main(int argc, char **argv)
{
    if( argc < 2 ){
        printf("running param: port issuefile certfile keyfile\n");
        exit(1);
    }

    int port    = 443;
    int index   = 1;
    char *p = argv[1];
    for( ; *p && isdigit(*p); p++ ){
    }
    if( *p == 0 ){
        port    = atoi(argv[1]);
        index ++;
    }
    const char* issuefile   = index < argc ? argv[index++] :NULL;
    const char* certfile    = index < argc ? argv[index++] :NULL;
    const char* keyfile     = index < argc ? argv[index++] :NULL;

    printf("running info: port = %d\n"
            "\t: cert = %s\n"
            "\t: key  = %s\n",
            port,
            certfile,
            keyfile
          );
    if( !certfile )
        exit(1);

    int sock    = TCPCreate(NULL,port,true);

    if( sock < 0 ){
        printf("create listen socket failed,exit\n");
        exit(1);
    }

    SSL_CTX* ctx    = SSLServer_init(certfile, keyfile,issuefile);
    if( !ctx ){
        printf("init ssl ctx failed,exit\n");
        exit(1);
    }

    /* Handle connections */
    while(1) {
        struct sockaddr_in addr;
        uint len = sizeof(addr);
        SSL *ssl;
        const char reply[] = "test\n";

        int client = accept(sock, (struct sockaddr*)&addr, &len);
        if (client < 0) {
            perror("Unable to accept");
            exit(EXIT_FAILURE);
        }

        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        }
        else {
            SSL_write(ssl, reply, strlen(reply));
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client);
    }

    close(sock);
    SSLServer_release();
    SSL_CTX_free(ctx);
    return 0;
}
#endif

