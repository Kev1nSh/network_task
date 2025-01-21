#ifndef GENERATE_CSR_H
#define GENERATE_CSR_H  

#include "mbedtls/x509_csr.h"
#include "mbedtls/pem.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "generate_csr.c"
#include "printer_helper.h"

int generate_csr(char *csr_buf, size_t csr_buf_size, const char *player_id);

#endif

#include "string.h"

uint8_t key_pem[2048];

int generate_csr(char *csr_buf, size_t csr_buf_size, const char *player_id)
{
    int ret;
    mbedtls_pk_context key;    
    mbedtls_x509write_csr req;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "csr_gen";

    char subject_name[256];

    // Format the subject name with the player ID
    snprintf(subject_name, sizeof(subject_name), "CN=%s,O=csr.chawp.com,C=SE", player_id);
    
    mbedtls_pk_init(&key);
    mbedtls_x509write_csr_init(&req);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);


    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, strlen(pers))) != 0) {
        PRINTFC_CSR("mbedtls_ctr_drbg_seed returned -0x%04x\n", -ret);
        goto exit;
    }

    // Generate a key pair
    if ((ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA))) != 0) {
        PRINTFC_CSR("mbedtls_pk_setup returned -0x%04x\n", -ret);
        goto exit;
    }

    if ((ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctr_drbg, 2048, 65537)) != 0) {
        PRINTFC_CSR("mbedtls_rsa_gen_key returned -0x%04x\n", -ret);
        goto exit;
    }

    mbedtls_x509write_csr_set_md_alg(&req, MBEDTLS_MD_SHA256);
    mbedtls_x509write_csr_set_key(&req, &key);

    if ((ret = mbedtls_x509write_csr_set_subject_name(&req, subject_name)) != 0) {
        PRINTFC_CSR("mbedtls_x509write_csr_set_subject_name returned -0x%04x\n", -ret);
        goto exit;
    }

    if ((ret = mbedtls_x509write_csr_pem(&req, (unsigned char *)csr_buf, csr_buf_size, mbedtls_ctr_drbg_random, &ctr_drbg)) < 0) {
        PRINTFC_CSR("mbedtls_x509write_csr_pem returned -0x%04x\n", -ret);
        goto exit;
    }

    // Write the key to PEM format
    if ((ret = mbedtls_pk_write_key_pem(&key, key_pem, sizeof(key_pem))) != 0) {
        PRINTFC_CLIENT("Failed to write key to PEM format: -0x%04x", -ret);
        goto exit;
    }
    ret = 0; // Success

exit:
    mbedtls_pk_free(&key);
    mbedtls_x509write_csr_free(&req);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);

    return ret;
}