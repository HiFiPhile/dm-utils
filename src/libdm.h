#ifndef _LIB_DM
#define _LIB_DM

#define MAX_CRYPTO_TYPE_NAME_LEN 64
#define MAX_CRYPTO_KEY_ASCII_LEN 129

struct crypt_params {
    const char *name;
    const char *blockdev;
    const char *key;
    const char *cipher;
    const char *extraparams;
};

int list_blk_dev(void);
int create_crypt_blk_dev(struct crypt_params *params);
int delete_blk_dev(const char *name);

#endif
