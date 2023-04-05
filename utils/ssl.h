#pragma once

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

// init SSL_CTX with file path

static SSL_CTX *get_client_context(const char *ca_pem,
                                   const char *cert_pem,
                                   const char *key_pem) {
    SSL_CTX *ctx;

    /* Create a generic context */
    if (!(ctx = SSL_CTX_new(SSLv23_client_method()))) {
        fprintf(stderr, "Cannot create a client context\n");
        return NULL;
    }

    /* Load the client's CA file location */
    if (SSL_CTX_load_verify_locations(ctx, ca_pem, NULL) != 1) {
        fprintf(stderr, "Cannot load client's CA file\n");
        goto fail;
    }

    /* Load the client's certificate */
    if (SSL_CTX_use_certificate_file(ctx, cert_pem, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "Cannot load client's certificate file\n");
        goto fail;
    }

    /* Load the client's key */
    if (SSL_CTX_use_PrivateKey_file(ctx, key_pem, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "Cannot load client's key file\n");
        goto fail;
    }

    /* Verify that the client's certificate and the key match */
    if (SSL_CTX_check_private_key(ctx) != 1) {
        fprintf(stderr, "Client's certificate and key don't match\n");
        goto fail;
    }

    /* We won't handle incomplete read/writes due to renegotiation */
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

    /* Specify that we need to verify the server's certificate */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    /* We accept only certificates signed only by the CA himself */
    SSL_CTX_set_verify_depth(ctx, 1);

    /* Done, return the context */
    return ctx;

    fail:
    SSL_CTX_free(ctx);
    return NULL;
}

static SSL_CTX *get_server_context(const char *ca_pem,
                                   const char *cert_pem,
                                   const char *key_pem) {
    SSL_CTX *ctx;

    /* Get a default context */
    if (!(ctx = SSL_CTX_new(SSLv23_server_method()))) {
        LOG(ERROR) <<   "SSL_CTX_new failed";
        return NULL;
    }

    /* Set the CA file location for the server */
    if (SSL_CTX_load_verify_locations(ctx, ca_pem, NULL) != 1) {
        LOG(ERROR) <<  "Could not set the CA file location";
        goto fail;
    }

    /* Load the client's CA file location as well */
    SSL_CTX_set_client_CA_list(ctx, SSL_load_client_CA_file(ca_pem));

    /* Set the server's certificate signed by the CA */
    if (SSL_CTX_use_certificate_file(ctx, cert_pem, SSL_FILETYPE_PEM) != 1) {
        LOG(ERROR) <<  "Could not set the server's certificate";
        goto fail;
    }

    /* Set the server's key for the above certificate */
    if (SSL_CTX_use_PrivateKey_file(ctx, key_pem, SSL_FILETYPE_PEM) != 1) {
        LOG(ERROR) <<  "Could not set the server's key";
        goto fail;
    }

    /* We've loaded both certificate and the key, check if they match */
    if (SSL_CTX_check_private_key(ctx) != 1) {
        LOG(ERROR) <<  "Server's certificate and the key don't match";
        goto fail;
    }

    /* We won't handle incomplete read/writes due to renegotiation */
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

    /* Specify that we need to verify the client as well */
    SSL_CTX_set_verify(ctx,
                       SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       NULL);

    /* We accept only certificates signed only by the CA himself */
    SSL_CTX_set_verify_depth(ctx, 1);

    /* Done, return the context */
    return ctx;

    fail:
    SSL_CTX_free(ctx);
    return NULL;
}

#ifdef __cplusplus
}  // extern "C"
#endif