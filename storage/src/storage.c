#include "storgae.h"
#include <flashdb.h>

/* KVDB object */
static struct fdb_kvdb kvdb = {0};

/* Function Declarations */
void storage_init(void)
{
    fdb_kvdb_init(&kvdb, "device_param", NULL, NULL, NULL);
}

int storage_read(char *key, uint8_t *buf, uint32_t size)
{
    struct fdb_blob blob;
    if (!key || !buf || size == 0) {
        return -1; // Invalid arguments
    }

    fdb_kv_get_blob(&kvdb, key, fdb_blob_make(&blob, buf, size));
    if (blob.saved.len > 0) {
        size = blob.saved.len;
        return 0; // Success
    } else {
        return -1; // Parameter not found
    }
}

int storage_write(char *key, uint8_t *buf, uint32_t size)
{
    struct fdb_blob blob;
    if (!key || !buf || size == 0) {
        return -1; // Invalid arguments
    }
    
    return fdb_kv_set_blob(&kvdb, key, fdb_blob_make(&blob, buf, size));
}