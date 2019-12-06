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

#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <openssl/ocsp.h>

#include <mutex>

#include "ocsp.h"

#define OCSP_CACHE_SIZE 12

#define MAXAGE_SEC (14*24*60*60)
#define JITTER_SEC (60)

//  modify from: //github.com/libressl-portable/openbsd.git
//                  lib/libtls/tls_ocsp.c
static const int  s_ocsp_require_verify   = 0;
static const int  s_ocsp_require_stapling = 1;
static const int  s_ocsp_verify_cert      = 1;
static const int  s_ocsp_verify_time      = 1;

struct ocsp_cache{
    struct ocsp_ctl items[OCSP_CACHE_SIZE];
};

static std::mutex cachelock_;
static struct ocsp_cache s_cache    = {
};


static void
tls_ocsp_reset(struct tls_ocsp *ocsp)
{
    if (ocsp == NULL)
        return;

    X509_free(ocsp->main_cert);
    free(ocsp->ocsp_url);
    memset(ocsp,0,sizeof(*ocsp));
}

static struct ocsp_ctl *ocsp_set(SSL* ssl,SSL_CTX* ctx)
{
    int     i = 0;
    std::lock_guard<std::mutex> guard(cachelock_);
    
    for( i = 0; i < OCSP_CACHE_SIZE && s_cache.items[i].ssl_conn != NULL; i ++ ){
    }
    if( i < OCSP_CACHE_SIZE ){
        s_cache.items[i].ssl_ctx    = ctx;
        s_cache.items[i].ssl_conn   = ssl;
    }    
    return i == OCSP_CACHE_SIZE ? NULL : &s_cache.items[i];
}

static struct ocsp_ctl *ocsp_get(SSL* ssl)
{
    int     i = 0;
    std::lock_guard<std::mutex> guard(cachelock_);
    for( i = 0; i < OCSP_CACHE_SIZE && s_cache.items[i].ssl_conn != ssl; i ++ ){
    }
    return i == OCSP_CACHE_SIZE ? NULL : &s_cache.items[i];
}

static void ocsp_ret(SSL* ssl)
{
    int i = 0;
    std::lock_guard<std::mutex> guard(cachelock_);
    for( i = 0; i < OCSP_CACHE_SIZE && s_cache.items[i].ssl_conn != ssl; i ++ ){
    }
    if( i < OCSP_CACHE_SIZE ){
        tls_ocsp_reset(&s_cache.items[i].ocsp);
        memset(&s_cache.items[i],0,sizeof(struct ocsp_ctl));
    }
}



 /*
 * Parse an RFC 5280 format ASN.1 time string.
 *
 * mode must be:
 * 0 if we expect to parse a time as specified in RFC 5280 for an X509 object.
 * V_ASN1_UTCTIME if we wish to parse an RFC5280 format UTC time.
 * V_ASN1_GENERALIZEDTIME if we wish to parse an RFC5280 format Generalized time.
 *
 * Returns:
 * -1 if the string was invalid.
 * V_ASN1_UTCTIME if the string validated as a UTC time string.
 * V_ASN1_GENERALIZEDTIME if the string validated as a Generalized time string.
 *
 * Fills in *tm with the corresponding time if tm is non NULL.
 */
#define RFC5280 0
#define GENTIME_LENGTH 15
#define UTCTIME_LENGTH 13
#define ATOI2(ar)       ((ar) += 2, ((ar)[-2] - '0') * 10 + ((ar)[-1] - '0'))
static int
ASN1_time_parse(const char *bytes, size_t len, struct tm *tm, int mode)
{
        size_t i;
        int type = 0;
        struct tm ltm;
        struct tm *lt;
        const char *p;

        if (bytes == NULL)
                return (-1);

        /* Constrain to valid lengths. */
        if (len != UTCTIME_LENGTH && len != GENTIME_LENGTH)
                return (-1);

        lt = tm;
        if (lt == NULL) {
                memset(&ltm, 0, sizeof(ltm));
                lt = &ltm;
        }
        /* Timezone is required and must be GMT (Zulu). */
        if (bytes[len - 1] != 'Z')
                return (-1);

        /* Make sure everything else is digits. */
        for (i = 0; i < len - 1; i++) {
                if (isdigit((unsigned char)bytes[i]))
                        continue;
                return (-1);
        }

        /*
         * Validate and convert the time
         */
        p = bytes;
        switch (len) {
        case GENTIME_LENGTH:
                if (mode == V_ASN1_UTCTIME)
                        return (-1);
                lt->tm_year = (ATOI2(p) * 100) - 1900;  /* cc */
                type = V_ASN1_GENERALIZEDTIME;
                /* FALLTHROUGH */
        case UTCTIME_LENGTH:
                if (type == 0) {
                        if (mode == V_ASN1_GENERALIZEDTIME)
                                return (-1);
                        type = V_ASN1_UTCTIME;
                }
                lt->tm_year += ATOI2(p);                /* yy */
                if (type == V_ASN1_UTCTIME) {
                        if (lt->tm_year < 50)
                                lt->tm_year += 100;
                }
                lt->tm_mon = ATOI2(p) - 1;              /* mm */
                if (lt->tm_mon < 0 || lt->tm_mon > 11)
                        return (-1);
                lt->tm_mday = ATOI2(p);                 /* dd */
                if (lt->tm_mday < 1 || lt->tm_mday > 31)
                        return (-1);
                lt->tm_hour = ATOI2(p);                 /* HH */
                if (lt->tm_hour < 0 || lt->tm_hour > 23)
                        return (-1);
                lt->tm_min = ATOI2(p);                  /* MM */
                if (lt->tm_min < 0 || lt->tm_min > 59)
                        return (-1);
                lt->tm_sec = ATOI2(p);                  /* SS */
                /* Leap second 60 is not accepted. Reconsider later? */
                if (lt->tm_sec < 0 || lt->tm_sec > 59)
                        return (-1);
                break;
        default:
                return (-1);
        }

        return (type);
}

static int
tls_ocsp_asn1_parse_time(ASN1_GENERALIZEDTIME *gt, time_t *gt_time)
{
	struct tm tm;

	if (gt == NULL)
		return -1;
	/* RFC 6960 specifies that all times in OCSP must be GENERALIZEDTIME */
	if (ASN1_time_parse((const char *)gt->data, gt->length, &tm,
		V_ASN1_GENERALIZEDTIME) == -1)
		return -1;
	if ((*gt_time = timegm(&tm)) == -1)
		return -1;
	return 0;
}

static int
tls_ocsp_fill_info(struct ocsp_ctl *ctx, int response_status, int cert_status,
    int crl_reason, ASN1_GENERALIZEDTIME *revtime,
    ASN1_GENERALIZEDTIME *thisupd, ASN1_GENERALIZEDTIME *nextupd)
{
	struct tls_ocsp_result *info = &ctx->ocsp.ocsp_result;

	memset(info,0,sizeof(*info));

	info->response_status = response_status;
	info->cert_status = cert_status;
	info->crl_reason = crl_reason;
	if (info->response_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		info->result_msg =
		    OCSP_response_status_str(info->response_status);
	} else if (info->cert_status != V_OCSP_CERTSTATUS_REVOKED) {
		info->result_msg = OCSP_cert_status_str(info->cert_status);
	} else {
		info->result_msg = OCSP_crl_reason_str(info->crl_reason);
	}
	info->revocation_time = info->this_update = info->next_update = -1;
	if (revtime != NULL &&
	    tls_ocsp_asn1_parse_time( revtime, &info->revocation_time) != 0) {
		tls_set_error(ctx,
		    "unable to parse revocation time in OCSP reply");
		goto err;
	}
	if (thisupd != NULL &&
	    tls_ocsp_asn1_parse_time(thisupd, &info->this_update) != 0) {
		tls_set_error(ctx,
		    "unable to parse this update time in OCSP reply");
		goto err;
	}
	if (nextupd != NULL &&
	    tls_ocsp_asn1_parse_time( nextupd, &info->next_update) != 0) {
		tls_set_error(ctx,
		    "unable to parse next update time in OCSP reply");
		goto err;
	}
	return 0;

 err:
	free(info);
	return -1;
}

static OCSP_CERTID *
tls_ocsp_get_certid(X509 *main_cert, STACK_OF(X509) *extra_certs,
    SSL_CTX *ssl_ctx)
{
    X509_NAME *issuer_name;
    X509 *issuer;
    X509_STORE_CTX *storectx = NULL;
    X509_OBJECT *tmpobj     = NULL;
    OCSP_CERTID *cid = NULL;
    X509_STORE *store;

    if ((issuer_name = X509_get_issuer_name(main_cert)) == NULL)
        return NULL;

    if (extra_certs != NULL) {
        issuer = X509_find_by_subject(extra_certs, issuer_name);
        if (issuer != NULL)
            return OCSP_cert_to_id(NULL, main_cert, issuer);
    }

    if ((store = SSL_CTX_get_cert_store(ssl_ctx)) == NULL)
        return NULL;
    tmpobj  = X509_OBJECT_new();
    if( tmpobj == NULL ){
        X509_STORE_CTX_free(storectx);
        return NULL;
    }

    if (X509_STORE_CTX_init(storectx, store, main_cert, extra_certs) != 1){
        X509_OBJECT_free(tmpobj);
        X509_STORE_CTX_free(storectx);
        return NULL;
    }



    if (X509_STORE_CTX_get_by_subject(storectx, X509_LU_X509, issuer_name,
                tmpobj) == 1) {
        cid = OCSP_cert_to_id(NULL, main_cert, X509_OBJECT_get0_X509(tmpobj));
        X509_OBJECT_free(tmpobj);
    }
    X509_OBJECT_free(tmpobj);

    X509_STORE_CTX_cleanup(storectx);
    X509_STORE_CTX_free(storectx);
    return cid;
}

static struct tls_ocsp *
tls_ocsp_setup_from_peer(struct ocsp_ctl *ctx)
{
	struct tls_ocsp *ocsp = &ctx->ocsp;
	STACK_OF(OPENSSL_STRING) *ocsp_urls = NULL;

	/* steal state from ctx struct */
	ocsp->main_cert = SSL_get_peer_certificate(ctx->ssl_conn);
	ocsp->extra_certs = SSL_get_peer_cert_chain(ctx->ssl_conn);
	if (ocsp->main_cert == NULL) {
		tls_set_errorx(ctx, "no peer certificate for OCSP");
		goto err;
	}

	ocsp_urls = X509_get1_ocsp(ocsp->main_cert);
	if (ocsp_urls == NULL) {
		tls_set_errorx(ctx, "no OCSP URLs in peer certificate");
		goto err;
	}

	ocsp->ocsp_url = strdup(sk_OPENSSL_STRING_value(ocsp_urls, 0));
	if (ocsp->ocsp_url == NULL) {
		tls_set_errorx(ctx, "out of memory");
		goto err;
	}

	X509_email_free(ocsp_urls);
	return ocsp;

 err:
	tls_ocsp_reset(ocsp);
	X509_email_free(ocsp_urls);
	return NULL;
}

static int
tls_ocsp_verify_response(struct ocsp_ctl *ctx, OCSP_RESPONSE *resp)
{
	OCSP_BASICRESP *br = NULL;
	ASN1_GENERALIZEDTIME *revtime = NULL, *thisupd = NULL, *nextupd = NULL;
	OCSP_CERTID *cid = NULL;
	STACK_OF(X509) *combined = NULL;
	int response_status=0, cert_status=0, crl_reason=0;
	int ret = -1;
	unsigned long flags;

	if ((br = OCSP_response_get1_basic(resp)) == NULL) {
		tls_set_errorx(ctx, "cannot load ocsp reply");
		goto err;
	}

	/*
	 * Skip validation of 'extra_certs' as this should be done
	 * already as part of main handshake.
	 */
	flags = OCSP_TRUSTOTHER;

	/* now verify */
	if (OCSP_basic_verify(br, ctx->ocsp.extra_certs,
                    SSL_CTX_get_cert_store(ctx->ssl_ctx), flags) != 1) {
            tls_set_error(ctx, "ocsp verify failed");
            if( s_ocsp_require_verify )
                goto err;
        }

	/* signature OK, look inside */
	response_status = OCSP_response_status(resp);
	if (response_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		tls_set_errorx(ctx, "ocsp verify failed: response - %s",
		    OCSP_response_status_str(response_status));
		goto err;
	}

	cid = tls_ocsp_get_certid(ctx->ocsp.main_cert,
	    ctx->ocsp.extra_certs, ctx->ssl_ctx);
	if (cid == NULL) {
		tls_set_errorx(ctx, "ocsp verify failed: no issuer cert");
		goto err;
	}

	if (OCSP_resp_find_status(br, cid, &cert_status, &crl_reason,
	    &revtime, &thisupd, &nextupd) != 1) {
		tls_set_errorx(ctx, "ocsp verify failed: no result for cert");
		goto err;
	}

	if (OCSP_check_validity(thisupd, nextupd, JITTER_SEC,
	    MAXAGE_SEC) != 1) {
		tls_set_errorx(ctx,
		    "ocsp verify failed: ocsp response not current");
		goto err;
	}

	if (tls_ocsp_fill_info(ctx, response_status, cert_status,
	    crl_reason, revtime, thisupd, nextupd) != 0)
		goto err;

	/* finally can look at status */
	if (cert_status != V_OCSP_CERTSTATUS_GOOD && cert_status !=
	    V_OCSP_CERTSTATUS_UNKNOWN) {
		tls_set_errorx(ctx, "ocsp verify failed: revoked cert - %s",
			       OCSP_crl_reason_str(crl_reason));
		goto err;
	}
	ret = 0;

 err:
	sk_X509_free(combined);
	OCSP_CERTID_free(cid);
	OCSP_BASICRESP_free(br);
	return ret;
}

#if 0
static int curl_cb_ssl_verify(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	struct http_ctx *ctx;
	X509 *cert;
	int err, depth;
	char buf[256];
	X509_NAME *name;
	const char *err_str;
	SSL *ssl;
	SSL_CTX *ssl_ctx;

	ssl = X509_STORE_CTX_get_ex_data(x509_ctx,
					 SSL_get_ex_data_X509_STORE_CTX_idx());
	ssl_ctx = SSL_get_SSL_CTX(ssl);
	ctx = SSL_CTX_get_app_data(ssl_ctx);

	wpa_printf(MSG_DEBUG, "curl_cb_ssl_verify, preverify_ok: %d",
		   preverify_ok);

	err = X509_STORE_CTX_get_error(x509_ctx);
	err_str = X509_verify_cert_error_string(err);
	depth = X509_STORE_CTX_get_error_depth(x509_ctx);
	cert = X509_STORE_CTX_get_current_cert(x509_ctx);
	if (!cert) {
		wpa_printf(MSG_INFO, "No server certificate available");
		ctx->last_err = "No server certificate available";
		return 0;
	}

	if (depth == 0)
		ctx->peer_cert = cert;
	else if (depth == 1)
		ctx->peer_issuer = cert;
	else if (depth == 2)
		ctx->peer_issuer_issuer = cert;

	name = X509_get_subject_name(cert);
	X509_NAME_oneline(name, buf, sizeof(buf));
	wpa_printf(MSG_INFO, "Server certificate chain - depth=%d err=%d (%s) subject=%s",
		   depth, err, err_str, buf);
	debug_dump_cert("Server certificate chain - certificate", cert);

	if (depth == 0 && preverify_ok && validate_server_cert(ctx, cert) < 0)
		return 0;

#ifdef OPENSSL_IS_BORINGSSL
	if (depth == 0 && ctx->ocsp != NO_OCSP && preverify_ok) {
		enum ocsp_result res;

		res = check_ocsp_resp(ssl_ctx, ssl, cert, ctx->peer_issuer,
				      ctx->peer_issuer_issuer);
		if (res == OCSP_REVOKED) {
			preverify_ok = 0;
			wpa_printf(MSG_INFO, "OCSP: certificate revoked");
			if (err == X509_V_OK)
				X509_STORE_CTX_set_error(
					x509_ctx, X509_V_ERR_CERT_REVOKED);
		} else if (res != OCSP_GOOD && (ctx->ocsp == MANDATORY_OCSP)) {
			preverify_ok = 0;
			wpa_printf(MSG_INFO,
				   "OCSP: bad certificate status response");
		}
	}
#endif /* OPENSSL_IS_BORINGSSL */

	if (!preverify_ok)
		ctx->last_err = "TLS validation failed";

	return preverify_ok;
}
#endif

#define failf tls_set_errorx
#define CURLE_OK    1
#define CURLE_SSL_INVALIDCERTSTATUS 0
static int ocsp_resp_cb(SSL *s, void *arg)
{
//static CURLcode verifystatus(struct connectdata *conn,
//                             struct ssl_connect_data *connssl)

  int i, ocsp_status;
  const unsigned char *p;
  int result = CURLE_OK;

  struct ocsp_ctl *ctx    = ocsp_get(s);
  OCSP_RESPONSE *rsp = NULL;
  OCSP_BASICRESP *br = NULL;
  X509_STORE     *st = NULL;
  STACK_OF(X509) *ch = NULL;

  long len = SSL_get_tlsext_status_ocsp_resp(s, &p);

  if(!p) {
    failf(data, "No OCSP response received");
    result = CURLE_SSL_INVALIDCERTSTATUS;
    goto end;
  }

  rsp = d2i_OCSP_RESPONSE(NULL, &p, len);
  if(!rsp) {
    failf(data, "Invalid OCSP response");
    result = CURLE_SSL_INVALIDCERTSTATUS;
    goto end;
  }

  ocsp_status = OCSP_response_status(rsp);
  if(ocsp_status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
    failf(data, "Invalid OCSP response status: %s (%d)",
          OCSP_response_status_str(ocsp_status), ocsp_status);
    result = CURLE_SSL_INVALIDCERTSTATUS;
    goto end;
  }

  br = OCSP_response_get1_basic(rsp);
  if(!br) {
    failf(data, "Invalid OCSP response");
    result = CURLE_SSL_INVALIDCERTSTATUS;
    goto end;
  }

  ch = SSL_get_peer_cert_chain(s);
  st = SSL_CTX_get_cert_store(ctx->ssl_ctx);

   tls_set_debug(data, "cert chain num:%p/%d cert store:%p ",
           ch,sk_X509_num(ch),st);

#if ((OPENSSL_VERSION_NUMBER <= 0x1000201fL) /* Fixed after 1.0.2a */ || \
     defined(LIBRESSL_VERSION_NUMBER))
  /* The authorized responder cert in the OCSP response MUST be signed by the
     peer cert's issuer (see RFC6960 section 4.2.2.2). If that's a root cert,
     no problem, but if it's an intermediate cert OpenSSL has a bug where it
     expects this issuer to be present in the chain embedded in the OCSP
     response. So we add it if necessary. */

  /* First make sure the peer cert chain includes both a peer and an issuer,
     and the OCSP response contains a responder cert. */
  if(sk_X509_num(ch) >= 2 && sk_X509_num(br->certs) >= 1) {
    X509 *responder = sk_X509_value(br->certs, sk_X509_num(br->certs) - 1);

    /* Find issuer of responder cert and add it to the OCSP response chain */
    for(i = 0; i < sk_X509_num(ch); i++) {
      X509 *issuer = sk_X509_value(ch, i);
      tls_set_debug(data, "push issuer %d:%p",i,issuer);

      if(X509_check_issued(issuer, responder) == X509_V_OK) {
        if(!OCSP_basic_add1_cert(br, issuer)) {
          failf(data, "Could not add issuer cert to OCSP response");
          result = CURLE_SSL_INVALIDCERTSTATUS;
          goto end;
        }
      }
    }
  }
#endif

#if 0
  if(OCSP_basic_verify(br, ch, st, OCSP_TRUSTOTHER) <= 0) {
    failf(data, "OCSP response verification failed");
    result = CURLE_SSL_INVALIDCERTSTATUS;
    goto end;
  }
#endif

  for(i = 0; i < OCSP_resp_count(br); i++) {
    int cert_status, crl_reason;
    OCSP_SINGLERESP *single = NULL;

    ASN1_GENERALIZEDTIME *rev, *thisupd, *nextupd;

    single = OCSP_resp_get0(br, i);
    if(!single)
      continue;

    cert_status = OCSP_single_get0_status(single, &crl_reason, &rev,
                                          &thisupd, &nextupd);

    if(!OCSP_check_validity(thisupd, nextupd, 300L, -1L)) {
      failf(data, "OCSP response has expired");
      result = CURLE_SSL_INVALIDCERTSTATUS;
      goto end;
    }

    tls_set_debug(data, "SSL certificate status: %s (%d)\n",
          OCSP_cert_status_str(cert_status), cert_status);

    switch(cert_status) {
      case V_OCSP_CERTSTATUS_GOOD:
        break;

      case V_OCSP_CERTSTATUS_REVOKED:
        result = CURLE_SSL_INVALIDCERTSTATUS;

        failf(data, "SSL certificate revocation reason: %s (%d)",
              OCSP_crl_reason_str(crl_reason), crl_reason);
        goto end;

      case V_OCSP_CERTSTATUS_UNKNOWN:
        result = CURLE_SSL_INVALIDCERTSTATUS;
        goto end;
    }
  }

end:
  if(br) OCSP_BASICRESP_free(br);
  OCSP_RESPONSE_free(rsp);

  return result;

#if 0
	struct ocsp_ctl *ctx = (struct ocsp_ctl *)arg;
	const unsigned char *p;
	int len, status, reason, res;
	OCSP_RESPONSE *rsp;
	OCSP_BASICRESP *basic;
	OCSP_CERTID *id;
	ASN1_GENERALIZEDTIME *produced_at, *this_update, *next_update;
	X509_STORE *store;
	STACK_OF(X509) *certs = NULL;

	len = SSL_get_tlsext_status_ocsp_resp(s, &p);
        if (!p) {
            if (s_ocsp_require_stapling) {
                tls_set_errorx(ctx, "no stapled OCSP response provided");
                return 0;
            }
            return 1;
        }

	tls_set_debug(MSG_DEBUG, "OpenSSL: OCSP response: %d", len);

	rsp = d2i_OCSP_RESPONSE(NULL, &p, len);
        if (!rsp) {
            tls_set_error(MSG_INFO, "OpenSSL: Failed to parse OCSP response");
            return 0;
        }

	//ocsp_debug_print_resp(rsp);

	status = OCSP_response_status(rsp);
	if (status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
		tls_set_error(MSG_INFO, "OpenSSL: OCSP responder error %d (%s)",
			   status, OCSP_response_status_str(status));
	    OCSP_RESPONSE_free(rsp);
		return 0;
	}

	basic = OCSP_response_get1_basic(rsp);
	if (!basic) {
		tls_set_error(MSG_INFO, "OpenSSL: Could not find BasicOCSPResponse");
	    OCSP_RESPONSE_free(rsp);
		return 0;
	}

	store = SSL_CTX_get_cert_store(ctx->ssl_ctx);
	if (ctx->peer_issuer) {
		tls_set_debug(MSG_DEBUG, "OpenSSL: Add issuer");
		//tls_set_error("OpenSSL: Issuer certificate",ctx->peer_issuer);

		if (X509_STORE_add_cert(store, ctx->peer_issuer) != 1) {
			tls_set_error(__func__,
					"OpenSSL: Could not add issuer to certificate store");
		}
		certs = sk_X509_new_null();
		if (certs) {
			X509 *cert;
			cert = X509_dup(ctx->peer_issuer);
			if (cert && !sk_X509_push(certs, cert)) {
				tls_set_error(
					__func__,
					"OpenSSL: Could not add issuer to OCSP responder trust store");
				X509_free(cert);
				sk_X509_free(certs);
				certs = NULL;
			}
			if (certs && ctx->peer_issuer_issuer) {
				cert = X509_dup(ctx->peer_issuer_issuer);
				if (cert && !sk_X509_push(certs, cert)) {
					tls_set_error(
						__func__,
						"OpenSSL: Could not add issuer's issuer to OCSP responder trust store");
					X509_free(cert);
				}
			}
		}
	}

	status = OCSP_basic_verify(basic, certs, store, OCSP_TRUSTOTHER);
	sk_X509_pop_free(certs, X509_free);
	if (status <= 0) {
		tls_set_error(__func__,
				"OpenSSL: OCSP response failed verification");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		return 0;
	}

	wpa_printf(MSG_DEBUG, "OpenSSL: OCSP response verification succeeded");

	if (!ctx->peer_cert) {
		tls_set_error(MSG_DEBUG, "OpenSSL: Peer certificate not available for OCSP status check");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		return 0;
	}

	if (!ctx->peer_issuer) {
		tls_set_error(MSG_DEBUG, "OpenSSL: Peer issuer certificate not available for OCSP status check");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		return 0;
	}

	id = OCSP_cert_to_id(EVP_sha256(), ctx->peer_cert, ctx->peer_issuer);
	if (!id) {
		tls_set_error(MSG_DEBUG,
			   "OpenSSL: Could not create OCSP certificate identifier (SHA256)");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		return 0;
	}

	res = OCSP_resp_find_status(basic, id, &status, &reason, &produced_at,
				    &this_update, &next_update);
	if (!res) {
		id = OCSP_cert_to_id(NULL, ctx->peer_cert, ctx->peer_issuer);
		if (!id) {
			tls_set_error(MSG_DEBUG,
				   "OpenSSL: Could not create OCSP certificate identifier (SHA1)");
			OCSP_BASICRESP_free(basic);
			OCSP_RESPONSE_free(rsp);
			return 0;
		}

		res = OCSP_resp_find_status(basic, id, &status, &reason,
					    &produced_at, &this_update,
					    &next_update);
	}

	if (!res) {
		tls_set_error(MSG_INFO, "OpenSSL: Could not find current server certificate from OCSP response%s",
			   (ctx->ocsp == MANDATORY_OCSP) ? "" :
			   " (OCSP not required)");
		OCSP_CERTID_free(id);
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		return s_ocsp_require_stapling ? 0 : 1;
	}
	OCSP_CERTID_free(id);

	if (!OCSP_check_validity(this_update, next_update, 5 * 60, -1)) {
		tls_show_errors(__func__, "OpenSSL: OCSP status times invalid");
		OCSP_BASICRESP_free(basic);
		OCSP_RESPONSE_free(rsp);
		return 0;
	}

	OCSP_BASICRESP_free(basic);
	OCSP_RESPONSE_free(rsp);

	tls_set_error(MSG_DEBUG, "OpenSSL: OCSP status for server certificate: %s",
		   OCSP_cert_status_str(status));

	if (status == V_OCSP_CERTSTATUS_GOOD)
		return 1;
	if (status == V_OCSP_CERTSTATUS_REVOKED) {
		return 0;
	}
	if (ctx->ocsp == MANDATORY_OCSP) {
		tls_set_error(MSG_DEBUG, "OpenSSL: OCSP status unknown, but OCSP required");
		return 0;
	}
	tls_set_error(MSG_DEBUG, "OpenSSL: OCSP status unknown, but OCSP was not required, so allow connection to continue");
	return 1;
#endif
}

/*
 * Process a raw OCSP response from an OCSP server request.
 * OCSP details can then be retrieved with tls_peer_ocsp_* functions.
 * returns 0 if certificate ok, -1 otherwise.
 */
static int
tls_ocsp_process_response_internal(struct ocsp_ctl *ctx, const unsigned char *response,
    size_t size)
{
	int ret;
	OCSP_RESPONSE *resp;

	resp = d2i_OCSP_RESPONSE(NULL, &response, size);
	if (resp == NULL) {
		tls_ocsp_reset(&ctx->ocsp);
		tls_set_error(ctx, "unable to parse OCSP response");
		return -1;
	}
	ret = tls_ocsp_verify_response(ctx, resp);
	OCSP_RESPONSE_free(resp);
	return ret;
}

/* TLS handshake verification callback for stapled requests */
static int
tls_ocsp_verify_cb(SSL *ssl, void *arg)
{
	const unsigned char *raw = NULL;
	int size, res           = -1;
	struct ocsp_ctl *ctx    = ocsp_get(ssl);

	if ( ctx == NULL)
		return -1;

	size = SSL_get_tlsext_status_ocsp_resp(ssl, &raw);
	if (size <= 0) {
		if (s_ocsp_require_stapling) {
			tls_set_errorx(ctx, "no stapled OCSP response provided");
			return 0;
		}
		return 1;
	}

	tls_ocsp_reset(&ctx->ocsp);
	if (tls_ocsp_setup_from_peer(ctx) == NULL)
		return 0;

	if (s_ocsp_verify_cert == 0 || s_ocsp_verify_time == 0)
		return 1;

	res = tls_ocsp_process_response_internal(ctx, raw, size);
        if( res == 0 )
            tls_set_info(ctx,"ocsp check OK");
        else
            tls_set_error(ctx,"ocsp check Error");

	return (res == 0) ? 1 : 0;
}

int ocsp_initctx(SSL_CTX* ctx)
{
    if (SSL_CTX_set_tlsext_status_cb(ctx, tls_ocsp_verify_cb) != 1) {
        tls_set_errorx(ctx, "ssl OCSP verification setup failure");
        return -1;
    }

    return 0;
}

int ocsp_initssl(SSL* ssl,SSL_CTX* ctx)
{
    struct ocsp_ctl* ctl    = ocsp_set(ssl,ctx);

    if( !ctl ){
        tls_set_errorx(ctx, "ocsp alloc record failed");
        return -1;
    }

    if (SSL_set_tlsext_status_type(ssl, TLSEXT_STATUSTYPE_ocsp) != 1) {
        tls_set_errorx(ctx, "ssl OCSP extension setup failure");
        ocsp_ret(ssl);
        return -1;
    }

    return 0;
}




void ocsp_releasessl(SSL* ssl){
    ocsp_ret(ssl);
}

