/*
 * Copyright (c) 2013-2017 Red Hat.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of Red Hat nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RED HAT AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL RED HAT OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <lzma.h>
#include "pmapi.h"
#include "impl.h"

#define XZ_HEADER_MAGIC     "\xfd" "7zXZ\0"
#define XZ_HEADER_MAGIC_LEN 6
#define XZ_FOOTER_MAGIC     "YZ"
#define XZ_FOOTER_MAGIC_LEN 2

/* A block cache. Implemented as a very simple LRU list with a fixed depth. */
typedef struct blkcache_stats {
    size_t hits;
    size_t misses;
} blkcache_stats;


typedef struct blkcache {
    size_t maxdepth;
    struct block *blocks;
    blkcache_stats stats;
} blkcache;

struct block {
    uint64_t start;
    uint64_t size;
    char *data;
};

/* The file handle */
typedef struct xzfile {
    FILE *f;
    int fd;
    lzma_index *idx;
    size_t nr_streams;
    size_t nr_blocks;
    blkcache *cache;
  __uint64_t max_uncompressed_block_size;
} xzfile;

static void
xz_debug(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

blkcache *
new_blkcache(size_t maxdepth)
{
  blkcache *c;

  c = malloc(sizeof *c);
  if (!c) {
    fprintf(stderr, "malloc: %m");
    return NULL;
  }

  c->blocks = calloc(maxdepth, sizeof(struct block));
  if (!c->blocks) {
    fprintf(stderr, "calloc: %m");
    free(c);
    return NULL;
  }
  c->maxdepth = maxdepth;
  c->stats.hits = c->stats.misses = 0;

  return c;
}

void
free_blkcache(blkcache *c)
{
  size_t i;

  for (i = 0; i < c->maxdepth; ++i)
    free(c->blocks[i].data);
  free(c->blocks);
  free(c);
}

char *
get_block(blkcache *c, uint64_t offset, uint64_t *start, uint64_t *size)
{
  size_t i;
  struct block tmp;

  for (i = 0; i < c->maxdepth; ++i) {
    if (c->blocks[i].data != NULL &&
        c->blocks[i].start <= offset &&
        offset < c->blocks[i].start + c->blocks[i].size) {
      /* This block is now most recently used, so put it at the start. */
      if (i > 0) {
        tmp = c->blocks[0];
        c->blocks[0] = c->blocks[i];
        c->blocks[i] = tmp;
      }

      c->stats.hits++;
      *start = c->blocks[0].start;
      *size = c->blocks[0].size;
      return c->blocks[0].data;
    }
  }

  c->stats.misses++;
  return NULL;
}

int
put_block(blkcache *c, uint64_t start, uint64_t size, char *data)
{
  size_t i;

  /* Eject the least recently used block. */
  i = c->maxdepth-1;
  if (c->blocks[i].data != NULL)
    free(c->blocks[i].data);

  for (; i >= 1; --i)
    c->blocks[i] = c->blocks[i-1];

  /* The new block is most recently used, so it goes at the start. */
  c->blocks[0].start = start;
  c->blocks[0].size = size;
  c->blocks[0].data = data;

  return 0;
}

void
blkcache_get_stats(blkcache *c, blkcache_stats *ret)
{
  memcpy(ret, &c->stats, sizeof(c->stats));
}

static int
check_header_magic(FILE *f)
{
  char buf[XZ_HEADER_MAGIC_LEN];

  if (fseek(f, 0, SEEK_SET) == -1)
      return 1; /* error */
  if (fread(buf, 1, sizeof(buf), f) != sizeof(buf))
      return 1; /* error */
  if (memcmp(buf, XZ_HEADER_MAGIC, sizeof(buf)) != 0)
      return 1; /* error */

  return 0; /* ok */
}

/* For explanation of this function, see src/xz/list.c:parse_indexes
 * in the xz sources.
 */
static lzma_index *
parse_indexes(FILE *f, size_t *nr_streams)
{
  lzma_ret r;
  off_t index_size;
  int pos;
  int sts;
  uint8_t footer[LZMA_STREAM_HEADER_SIZE];
  uint8_t header[LZMA_STREAM_HEADER_SIZE];
  lzma_stream_flags footer_flags;
  lzma_stream_flags header_flags;
  lzma_stream strm = LZMA_STREAM_INIT;
  ssize_t n;
  lzma_index *combined_index = NULL;
  lzma_index *this_index = NULL;
  lzma_vli stream_padding = 0;

  *nr_streams = 0;

  /* Check file size is a multiple of 4 bytes. */
  sts = fseek(f, 0, SEEK_END);
  if (sts != 0)
      goto err;
  pos = ftell(f);
  if (pos == -1)
      goto err;
  if ((pos & 3) != 0)
      goto err;

  /* Jump backwards through the file identifying each stream. */
  while (pos > 0) {
    if (pos < LZMA_STREAM_HEADER_SIZE)
	goto err;

    if (fseek(f, -LZMA_STREAM_HEADER_SIZE, SEEK_CUR) != 0) {
      fprintf(stderr, "%s: fseek: %m", __func__);
      goto err;
    }
    if (fread(footer, 1, LZMA_STREAM_HEADER_SIZE, f) != LZMA_STREAM_HEADER_SIZE) {
      fprintf(stderr, "%s: read stream footer: %m", __func__);
      goto err;
    }
    /* Skip stream padding. */
    if (footer[8] == 0 && footer[9] == 0 &&
        footer[10] == 0 && footer[11] == 0) {
      stream_padding += 4;
      pos -= 4;
      continue;
    }

    pos -= LZMA_STREAM_HEADER_SIZE;
    (*nr_streams)++;

    xz_debug("decode stream footer at pos = %d", pos);

    /* Does the stream footer look reasonable? */
    r = lzma_stream_footer_decode(&footer_flags, footer);
    if (r != LZMA_OK) {
      fprintf(stderr, "parse_indexes: invalid stream footer (error %d)", r);
      goto err;
    }
    xz_debug("backward_size = %lu",
                  (unsigned long) footer_flags.backward_size);
    index_size = footer_flags.backward_size;
    if (pos < index_size + LZMA_STREAM_HEADER_SIZE) {
      fprintf(stderr, "%s: invalid stream footer", __func__);
      goto err;
    }

    pos -= index_size;
    xz_debug("decode index at pos = %d", pos);

    /* Seek backwards to the index of this stream. */
    if (fseek(f, pos, SEEK_SET) != 0) {
      fprintf(stderr, "%s: fseek: %m", __func__);
      goto err;
    }

    /* Decode the index. */
    r = lzma_index_decoder(&strm, &this_index, UINT64_MAX);
    if (r != LZMA_OK) {
      fprintf(stderr, "%s: invalid stream index (error %d)", __func__, r);
      goto err;
    }

    do {
      uint8_t buf[BUFSIZ];

      strm.avail_in = index_size;
      if (strm.avail_in > BUFSIZ)
        strm.avail_in = BUFSIZ;

      n = fread(&buf, 1, strm.avail_in, f);
      if (n != strm.avail_in) {
        fprintf(stderr, "read: %m");
        goto err;
      }
      index_size -= strm.avail_in;

      strm.next_in = buf;
      r = lzma_code(&strm, LZMA_RUN);
    } while (r == LZMA_OK);

    if (r != LZMA_STREAM_END) {
      fprintf(stderr, "%s: could not parse index (error %d)",
                    __func__, r);
      goto err;
    }

    pos -= lzma_index_total_size(this_index) + LZMA_STREAM_HEADER_SIZE;

    xz_debug("decode stream header at pos = %d", pos);

    /* Read and decode the stream header. */
    if (fseek(f, pos, SEEK_SET) != 0) {
      fprintf(stderr, "%s: fseek: %m", __func__);
      goto err;
    }
    if (fread(header, 1, LZMA_STREAM_HEADER_SIZE, f) != LZMA_STREAM_HEADER_SIZE) {
      fprintf(stderr, "%s: read stream header: %m", __func__);
      goto err;
    }

    r = lzma_stream_header_decode(&header_flags, header);
    if (r != LZMA_OK) {
      fprintf(stderr, "%s: invalid stream header (error %d)", __func__, r);
      goto err;
    }

    /* Header and footer of the stream should be equal. */
    r = lzma_stream_flags_compare(&header_flags, &footer_flags);
    if (r != LZMA_OK) {
      fprintf(stderr, "%s: header and footer of stream are not equal (error %d)",
                    __func__, r);
      goto err;
    }

    /* Store the decoded stream flags in this_index. */
    r = lzma_index_stream_flags(this_index, &footer_flags);
    if (r != LZMA_OK) {
      fprintf(stderr, "%s: cannot read stream_flags from index (error %d)",
                    __func__, r);
      goto err;
    }

    /* Store the amount of stream padding so far.  Needed to calculate
     * compressed offsets correctly in multi-stream files.
     */
    r = lzma_index_stream_padding(this_index, stream_padding);
    if (r != LZMA_OK) {
      fprintf(stderr, "%s: cannot set stream_padding in index (error %d)",
                    __func__, r);
      goto err;
    }

    if (combined_index != NULL) {
      r = lzma_index_cat(this_index, combined_index, NULL);
      if (r != LZMA_OK) {
        fprintf(stderr, "%s: cannot combine indexes", __func__);
        goto err;
      }
    }

    combined_index = this_index;
    this_index = NULL;
  }

  lzma_end(&strm);

  return combined_index;

 err:
  lzma_end(&strm);
  lzma_index_end(this_index, NULL);
  lzma_index_end(combined_index, NULL);
  return NULL;
}

/* Iterate over the indexes to find the number of blocks and
 * the largest block.
 */
static int
iter_indexes(lzma_index *idx,
              size_t *nr_blocks, __uint64_t *max_uncompressed_block_size)
{
  lzma_index_iter iter;

  *nr_blocks = 0;
  *max_uncompressed_block_size = 0;

  lzma_index_iter_init(&iter, idx);
  while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_NONEMPTY_BLOCK)) {
    if (iter.block.uncompressed_size > *max_uncompressed_block_size)
      *max_uncompressed_block_size = iter.block.uncompressed_size;
    (*nr_blocks)++;
  }

  return 0;
}

static int
xz_init(xzfile *xz)
{
  /* Check file magic. */
  if (check_header_magic(xz->f) != 0)
      return 1; /* error */

  /* Read and parse the indexes. */
  xz->idx = parse_indexes(xz->f, &xz->nr_streams);
  if (xz->idx == NULL)
      return 1; /* error */

  /* Iterate over indexes to find the number of and largest block. */
  if (iter_indexes(xz->idx,
                    &xz->nr_blocks, &xz->max_uncompressed_block_size) == -1)
      return 1; /* error */

  //  lzma_index_uncompressed_size(xz->idx);

  return 0; /* ok */
}

static void *
xz_open(__pmFILE *f, const char *path, const char *mode)
{
  xzfile *xz;

  xz = malloc(sizeof *xz);
  if (xz == NULL) {
      __pmNoMem("xz_open", sizeof(*xz), PM_FATAL_ERR);
      return NULL;
  }

  /* Open the file. */
  xz->f = fopen(path, mode);
  if (xz->f == NULL)
      goto err;

  if (xz_init(xz) == 0) {
      xz->fd = fileno(xz->f);
      f->priv = xz;
      return xz;
  }

  fclose(xz->f);
 err:
  free(xz);
  return NULL;
}

static void *
xz_fdopen(__pmFILE *f, int fd, const char *mode)
{
  xzfile *xz;

  xz = malloc(sizeof *xz);
  if (xz == NULL) {
      __pmNoMem("xz_open", sizeof(*xz), PM_FATAL_ERR);
      return NULL;
  }

  /* Open the file. */
  xz->f = fdopen(fd, mode);
  if (xz->f == NULL)
      goto err;

  if (xz_init(xz) == 0) {
      xz->fd = fd;
      f->priv = xz;
      return xz;
  }

  fclose(xz->f);
 err:
  free(xz);
  return NULL;
}

static int
xz_seek(__pmFILE *f, off_t offset, int whence)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static void
xz_rewind(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
}

static off_t
xz_tell(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_getc(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return EOF;
}

static size_t
xz_read(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static size_t
xz_write(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static int
xz_flush(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return EOF;
}

static int
xz_fsync(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_fileno(__pmFILE *f)
{
    xzfile *xz = f->priv;
    return xz->fd;
}

static off_t
xz_lseek(__pmFILE *f, off_t offset, int whence)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_fstat(__pmFILE *f, struct stat *buf)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_feof(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static int
xz_ferror(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static void
xz_clearerr(__pmFILE *f)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
}

static int
xz_setvbuf(__pmFILE *f, char *buf, int mode, size_t size)
{
    fprintf(stderr, "libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_close(__pmFILE *f)
{
    xzfile *xz = f->priv;
    int sts;
    
    lzma_index_end (xz->idx, NULL);
    sts = fclose(xz->f);
    free(xz);
    return sts;
}

__pm_fops __pm_xz = {
    /*
     * xz decompression
     */
    .__pmopen = xz_open,
    .__pmfdopen = xz_fdopen,
    .__pmseek = xz_seek,
    .__pmrewind = xz_rewind,
    .__pmtell = xz_tell,
    .__pmfgetc = xz_getc,
    .__pmread = xz_read,
    .__pmwrite = xz_write,
    .__pmflush = xz_flush,
    .__pmfsync = xz_fsync,
    .__pmfileno = xz_fileno,
    .__pmlseek = xz_lseek,
    .__pmfstat = xz_fstat,
    .__pmfeof = xz_feof,
    .__pmferror = xz_ferror,
    .__pmclearerr = xz_clearerr,
    .__pmsetvbuf = xz_setvbuf,
    .__pmclose = xz_close
};
