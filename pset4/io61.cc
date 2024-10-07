#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>

// io61.cc
//    YOUR CODE HERE!


// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd = -1;     // file descriptor
    int mode;        // open mode (O_RDONLY or O_WRONLY)
};

struct io61_fcache {
    int fd;

    // `bufsiz` is the cache block size
    static constexpr off_t bufsize = 4096;
    // Cached data is stored in `cbuf`
    unsigned char cbuf[bufsize];

    // The following “tags” are addresses—file offsets—that describe the cache’s contents.
    // `tag`: File offset of first byte of cached data (0 when file is opened).
    off_t tag = 0;
    // `end_tag`: File offset one past the last byte of cached data (0 when file is opened).
    off_t end_tag = 0;
    // `pos_tag`: Cache position: file offset of the cache.
    // In read caches, this is the file offset of the next character to be read.
    off_t pos_tag = 0;
};

io61_fcache fc_read;
io61_fcache fc_write;

int io61_fill(io61_fcache* f) {
    // Empty the cache entry, then fill it with new data.
    // The new data is taken from the file starting at file offset `end_tag`
    // (which equals the file position).
    // The cache position equals the first character read.
    // Only called for read caches.

    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    // Reset the cache to empty.
    f->tag = f->pos_tag = f->end_tag;
    ssize_t n = read(f->fd, f->cbuf, f->bufsize);
    if (n >= 0) {
        f->end_tag = f->tag + n;
    }

    // Recheck invariants (good practice!).
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);


    return n;
}


// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file or O_WRONLY for a write-only file.
//    You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    return f;
}


// io61_close(f)
//    Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}


// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.

int io61_readc(io61_file* f) {
    unsigned char ch;

    if (fc_read.end_tag == 0) {
        fc_read.fd = f->fd;

    }
    if (fc_read.pos_tag == fc_read.end_tag) {
        ssize_t nread = io61_fill(&fc_read);

        if (fc_read.pos_tag == fc_read.end_tag || nread < 0) {
            return -1;
        }
    }
    ch = fc_read.cbuf[fc_read.pos_tag - fc_read.tag];
    ++fc_read.pos_tag;

    return ch;


}


// io61_read(f, buf, sz)
//    Reads up to `sz` bytes from `f` into `buf`. Returns the number of
//    bytes read on success. Returns 0 if end-of-file is encountered before
//    any bytes are read, and -1 if an error is encountered before any
//    bytes are read.
//
//    Note that the return value might be positive, but less than `sz`,
//    if end-of-file or error is encountered before all `sz` bytes are read.
//    This is called a “short read.”

ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    size_t nleft = sz;
    ssize_t nread = 0;
    unsigned char * bufp = buf;
    while (nleft > 0) {
        if ((nread = read(f->fd, bufp, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;
            else
                return -1;
        }
        else if (nread == 0)
            break;
        nleft -= nread;
        bufp += nread;
    }
    return (sz - nleft);

}


// io61_writec(f)
//    Write a single character `c` to `f` (converted to unsigned char).
//    Returns 0 on success and -1 on error.

int io61_writec(io61_file* f, int c) {
    unsigned char ch = c;
    if (fc_write.end_tag == 0) {
        fc_write.fd = f->fd;

    }
    if (fc_write.end_tag == fc_write.pos_tag && io61_flush(f)) {
        return -1;
    }

    fc_write.cbuf[fc_write.pos_tag - fc_write.tag] = ch;
    ++fc_write.pos_tag;
    return 0;
}


// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    size_t nleft = sz;
    ssize_t nwritten = 0;
    const unsigned char * bufp = buf;
    while (nleft > 0) {
        if ((nwritten = write(f->fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)
                nwritten = 0;
            else
                return -1;
        }
        else if (nwritten == 0)
            break;
        nleft -= nwritten;
        bufp += nwritten;
    }
    return sz;
}


// io61_flush(f)
//    If `f` was opened write-only, `io61_flush(f)` forces a write of any
//    cached data written to `f`. Returns 0 on success; returns -1 if an error
//    is encountered before all cached data was written.
//
//    If `f` was opened read-only, `io61_flush(f)` returns 0. It may also
//    drop any data cached for reading.

int io61_flush(io61_file* f) {

    ssize_t nwrite = write(f->fd, fc_write.cbuf, fc_write.pos_tag - fc_write.tag);
    if (nwrite >= 0) {
        fc_write.tag = fc_write.pos_tag = fc_write.end_tag;
        fc_write.end_tag += 199;
        return 0;
    }

    return -1;
}


// io61_seek(f, off)
//    Changes the file pointer for file `f` to `off` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t off) {
    off_t r = lseek(f->fd, (off_t) off, SEEK_SET);
    // Ignore the returned offset unless it’s an error.
    if (r == -1) {
        return -1;
    } else {
        return 0;
    }
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Opens the file corresponding to `filename` and returns its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_fileno(f)
//    Returns the file descriptor associated with `f`.

int io61_fileno(io61_file* f) {
    return f->fd;
}


// io61_filesize(f)
//    Returns the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}


