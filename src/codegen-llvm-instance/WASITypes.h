#ifndef SABLE_INCLUDE_CODEGEN_LLVM_CTX_WASI_TYPES
#define SABLE_INCLUDE_CODEGEN_LLVM_CTX_WASI_TYPES

#include <cstddef>
#include <cstdint>

namespace runtime::wasi {
using wasi_errno_t = std::uint16_t;
// clang-format off
constexpr wasi_errno_t ERRNO_SUCCESS        =  0;
constexpr wasi_errno_t ERRNO_2BIG           =  1;
constexpr wasi_errno_t ERRNO_ACCES          =  2;
constexpr wasi_errno_t ERRNO_ADDRINUSE      =  3;
constexpr wasi_errno_t ERRNO_ADDRNOTRAVAIL  =  4;
constexpr wasi_errno_t ERRNO_AFNOSUPPORT    =  5;
constexpr wasi_errno_t ERRNO_AGAIN          =  6;
constexpr wasi_errno_t ERRNO_ALREADY        =  7;
constexpr wasi_errno_t ERRNO_BADF           =  8;
constexpr wasi_errno_t ERRNO_BADMSG         =  9;
constexpr wasi_errno_t ERRNO_BUSY           = 10;
constexpr wasi_errno_t ERRNO_CANCELED       = 11;
constexpr wasi_errno_t ERRNO_CHILD          = 12;
constexpr wasi_errno_t ERRNO_CONNABORTED    = 13;
constexpr wasi_errno_t ERRNO_CONNREFUSED    = 14;
constexpr wasi_errno_t ERRNO_CONNRESET      = 15;
constexpr wasi_errno_t ERRNO_DEADLK         = 16;
constexpr wasi_errno_t ERRNO_DESTADDRRERQ   = 17;
constexpr wasi_errno_t ERRNO_DOM            = 18;
constexpr wasi_errno_t ERRNO_DQUOT          = 19;
constexpr wasi_errno_t ERRNO_EXIST          = 20;
constexpr wasi_errno_t ERRNO_FAULT          = 21;
constexpr wasi_errno_t ERRNO_FBIG           = 22;
constexpr wasi_errno_t ERRNO_HOSTUNREACH    = 23;
constexpr wasi_errno_t ERRNO_IDRM           = 24;
constexpr wasi_errno_t ERRNO_ILSEQ          = 25;
constexpr wasi_errno_t ERRNO_INPROGRESS     = 26;
constexpr wasi_errno_t ERRNO_INTR           = 27;
constexpr wasi_errno_t ERRNO_INVAL          = 28;
constexpr wasi_errno_t ERRNO_IO             = 29;
constexpr wasi_errno_t ERRNO_ISCONN         = 30;
constexpr wasi_errno_t ERRNO_ISDIR          = 31;
constexpr wasi_errno_t ERRNO_LOOP           = 32;
constexpr wasi_errno_t ERRNO_MFILE          = 33;
constexpr wasi_errno_t ERRNO_MLINK          = 34;
constexpr wasi_errno_t ERRNO_MSGSIZE        = 35;
constexpr wasi_errno_t ERRNO_MULTIHOP       = 36;
constexpr wasi_errno_t ERRNO_NAMETOOLONG    = 37;
constexpr wasi_errno_t ERRNO_NETDOWN        = 38;
constexpr wasi_errno_t ERRNO_NETRESET       = 39;
constexpr wasi_errno_t ERRNO_NETUNREACH     = 40;
constexpr wasi_errno_t ERRNO_NFILE          = 41;
constexpr wasi_errno_t ERRNO_NOBUFS         = 42;
constexpr wasi_errno_t ERRNO_NODEV          = 43;
constexpr wasi_errno_t ERRNO_NOENT          = 44;
constexpr wasi_errno_t ERRNO_NOEXEC         = 45;
constexpr wasi_errno_t ERRNO_NOLCK          = 46;
constexpr wasi_errno_t ERRNO_NOLINK         = 47;
constexpr wasi_errno_t ERRNO_NOMEM          = 48;
constexpr wasi_errno_t ERRNO_MOMSG          = 49;
constexpr wasi_errno_t ERRNO_NOPROTOOPT     = 50;
constexpr wasi_errno_t ERRNO_NOSPC          = 51;
constexpr wasi_errno_t ERRNO_NOSYS          = 52;
constexpr wasi_errno_t ERRNO_NOTCONN        = 53;
constexpr wasi_errno_t ERRNO_NOTDIR         = 54;
constexpr wasi_errno_t ERRNO_NOTEMPTY       = 55;
constexpr wasi_errno_t ERRNO_NOTRECOVERABLE = 56;
constexpr wasi_errno_t ERRNO_NOTSOCK        = 57;
constexpr wasi_errno_t ERRNO_NOTSUP         = 58;
constexpr wasi_errno_t ERRNO_NOTTY          = 59;
constexpr wasi_errno_t ERRNO_NXIO           = 60;
constexpr wasi_errno_t ERRNO_OVERFLOW       = 61;
constexpr wasi_errno_t ERRNO_OWNERDEAD      = 62;
constexpr wasi_errno_t ERRNO_PERM           = 63;
constexpr wasi_errno_t ERRNO_PIPE           = 64;
constexpr wasi_errno_t ERRNO_PROTO          = 65;
constexpr wasi_errno_t ERRNO_PROTONOSUPPORT = 66;
constexpr wasi_errno_t ERRNO_PROTOTYPE      = 67;
constexpr wasi_errno_t ERRNO_RANGE          = 68;
constexpr wasi_errno_t ERRNO_ROFS           = 69;
constexpr wasi_errno_t ERRNO_SPIPE          = 70;
constexpr wasi_errno_t ERRNO_SRCH           = 71;
constexpr wasi_errno_t ERRNO_STALE          = 72;
constexpr wasi_errno_t ERRNO_TIMEDOUT       = 73;
constexpr wasi_errno_t ERRNO_TXTBSY         = 74;
constexpr wasi_errno_t ERRNO_XDEV           = 75;
constexpr wasi_errno_t ERRNO_NOTCAPABLE     = 76;
// clang-format on

using wasi_intptr_t = std::uint32_t;
using wasi_size_t = std::uint32_t;

using wasi_fd_t = std::uint32_t;
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

using wasi_timestamp_t = std::uint64_t;
using wasi_clockid_t = std::uint32_t;
// clang-format off
constexpr wasi_clockid_t CLOCKID_REALTIME           = 0;
constexpr wasi_clockid_t CLOCKID_MONOTONIC          = 1;
constexpr wasi_clockid_t CLOCKID_PROCESS_CPUTIME_ID = 2;
constexpr wasi_clockid_t CLOCKID_THREAD_CPUTIME_ID  = 3;
// clang-format on
} // namespace runtime::wasi

#endif