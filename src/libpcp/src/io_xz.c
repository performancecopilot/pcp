/*
 * Copyright (c) 2013-2018 Red Hat.
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
#include "config.h"
#if HAVE_LZMA_DECOMPRESSION
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <lzma.h>
#include "pmapi.h"
#include "libpcp.h"

#ifndef PCP_XZ_CACHE_BLOCKS
#define PCP_XZ_CACHE_BLOCKS 4 /* 4 blocks in the cache, for now */
#endif

#define XZ_HEADER_MAGIC     "\xfd" "7zXZ\0"
#define XZ_HEADER_MAGIC_LEN 6
#define XZ_FOOTER_MAGIC     "YZ"
#define XZ_FOOTER_MAGIC_LEN 2

/* A block cache. Implemented as a very simple LRU list with a fixed depth. */
#if 0 /* not used yet */
typedef struct blkcache_stats {
    size_t hits;
    size_t misses;
} blkcache_stats;
#endif

/* A buffer of uncompressed blocks */
typedef struct block {
    uint64_t start;
    uint64_t size;
    uint64_t current_offset;
    char *data;
} block;

typedef struct blkcache {
    int maxdepth;
    block *blocks;
#if 0 /* not used yet */
    blkcache_stats stats;
#endif
} blkcache;

/* The file handle */
typedef struct xzfile {
    FILE *f;
    int fd;
    lzma_index *idx;
    size_t nr_streams;
    size_t nr_blocks;
    blkcache *cache;
    off_t uncompressed_offset;
  __uint64_t uncompressed_size;
  __uint64_t max_uncompressed_block_size;
} xzfile;

static void
xz_debug(const char *fmt, ...)
{
#if 0
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
#endif
}

blkcache *
new_blkcache(size_t maxdepth)
{
  blkcache *c;

  c = malloc(sizeof *c);
  if (!c) {
    xz_debug("malloc: %m");
    return NULL;
  }

  c->blocks = calloc(maxdepth, sizeof(block));
  if (!c->blocks) {
    xz_debug("calloc: %m");
    free(c);
    return NULL;
  }
  c->maxdepth = maxdepth;
#if 0 /* not used yet */
  c->stats.hits = c->stats.misses = 0;
#endif

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

#if 0 /* not used yet */
void
blkcache_get_stats(blkcache *c, blkcache_stats *ret)
{
  memcpy(ret, &c->stats, sizeof(c->stats));
}
#endif

static int
xz_feof(__pmFILE *f)
{
    xzfile *xz = f->priv;
    if (xz->uncompressed_offset >= xz->uncompressed_size)
	return 1;
    return 0;
}

static int
xz_ferror(__pmFILE *f)
{
    xz_debug("libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static void
xz_clearerr(__pmFILE *f)
{
    /* Do nothing --- for now */
}

static int
check_header_magic(FILE *f)
{
  char buf[XZ_HEADER_MAGIC_LEN];

  if (fseek(f, 0, SEEK_SET) == -1)
      return 1; /* error */
  if (fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
      setoserror(-PM_ERR_LOGREC);
      return 1; /* error */
  }
  if (memcmp(buf, XZ_HEADER_MAGIC, sizeof(buf)) != 0) {
      setoserror(-PM_ERR_LOGREC);
      return 1; /* error */
  }

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
  if ((pos & 3) != 0) {
      setoserror(-PM_ERR_LOGREC);
      goto err;
  }

  /* Jump backwards through the file identifying each stream. */
  while (pos > 0) {
      if (pos < LZMA_STREAM_HEADER_SIZE) {
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      if (fseek(f, -LZMA_STREAM_HEADER_SIZE, SEEK_CUR) != 0) {
	  xz_debug("%s: fseek: %m", __func__);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }
      if (fread(footer, 1, LZMA_STREAM_HEADER_SIZE, f) != LZMA_STREAM_HEADER_SIZE) {
	  xz_debug("%s: read stream footer: %m", __func__);
	  setoserror(-PM_ERR_LOGREC);
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
	  xz_debug("parse_indexes: invalid stream footer (error %d)", r);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }
      xz_debug("backward_size = %lu",
	       (unsigned long) footer_flags.backward_size);
      index_size = footer_flags.backward_size;
      if (pos < index_size + LZMA_STREAM_HEADER_SIZE) {
	  xz_debug("%s: invalid stream footer", __func__);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      pos -= index_size;
      xz_debug("decode index at pos = %d", pos);

      /* Seek backwards to the index of this stream. */
      if (fseek(f, pos, SEEK_SET) != 0) {
	  xz_debug("%s: fseek: %m", __func__);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      /* Decode the index. */
      r = lzma_index_decoder(&strm, &this_index, UINT64_MAX);
      if (r != LZMA_OK) {
	  xz_debug("%s: invalid stream index (error %d)", __func__, r);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      do {
	  uint8_t buf[BUFSIZ];

	  strm.avail_in = index_size;
	  if (strm.avail_in > BUFSIZ)
	      strm.avail_in = BUFSIZ;

	  n = fread(&buf, 1, strm.avail_in, f);
	  if (n != strm.avail_in) {
	      xz_debug("read: %m");
	      setoserror(-PM_ERR_LOGREC);
	      goto err;
	  }
	  index_size -= strm.avail_in;

	  strm.next_in = buf;
	  r = lzma_code(&strm, LZMA_RUN);
      } while (r == LZMA_OK);

      if (r != LZMA_STREAM_END) {
	  xz_debug("%s: could not parse index (error %d)",
		   __func__, r);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      pos -= lzma_index_total_size(this_index) + LZMA_STREAM_HEADER_SIZE;

      xz_debug("decode stream header at pos = %d", pos);

      /* Read and decode the stream header. */
      if (fseek(f, pos, SEEK_SET) != 0) {
	  xz_debug("%s: fseek: %m", __func__);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }
      if (fread(header, 1, LZMA_STREAM_HEADER_SIZE, f) != LZMA_STREAM_HEADER_SIZE) {
	  xz_debug("%s: read stream header: %m", __func__);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      r = lzma_stream_header_decode(&header_flags, header);
      if (r != LZMA_OK) {
	  xz_debug("%s: invalid stream header (error %d)", __func__, r);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      /* Header and footer of the stream should be equal. */
      r = lzma_stream_flags_compare(&header_flags, &footer_flags);
      if (r != LZMA_OK) {
	  xz_debug("%s: header and footer of stream are not equal (error %d)",
		   __func__, r);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      /* Store the decoded stream flags in this_index. */
      r = lzma_index_stream_flags(this_index, &footer_flags);
      if (r != LZMA_OK) {
	  xz_debug("%s: cannot read stream_flags from index (error %d)",
		   __func__, r);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      /* Store the amount of stream padding so far.  Needed to calculate
       * compressed offsets correctly in multi-stream files.
       */
      r = lzma_index_stream_padding(this_index, stream_padding);
      if (r != LZMA_OK) {
	  xz_debug("%s: cannot set stream_padding in index (error %d)",
		   __func__, r);
	  setoserror(-PM_ERR_LOGREC);
	  goto err;
      }

      if (combined_index != NULL) {
	  r = lzma_index_cat(this_index, combined_index, NULL);
	  if (r != LZMA_OK) {
	      xz_debug("%s: cannot combine indexes", __func__);
	      setoserror(-PM_ERR_LOGREC);
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
init(xzfile *xz)
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

  xz->uncompressed_size = lzma_index_uncompressed_size(xz->idx);
  xz->uncompressed_offset = 0;
  xz->cache = new_blkcache(PCP_XZ_CACHE_BLOCKS);
  
  return 0; /* ok */
}

static void *
xz_open(__pmFILE *f, const char *path, const char *mode)
{
  xzfile *xz;

  xz = malloc(sizeof *xz);
  if (xz == NULL) {
      pmNoMem("xz_open", sizeof(*xz), PM_FATAL_ERR);
      return NULL;
  }

  /* Open the file. */
  xz->f = fopen(path, mode);
  if (xz->f == NULL)
      goto err;

  if (init(xz) == 0) {
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
      pmNoMem("xz_open", sizeof(*xz), PM_FATAL_ERR);
      return NULL;
  }

  /* Open the file. */
  xz->f = fdopen(fd, mode);
  if (xz->f == NULL)
      goto err;

  if (init(xz) == 0) {
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
    xzfile *xz = (xzfile *)f->priv;;
    __int64_t new_offset;

    switch (whence) {
    case SEEK_SET:
	new_offset = offset;
	break;
    case SEEK_CUR:
	new_offset = xz->uncompressed_offset + offset;
	break;
    case SEEK_END:
	new_offset = xz->uncompressed_size + offset;
	break;
    default:
	errno = EINVAL;
	return -1;
    }

    if (new_offset < 0) {
	errno = EINVAL;
	return -1;
    }

    /* Don't actually seek to the requested offset now. Just record it. */
    xz->uncompressed_offset = new_offset;
    return 0;
}

static off_t
xz_lseek(__pmFILE *f, off_t offset, int whence)
{
    /* The same as xz_seek for our purposes */
    return xz_seek(f, offset, whence);
}

static void
xz_rewind(__pmFILE *f)
{
    xzfile *xz = (xzfile *)f->priv;;

    /* Don't actually seek to the requested offset now. Just record it. */
    xz->uncompressed_offset = 0;

    /* The function also requires that any error flag be reset. */
    xz_clearerr(f);
}

static off_t
xz_tell(__pmFILE *f)
{
    xzfile *xz = (xzfile *)f->priv;;
    return xz->uncompressed_offset;
}

static block *
cache_block_used(blkcache *cache, int slot)
{
    /* Move this block into the first slot, if it is not already there */
    if (slot != 0) {
	block used = cache->blocks[slot];
	int i;
	for (i = slot; i > 0; --i)
	    cache->blocks[i] = cache->blocks[i - 1];
	cache->blocks[0] = used;
    }
    return &cache->blocks[0];
}

static char *
read_block(xzfile *xz, uint64_t offset,
                   uint64_t *start_rtn, uint64_t *size_rtn)
{
  lzma_index_iter iter;
  uint8_t header[LZMA_BLOCK_HEADER_SIZE_MAX];
  lzma_block block;
  lzma_filter filters[LZMA_FILTERS_MAX + 1];
  lzma_ret r;
  lzma_stream strm = LZMA_STREAM_INIT;
  char *data;
  ssize_t n;
  size_t i;

  /* Locate the block containing the uncompressed offset. */
  lzma_index_iter_init(&iter, xz->idx);
  if (lzma_index_iter_locate(&iter, offset)) {
    xz_debug("cannot find offset %lu in the xz file", offset);
    return NULL;
  }

  *start_rtn = iter.block.uncompressed_file_offset;
  *size_rtn = iter.block.uncompressed_size;

  xz_debug("seek: block number %d at file offset %lu",
                (int) iter.block.number_in_file,
                (uint64_t) iter.block.compressed_file_offset);

  if (lseek(xz->fd, iter.block.compressed_file_offset, SEEK_SET) == -1) {
    xz_debug("lseek: %m");
    return NULL;
  }

  /* Read the block header.  Start by reading a single byte which
   * tell us how big the block header is.
   */
  n = read(xz->fd, header, 1);
  if (n == 0) {
    xz_debug("read: unexpected end of file reading block header byte");
    return NULL;
  }
  if (n == -1) {
    xz_debug("read: %m");
    return NULL;
  }

  if (header[0] == '\0') {
    xz_debug("read: unexpected invalid block in file, header[0] = 0");
    return NULL;
  }

  block.version = 0;
  block.check = iter.stream.flags->check;
  block.filters = filters;
  block.header_size = lzma_block_header_size_decode(header[0]);

  /* Now read and decode the block header. */
  n = read(xz->fd, &header[1], block.header_size-1);
  if (n >= 0 && n != block.header_size-1) {
    xz_debug("read: unexpected end of file reading block header");
    return NULL;
  }
  if (n == -1) {
    xz_debug("read: %m");
    return NULL;
  }

  r = lzma_block_header_decode(&block, NULL, header);
  if (r != LZMA_OK) {
    xz_debug("invalid block header (error %d)", r);
    return NULL;
  }

  /* What this actually does is it checks that the block header
   * matches the index.
   */
  r = lzma_block_compressed_size(&block, iter.block.unpadded_size);
  if (r != LZMA_OK) {
    xz_debug("cannot calculate compressed size (error %d)", r);
    goto err1;
  }

  /* Read the block data. */
  r = lzma_block_decoder(&strm, &block);
  if (r != LZMA_OK) {
    xz_debug("invalid block (error %d)", r);
    goto err1;
  }

  data = malloc(*size_rtn);
  if (data == NULL) {
    xz_debug("malloc(%lu bytes): %m\n"
                  "NOTE: If this error occurs, you need to recompress your xz files with a smaller block size.  Use: 'xz --block-size=16777216 ...'.",
                  *size_rtn);
    goto err1;
  }

  strm.next_in = NULL;
  strm.avail_in = 0;
  strm.next_out = (uint8_t *) data;
  strm.avail_out = block.uncompressed_size;

  do {
    uint8_t buf[BUFSIZ];
    lzma_action action = LZMA_RUN;

    if (strm.avail_in == 0) {
      strm.next_in = buf;
      n = read(xz->fd, buf, sizeof buf);
      if (n == -1) {
        xz_debug("read: %m");
        goto err2;
      }
      strm.avail_in = n;
      if (n == 0)
        action = LZMA_FINISH;
    }

    strm.avail_in = n;
    strm.next_in = buf;
    r = lzma_code(&strm, action);
  } while (r == LZMA_OK);

  if (r != LZMA_OK && r != LZMA_STREAM_END) {
    xz_debug("could not parse block data (error %d)", r);
    goto err2;
  }

  lzma_end(&strm);

  for (i = 0; filters[i].id != LZMA_VLI_UNKNOWN; ++i)
    free(filters[i].options);

  return data;

 err2:
  free(data);
  lzma_end(&strm);
 err1:
  for (i = 0; filters[i].id != LZMA_VLI_UNKNOWN; ++i)
    free(filters[i].options);

  return NULL;
}

static block *
read_new_block(xzfile *xz, int slot)
{
    blkcache *cache;
    block *blk;
    char *data;
    uint64_t start = 0, size = 0; /* silence coverity */

    /* Decompress a new block into the given slot. */
    data = read_block(xz, xz->uncompressed_offset, &start, &size);
    if (data == NULL)
	return NULL;

    /* Add the data to this block */
    cache = xz->cache;
    blk = &cache->blocks[slot];
    if (blk->data != NULL)
	free(blk->data);
    blk->data = data;
    blk->start = start;
    blk->size = size;
    blk->current_offset = xz->uncompressed_offset - blk->start;

    /* Mark this block as most recently used */
    blk = cache_block_used(cache, slot);
	    
    return blk;
}

/*
 * Find the block containing the current uncompressed offset and update the
 * current offset within that block..
 */
static block *
reposition(xzfile *xz)
{
    blkcache *cache;
    block *blk;
    int slot;

    /*
     * Search the cache for the block we want.
     * The cache is sorted by most recently used blocks. This works out well
     * since the data we want is most often in the block which will be checked
     * first.
     */
    cache = xz->cache;
    for (slot = 0; slot < cache->maxdepth; ++slot) {
	blk = &cache->blocks[slot];
	if (blk->data == NULL)
	    break; /* end of cache */

	if (xz->uncompressed_offset >= blk->start &&
	    xz->uncompressed_offset < blk->start + blk->size) {
	    /* found it */
	    blk->current_offset = xz->uncompressed_offset - blk->start;
	    blk = cache_block_used(cache, slot);
	    return blk;
	}
    }

    /*
     * No cached block contains the uncompressed offset that we want.
     * Decompress a new block into an empty slot, or into the least recently
     * used slot, which is the last one.
     */
    if (slot >= cache->maxdepth)
	slot = cache->maxdepth - 1;
    blk = read_new_block(xz, slot);

    return blk;
}

static int
xz_getc(__pmFILE *f)
{
    xzfile *xz = (xzfile *)f->priv;;
    block *blk = reposition(xz);
    int c;

    if (blk == NULL)
	return EOF;

    /* It's a single byte. It is guaranteed that we can copy it. */
    c = *(unsigned char *)(blk->data + blk->current_offset);
    ++xz->uncompressed_offset;
    ++blk->current_offset;
    return c;
}

static size_t
xz_read(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    xzfile *xz = (xzfile *)f->priv;;
    block *blk;
    const char *p;
    size_t n;
    size_t copied;

    /* Obtain the requested size in bytes. */
    size *= nmemb;

    /*
     * Copy data from the current block and possibly from subsequent blocks
     * until we have copied the requested number of bytes. */
    copied = 0;
    while (size > 0) {
	blk = reposition(xz);
	if (blk == NULL)
	    return 0; /* error */

	/* See how many bytes we can copy from the current block. */
	p =  blk->data + blk->current_offset;
	n = size;
	if (n > blk->size - blk->current_offset)
	    n = blk->size - blk->current_offset;

	/*
	 * Copy the requested number of bytes from the current block to the
	 * output buffer. We are guaranteed to be able to copy at least one
	 * byte.
	 */
	memcpy(ptr, p, n);
	copied += n;
	xz->uncompressed_offset += n;
	blk->current_offset += n;
	ptr = ((char *)ptr) + n;
	size -= n;
    }

    return copied;
}

static size_t
xz_write(void *ptr, size_t size, size_t nmemb, __pmFILE *f)
{
    xz_debug("libpcp internal error: %s not implemented\n", __func__);
    return 0;
}

static int
xz_flush(__pmFILE *f)
{
    xz_debug("libpcp internal error: %s not implemented\n", __func__);
    return EOF;
}

static int
xz_fsync(__pmFILE *f)
{
    xz_debug("libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_fileno(__pmFILE *f)
{
    xzfile *xz = f->priv;
    return xz->fd;
}

static int
xz_fstat(__pmFILE *f, struct stat *buf)
{
    xzfile *xz = f->priv;
    FILE *fp = xz->f;
    int rc = fstat(fileno(fp), buf);

    /* What the caller really wants for st_size is the uncompressed size. */
    if (rc != -1)
	buf->st_size = xz->uncompressed_size;
    
    return rc;
}

static int
xz_setvbuf(__pmFILE *f, char *buf, int mode, size_t size)
{
    xz_debug("libpcp internal error: %s not implemented\n", __func__);
    return -1;
}

static int
xz_close(__pmFILE *f)
{
    xzfile *xz = f->priv;
    int sts;
    
    lzma_index_end (xz->idx, NULL);
    sts = fclose(xz->f);
    free_blkcache(xz->cache);
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
#endif /* HAVE_LZMA_DECOMPRESSION */
