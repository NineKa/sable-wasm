#ifndef SABLE_INCLUDE_CODEGEN_LLVM_CTX_WASI_TYPES
#define SABLE_INCLUDE_CODEGEN_LLVM_CTX_WASI_TYPES

namespace runtime::wasi {
// clang-format off
#define SABLE_WASI_ERRNO_SUCCESS         0
#define SABLE_WASI_ERRNO_2BIG            1
#define SABLE_WASI_ERRNO_ACCES           2
#define SABLE_WASI_ERRNO_ADDRINUSE       3
#define SABLE_WASI_ERRNO_ADDRNOTRAVAIL   4
#define SABLE_WASI_ERRNO_AFNOSUPPORT     5
#define SABLE_WASI_ERRNO_AGAIN           6
#define SABLE_WASI_ERRNO_ALREADY         7
#define SABLE_WASI_ERRNO_BADF            8
#define SABLE_WASI_ERRNO_BADMSG          9
#define SABLE_WASI_ERRNO_BUSY           10
#define SABLE_WASI_ERRNO_CANCELED       11
#define SABLE_WASI_ERRNO_CHILD          12
#define SABLE_WASI_ERRNO_CONNABORTED    13
#define SABLE_WASI_ERRNO_CONNREFUSED    14
#define SABLE_WASI_ERRNO_CONNRESET      15
#define SABLE_WASI_ERRNO_DEADLK         16
#define SABLE_WASI_ERRNO_DESTADDRRERQ   17
#define SABLE_WASI_ERRNO_DOM            18
#define SABLE_WASI_ERRNO_DQUOT          19
#define SABLE_WASI_ERRNO_EXIST          20
#define SABLE_WASI_ERRNO_FAULT          21
#define SABLE_WASI_ERRNO_FBIG           22
#define SABLE_WASI_ERRNO_HOSTUNREACH    23
#define SABLE_WASI_ERRNO_IDRM           24
#define SABLE_WASI_ERRNO_ILSEQ          25
#define SABLE_WASI_ERRNO_INPROGRESS     26
#define SABLE_WASI_ERRNO_INTR           27
#define SABLE_WASI_ERRNO_INVAL          28
#define SABLE_WASI_ERRNO_IO             29
#define SABLE_WASI_ERRNO_ISCONN         30
#define SABLE_WASI_ERRNO_ISDIR          31
#define SABLE_WASI_ERRNO_LOOP           32
#define SABLE_WASI_ERRNO_MFILE          33
#define SABLE_WASI_ERRNO_MLINK          34
#define SABLE_WASI_ERRNO_MSGSIZE        35
#define SABLE_WASI_ERRNO_MULTIHOP       36
#define SABLE_WASI_ERRNO_NAMETOOLONG    37
#define SABLE_WASI_ERRNO_NETDOWN        38
#define SABLE_WASI_ERRNO_NETRESET       39
#define SABLE_WASI_ERRNO_NETUNREACH     40
#define SABLE_WASI_ERRNO_NFILE          41
#define SABLE_WASI_ERRNO_NOBUFS         42
#define SABLE_WASI_ERRNO_NODEV          43
#define SABLE_WASI_ERRNO_NOENT          44
#define SABLE_WASI_ERRNO_NOEXEC         45
#define SABLE_WASI_ERRNO_NOLCK          46
#define SABLE_WASI_ERRNO_NOLINK         47
#define SABLE_WASI_ERRNO_NOMEM          48
#define SABLE_WASI_ERRNO_MOMSG          49
#define SABLE_WASI_ERRNO_NOPROTOOPT     50
#define SABLE_WASI_ERRNO_NOSPC          51
#define SABLE_WASI_ERRNO_NOSYS          52
#define SABLE_WASI_ERRNO_NOTCONN        53
#define SABLE_WASI_ERRNO_NOTDIR         54
#define SABLE_WASI_ERRNO_NOTEMPTY       55
#define SABLE_WASI_ERRNO_NOTRECOVERABLE 56
#define SABLE_WASI_ERRNO_NOTSOCK        57
#define SABLE_WASI_ERRNO_NOTSUP         58
#define SABLE_WASI_ERRNO_NOTTY          59
#define SABLE_WASI_ERRNO_NXIO           60
#define SABLE_WASI_ERRNO_OVERFLOW       61
#define SABLE_WASI_ERRNO_OWNERDEAD      62
#define SABLE_WASI_ERRNO_PERM           63
#define SABLE_WASI_ERRNO_PIPE           64
#define SABLE_WASI_ERRNO_PROTO          65
#define SABLE_WASI_ERRNO_PROTONOSUPPORT 66
#define SABLE_WASI_ERRNO_PROTOTYPE      67
#define SABLE_WASI_ERRNO_RANGE          68
#define SABLE_WASI_ERRNO_ROFS           69
#define SABLE_WASI_ERRNO_SPIPE          70
#define SABLE_WASI_ERRNO_SRCH           71
#define SABLE_WASI_ERRNO_STALE          72
#define SABLE_WASI_ERRNO_TIMEDOUT       73
#define SABLE_WASI_ERRNO_TXTBSY         74
#define SABLE_WASI_ERRNO_XDEV           75
#define SABLE_WASI_ERRNO_NOTCAPABLE     76
// clang-format on

using wasi_intptr_t = std::uint32_t;
using wasi_size_t = std::uint32_t;

using wasi_filetype_t = std::uint8_t;
using wasi_fdflags_t = std::uint16_t;
using wasi_rights_t = std::uint64_t;

struct wasi_fdstat_t {
  wasi_filetype_t fs_filetype;
  wasi_fdflags_t fs_flags;
  wasi_rights_t fs_rights_base;
  wasi_rights_t fs_rights_inheriting;
};
static_assert(sizeof(wasi_fdstat_t) == 24);
static_assert(offsetof(wasi_fdstat_t, fs_filetype) == 0);
static_assert(offsetof(wasi_fdstat_t, fs_flags) == 2);
static_assert(offsetof(wasi_fdstat_t, fs_rights_base) == 8);
static_assert(offsetof(wasi_fdstat_t, fs_rights_inheriting) == 16);

struct wasi_ciovec_t {
  wasi_intptr_t buf;
  wasi_size_t buf_len;
};
static_assert(sizeof(wasi_ciovec_t) == 8);
static_assert(offsetof(wasi_ciovec_t, buf) == 0);
static_assert(offsetof(wasi_ciovec_t, buf_len) == 4);
} // namespace runtime::wasi

#endif