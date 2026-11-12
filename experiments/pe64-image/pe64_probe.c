/* Minimal x86-64 PE32+ execution probe. NOT a Windows loader or sandbox. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#if !defined(__x86_64__) || !defined(__GNUC__)
#error "This experiment requires x86-64 and a compiler supporting ms_abi"
#endif

#define MAX_FILE (16U * 1024U * 1024U)
#define MAX_IMAGE (64U * 1024U * 1024U)
#define MAX_SECTIONS 96U
#define PE_SECTION_HEADER 40U
#define PE32_PLUS_MAGIC 0x20bU
#define IMAGE_FILE_MACHINE_AMD64 0x8664U
#define MEM_EXECUTE UINT32_C(0x20000000)
#define MEM_READ UINT32_C(0x40000000)
#define MEM_WRITE UINT32_C(0x80000000)
#define BASERELOC_INDEX 5U
#define IMAGE_REL_BASED_DIR64 10U
#define IMAGE_REL_BASED_ABSOLUTE 0U

struct section {
    uint32_t virt_size, rva, raw_size, raw_offset, flags;
    size_t mapped_start, mapped_len;
};

static bool bounds(size_t offset, size_t amount, size_t extent) {
    return offset <= extent && amount <= extent - offset;
}
static uint16_t u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t u64(const uint8_t *p) {
    return (uint64_t)u32(p) | ((uint64_t)u32(p + 4) << 32);
}
static void store64(uint8_t *p, uint64_t v) {
    for (unsigned i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
static bool power2(uint32_t n) { return n && !(n & (n - 1)); }
static bool roundup(size_t n, size_t gran, size_t *out) {
    if (n > SIZE_MAX - (gran - 1)) return false;
    *out = (n + gran - 1) & ~(gran - 1);
    return true;
}
static int reject(const char *why) {
    fprintf(stderr, "REJECT: %s\n", why);
    return 1;
}
static int read_file(const char *path, uint8_t **out, size_t *out_len) {
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uint64_t)st.st_size > MAX_FILE) {
        close(fd);
        return reject("fixture is not a regular file or exceeds 16 MiB");
    }
    *out_len = (size_t)st.st_size;
    uint8_t *buf = malloc(*out_len ? *out_len : 1);
    if (!buf) { perror("malloc"); close(fd); return 1; }
    size_t done = 0;
    while (done < *out_len) {
        ssize_t n = read(fd, buf + done, *out_len - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { perror("read or unexpected EOF"); free(buf); close(fd); return 1; }
        done += (size_t)n;
    }
    close(fd);
    *out = buf;
    return 0;
}
static bool rva_has_data(const struct section *s, size_t count,
                         uint32_t rva, size_t len) {
    for (size_t i = 0; i < count; ++i) {
        size_t virtual_len = s[i].virt_size > s[i].raw_size ? s[i].virt_size : s[i].raw_size;
        if (rva >= s[i].rva && bounds((size_t)(rva - s[i].rva), len, virtual_len))
            return true;
    }
    return false;
}

static int execute(const uint8_t *file, size_t file_len, size_t page_size) {
    if (file_len < 64 || u16(file) != 0x5a4d)
        return reject("DOS MZ signature missing");
    size_t pe = u32(file + 0x3c);
    if (!bounds(pe, 24, file_len) || u32(file + pe) != 0x00004550)
        return reject("PE signature or COFF header invalid");
    size_t coff = pe + 4;
    unsigned sections_count = u16(file + coff + 2);
    unsigned opt_size = u16(file + coff + 16);
    if (u16(file + coff) != IMAGE_FILE_MACHINE_AMD64 ||
        sections_count == 0 || sections_count > MAX_SECTIONS || opt_size < 112)
        return reject("not an x86-64 PE image with supported section count");
    size_t opt = coff + 20;
    if (!bounds(opt, opt_size, file_len) || u16(file + opt) != PE32_PLUS_MAGIC)
        return reject("invalid PE32+ optional header");
    size_t section_table = opt + opt_size;
    if (!bounds(section_table, (size_t)sections_count * PE_SECTION_HEADER, file_len))
        return reject("section table exceeds file");
    uint32_t entry = u32(file + opt + 16);
    uint64_t preferred_base = u64(file + opt + 24);
    uint32_t section_alignment = u32(file + opt + 32);
    uint32_t file_alignment = u32(file + opt + 36);
    uint32_t image_size = u32(file + opt + 56);
    uint32_t header_size = u32(file + opt + 60);
    uint32_t dir_count = u32(file + opt + 108);
    if (section_alignment != page_size || !power2(file_alignment) ||
        file_alignment < 512 || file_alignment > 65536 ||
        (section_alignment < file_alignment) ||
        image_size == 0 || image_size > MAX_IMAGE || image_size % page_size ||
        header_size == 0 || header_size > image_size ||
        header_size > file_len || header_size % file_alignment ||
        entry == 0 || entry >= image_size || dir_count < 6 || dir_count > 16 ||
        !bounds(112, (size_t)dir_count * 8, opt_size))
        return reject("unsupported alignment, image/header size, entry or directory count");

    uint32_t reloc_rva = 0, reloc_len = 0;
    for (unsigned i = 0; i < dir_count; ++i) {
        uint32_t rva = u32(file + opt + 112 + (size_t)i * 8);
        uint32_t n = u32(file + opt + 116 + (size_t)i * 8);
        if (i == BASERELOC_INDEX) { reloc_rva = rva; reloc_len = n; }
        else if (rva || n) return reject("imports, TLS, exceptions, CLR or other directories unsupported");
    }
    if (!reloc_rva || reloc_len < 8 || !bounds(reloc_rva, reloc_len, image_size))
        return reject("image must contain a well-formed base-relocation directory");

    struct section *sections = calloc(sections_count, sizeof(*sections));
    if (!sections) { perror("calloc"); return 1; }
    bool found_entry = false, found_reloc = false;
    size_t previous_end = 0;
    for (unsigned i = 0; i < sections_count; ++i) {
        size_t h = section_table + (size_t)i * PE_SECTION_HEADER;
        struct section *s = &sections[i];
        s->virt_size = u32(file + h + 8);
        s->rva = u32(file + h + 12);
        s->raw_size = u32(file + h + 16);
        s->raw_offset = u32(file + h + 20);
        s->flags = u32(file + h + 36);
        size_t mapped_len = s->virt_size > s->raw_size ? s->virt_size : s->raw_size;
        if (!mapped_len || s->rva < header_size || s->rva % page_size ||
            !bounds(s->rva, mapped_len, image_size) ||
            (s->raw_size && (s->raw_offset % file_alignment ||
                             !bounds(s->raw_offset, s->raw_size, file_len))) ||
            (!s->raw_size && s->raw_offset) ||
            ((s->flags & MEM_WRITE) && (s->flags & MEM_EXECUTE))) {
            free(sections); return reject("unsupported or unsafe section layout");
        }
        if (!roundup(mapped_len, page_size, &s->mapped_len)) {
            free(sections); return reject("section length overflows");
        }
        s->mapped_start = s->rva;
        if (s->mapped_start < previous_end ||
            !bounds(s->mapped_start, s->mapped_len, image_size)) {
            free(sections); return reject("overlapping or out-of-order section pages");
        }
        previous_end = s->mapped_start + s->mapped_len;
        if ((s->flags & MEM_EXECUTE) &&
            entry >= s->rva && (size_t)(entry - s->rva) < mapped_len)
            found_entry = true;
        if (reloc_rva >= s->rva && bounds((size_t)(reloc_rva - s->rva), reloc_len, mapped_len))
            found_reloc = true;
    }
    if (!found_entry || !found_reloc) {
        free(sections); return reject("entry must be executable and relocations in a section");
    }

    uint8_t *image = mmap(NULL, image_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (image == MAP_FAILED) { free(sections); perror("mmap"); return 1; }
    memcpy(image, file, header_size);
    for (unsigned i = 0; i < sections_count; ++i) {
        if (sections[i].raw_size)
            memcpy(image + sections[i].rva, file + sections[i].raw_offset, sections[i].raw_size);
    }
    uint64_t delta = (uint64_t)(uintptr_t)image - preferred_base;
    size_t cursor = reloc_rva, reloc_end = (size_t)reloc_rva + reloc_len, changed = 0;
    bool valid_reloc = true;
    while (cursor < reloc_end) {
        if (!bounds(cursor, 8, reloc_end)) { valid_reloc = false; break; }
        uint32_t page = u32(image + cursor);
        uint32_t block_size = u32(image + cursor + 4);
        if (page % 4096 || block_size < 8 || (block_size & 1) ||
            !bounds(cursor, block_size, reloc_end)) { valid_reloc = false; break; }
        for (size_t at = cursor + 8; at < cursor + block_size; at += 2) {
            uint16_t fixup = u16(image + at);
            unsigned type = fixup >> 12;
            uint32_t rva = page + (fixup & 4095);
            if (type == IMAGE_REL_BASED_ABSOLUTE) continue;
            if (type != IMAGE_REL_BASED_DIR64 || rva < page ||
                !bounds(rva, 8, image_size) ||
                !rva_has_data(sections, sections_count, rva, 8)) {
                valid_reloc = false; break;
            }
            store64(image + rva, u64(image + rva) + delta);
            ++changed;
        }
        if (!valid_reloc) break;
        cursor += block_size;
    }
    if (!valid_reloc || !changed) {
        munmap(image, image_size); free(sections);
        return reject("invalid or empty DIR64 relocation entries");
    }

    /* Eliminate transient write+execute mappings before entering fixture code. */
    if (mprotect(image, image_size, PROT_NONE)) {
        perror("mprotect image"); munmap(image, image_size); free(sections); return 1;
    }
    size_t protected_headers;
    if (!roundup(header_size, page_size, &protected_headers) ||
        mprotect(image, protected_headers, PROT_READ)) {
        perror("mprotect headers"); munmap(image, image_size); free(sections); return 1;
    }
    for (unsigned i = 0; i < sections_count; ++i) {
        int prot = 0;
        if (sections[i].flags & MEM_READ) prot |= PROT_READ;
        if (sections[i].flags & MEM_WRITE) prot |= PROT_WRITE;
        if (sections[i].flags & MEM_EXECUTE) prot |= PROT_EXEC;
        if (mprotect(image + sections[i].mapped_start, sections[i].mapped_len, prot)) {
            perror("mprotect section"); munmap(image, image_size); free(sections); return 1;
        }
    }
    typedef int (__attribute__((ms_abi)) *win_entry_fn)(void);
    win_entry_fn fn = (win_entry_fn)(void *)(image + entry);
    int result = fn();
    printf("pe64_entry_return=%d applied_dir64=%zu image_size=%" PRIu32
           " actual_base=%p preferred_base=0x%" PRIx64 "\n",
           result, changed, image_size, (void *)image, preferred_base);
    munmap(image, image_size);
    free(sections);
    return result == 42 ? 0 : reject("entry returned unexpected value");
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s <trusted-no-imports-pe64-fixture.exe>\n", argv[0]); return 2; }
    long p = sysconf(_SC_PAGESIZE);
    if (p != 4096) return reject("this deliberately limited probe requires 4096-byte pages");
    uint8_t *file = NULL;
    size_t len = 0;
    if (read_file(argv[1], &file, &len)) return 1;
    int status = execute(file, len, (size_t)p);
    free(file);
    return status;
}
