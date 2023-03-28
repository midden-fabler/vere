/// @file

#include "wal.c"

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "page.h"

//==============================================================================
// FUNCTION TESTS

static void
test_wal_(void)
{
    // Empty write-ahead log.
    {
        wal_t wal;
        assert(wal_open("/tmp/wal-empty-test", &wal) == 0);

        char pg[kPageSz];

        char start = 'a';
        char end   = 'z';
        assert(start <= end);
        ssize_t heap_idx  = 0;
        ssize_t stack_idx = -1;
        for (char ch = start; ch <= end; ch++) {
            memset(pg, ch, sizeof(pg));
            size_t i = ch - start;
            assert(wal_append(&wal, i % 2 == 0 ? heap_idx : stack_idx, pg)
                   == 0);
            if (i % 2 == 0) {
                heap_idx++;
            } else {
                stack_idx--;
            }
        }
        assert(wal_sync(&wal) == 0);

        static const char kHeapPath[]  = "/tmp/wal-empty-test-target-heap";
        static const char kStackPath[] = "/tmp/wal-empty-test-target-stack";
        int heap_fd  = open(kHeapPath, O_CREAT | O_RDWR | O_TRUNC, 0644);
        int stack_fd = open(kStackPath, O_CREAT | O_RDWR | O_TRUNC, 0644);
        assert(wal_apply(&wal, heap_fd, stack_fd) == 0);

        assert(lseek(heap_fd, 0, SEEK_SET) == 0);
        assert(lseek(stack_fd, 0, SEEK_SET) == 0);

        for (char ch = start; ch <= end; ch++) {
            size_t i = ch - start;
            assert(read_all(i % 2 == 0 ? heap_fd : stack_fd, pg, sizeof(pg))
                   == 0);
            for (size_t j = 0; j < sizeof(pg); j++) {
                assert(pg[j] == ch);
            }
        }

        wal_destroy(&wal);
        close(heap_fd);
        close(stack_fd);
        unlink(kHeapPath);
        unlink(kStackPath);
    }

    // Corrupt data file of write-ahead log.
    {
        wal_t      wal;
        const char kPath[] = "/tmp/wal-corrupt-data-test";
        assert(wal_open(kPath, &wal) == 0);

        char pg[kPageSz];
        char start = '0';
        char end   = '9';
        assert(start <= end);
        for (char ch = start; ch <= end; ch++) {
            memset(pg, ch, sizeof(pg));
            assert(wal_append(&wal, ch - start, pg) == 0);
        }
        assert(wal_sync(&wal) == 0);

        int fd = open(wal.data_path, O_RDWR, 0644);
        assert(fd != -1);
        char ch = end + 1;
        assert(lseek(fd, 0, SEEK_SET) == 0);
        assert(write_all(fd, &ch, sizeof(ch)) == 0);
        assert(fsync(fd) == 0);

        // Can't use wal_destroy() here because it'll remove the WAL files.
        close(wal.data_fd);
        close(wal.meta_fd);
        free((void *)wal.data_path);
        free((void *)wal.meta_path);

        assert(wal_open(kPath, &wal) == -1);
        assert(errno == ENOTRECOVERABLE);

        char buf[512];

        snprintf(buf, sizeof(buf), "%s.%s", kPath, kWalDataExt);
        assert(unlink(buf) == 0);

        snprintf(buf, sizeof(buf), "%s.%s", kPath, kWalMetaExt);
        assert(unlink(buf) == 0);
    }

    // Corrupt metadata file of write-ahead log.
    {
        wal_t      wal;
        const char kPath[] = "/tmp/wal-corrupt-metadata-test";
        assert(wal_open(kPath, &wal) == 0);

        char pg[kPageSz];
        char start = '0';
        char end   = '9';
        assert(start <= end);
        for (char ch = start; ch <= end; ch++) {
            memset(pg, ch, sizeof(pg));
            assert(wal_append(&wal, ch - start, pg) == 0);
        }
        assert(wal_sync(&wal) == 0);

        int fd = open(wal.meta_path, O_RDWR, 0644);
        assert(fd != -1);
        uint64_t bad_pg_idx = -1;
        assert(lseek(fd, 0, SEEK_SET) == 0);
        assert(write_all(fd, &bad_pg_idx, sizeof(bad_pg_idx)) == 0);
        assert(fsync(fd) == 0);

        // Can't use wal_destroy() here because it'll remove the WAL files.
        close(wal.data_fd);
        close(wal.meta_fd);
        free((void *)wal.data_path);
        free((void *)wal.meta_path);

        assert(wal_open(kPath, &wal) == -1);
        assert(errno == ENOTRECOVERABLE);

        char buf[512];

        snprintf(buf, sizeof(buf), "%s.%s", kPath, kWalDataExt);
        assert(unlink(buf) == 0);

        snprintf(buf, sizeof(buf), "%s.%s", kPath, kWalMetaExt);
        assert(unlink(buf) == 0);
    }
}

int
main(int argc, char *argv[])
{
    test_wal_();
    return 0;
}
