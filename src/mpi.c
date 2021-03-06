#include "mpi.h"
#include "deserialize.h"
#include "serialize.h"

void otr_mpi_init(otr_mpi_t mpi) {
  mpi->len = 0;
  mpi->data = NULL;
}

void otr_mpi_free(otr_mpi_t mpi) {
  free(mpi->data);
  mpi->data = NULL;
}

void otr_mpi_set(otr_mpi_t dst, const uint8_t *src, size_t len) {
  if (src == NULL || len == 0) {
    dst->len = 0;
    dst->data = NULL;
    return;
  }

  dst->len = len;
  dst->data = malloc(dst->len);
  if (!dst->data) {
    return; // should it be an error?
  }

  memcpy(dst->data, src, dst->len);
}

void otr_mpi_copy(otr_mpi_t dst, const otr_mpi_t src) {
  otr_mpi_set(dst, src->data, src->len);
}

static bool otr_mpi_read_len(otr_mpi_t dst, const uint8_t *src, size_t src_len,
                             size_t *read) {
  size_t r = 0;
  if (deserialize_uint32(&dst->len, src, src_len, &r)) {
    return false;
  }

  if (read != NULL) {
    *read = r;
  }

  if (dst->len > src_len - r) {
    return false;
  }

  return true;
}

otr4_err_t otr_mpi_deserialize(otr_mpi_t dst, const uint8_t *src,
                               size_t src_len, size_t *read) {
  if (!otr_mpi_read_len(dst, src, src_len, read)) {
    return OTR4_ERROR;
  }

  if (dst->len == 0) {
    dst->data = NULL;
    return OTR4_SUCCESS;
  }

  dst->data = malloc(dst->len);
  if (dst->data == NULL) {
    return OTR4_ERROR;
  }

  memcpy(dst->data, src + *read, dst->len);

  if (read != NULL) {
    *read += dst->len;
  }
  return OTR4_SUCCESS;
}

otr4_err_t otr_mpi_deserialize_no_copy(otr_mpi_t dst, const uint8_t *src,
                                       size_t src_len, size_t *read) {
  if (!otr_mpi_read_len(dst, src, src_len, read)) {
    return OTR4_ERROR;
  }

  if (dst->len == 0) {
    dst->data = NULL;
    return OTR4_SUCCESS;
  }
  /* points to original buffer without copying */
  dst->data = (uint8_t *)src + *read;
  return OTR4_SUCCESS;
}

size_t otr_mpi_memcpy(uint8_t *dst, const otr_mpi_t mpi) {
  memcpy(dst, mpi->data, mpi->len);
  return mpi->len;
}
