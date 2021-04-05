#include "WASI.h"
#include "WASITypes.h"

#include "WebAssemblyInstance.h"

#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cstdlib>
#include <vector>

#define SABLE_WASI_STDIN 0
#define SABLE_WASI_STDOUT 1
#define SABLE_WASI_STDERR 2

namespace runtime::wasi {
namespace {
__sable_memory_t *getImplicitLinearMemory(__sable_instance_t *InstancePtr) {
  auto &Instance = *WebAssemblyInstance::fromInstancePtr(InstancePtr);
  return Instance.getMemory("memory").asInstancePtr();
}

template <typename T>
T readFromLinearMemory(__sable_memory_t *MemoryPtr, std::uint32_t Address) {
  __sable_memory_guard(MemoryPtr, Address + sizeof(T));
  auto &Memory = *WebAssemblyMemory::fromInstancePtr(MemoryPtr);
  T Result{};
  auto *Source = std::addressof(Memory[Address]);
  auto *Dest = std::addressof(Result);
  std::memcpy(Dest, Source, sizeof(T));
  return Result;
}

template <typename T>
void writeToLinearMemory(
    __sable_memory_t *MemoryPtr, std::uint32_t Address, T const &Value) {
  __sable_memory_guard(MemoryPtr, Address + sizeof(T));
  auto &Memory = *WebAssemblyMemory::fromInstancePtr(MemoryPtr);
  auto *Source = std::addressof(Value);
  auto *Dest = std::addressof(Memory[Address]);
  std::memcpy(Dest, Source, sizeof(T));
}

std::int32_t convertNativeErrnoToWASI() {
  // clang-format off
  switch (errno) {
  case E2BIG          : return SABLE_WASI_ERRNO_2BIG;
  case EACCES         : return SABLE_WASI_ERRNO_ACCES;
  case EADDRINUSE     : return SABLE_WASI_ERRNO_ADDRINUSE;
  case EADDRNOTAVAIL  : return SABLE_WASI_ERRNO_ADDRNOTRAVAIL;
  case EAFNOSUPPORT   : return SABLE_WASI_ERRNO_AFNOSUPPORT;
  case EAGAIN         : return SABLE_WASI_ERRNO_AGAIN;
  case EALREADY       : return SABLE_WASI_ERRNO_ALREADY;
  case EBADF          : return SABLE_WASI_ERRNO_BADF;
  case EBADMSG        : return SABLE_WASI_ERRNO_BADMSG;
  case EBUSY          : return SABLE_WASI_ERRNO_BUSY;
  case ECANCELED      : return SABLE_WASI_ERRNO_CANCELED;
  case ECHILD         : return SABLE_WASI_ERRNO_CHILD;
  case ECONNABORTED   : return SABLE_WASI_ERRNO_CONNABORTED;
  case ECONNREFUSED   : return SABLE_WASI_ERRNO_CONNREFUSED;
  case ECONNRESET     : return SABLE_WASI_ERRNO_CONNRESET;
  case EDEADLK        : return SABLE_WASI_ERRNO_DEADLK;
  case EDESTADDRREQ   : return SABLE_WASI_ERRNO_DESTADDRRERQ;
  case EDOM           : return SABLE_WASI_ERRNO_DOM;
  case EDQUOT         : return SABLE_WASI_ERRNO_DQUOT;
  case EEXIST         : return SABLE_WASI_ERRNO_EXIST;
  case EFAULT         : return SABLE_WASI_ERRNO_FAULT;
  case EFBIG          : return SABLE_WASI_ERRNO_FBIG;
  case EHOSTUNREACH   : return SABLE_WASI_ERRNO_HOSTUNREACH;
  case EIDRM          : return SABLE_WASI_ERRNO_IDRM;
  case EILSEQ         : return SABLE_WASI_ERRNO_ILSEQ;
  case EINPROGRESS    : return SABLE_WASI_ERRNO_INPROGRESS;
  case EINTR          : return SABLE_WASI_ERRNO_INTR;
  case EINVAL         : return SABLE_WASI_ERRNO_INVAL;
  case EIO            : return SABLE_WASI_ERRNO_IO;
  case EISCONN        : return SABLE_WASI_ERRNO_ISCONN;
  case EISDIR         : return SABLE_WASI_ERRNO_ISDIR;
  case ELOOP          : return SABLE_WASI_ERRNO_LOOP;
  case EMFILE         : return SABLE_WASI_ERRNO_MFILE;
  case EMLINK         : return SABLE_WASI_ERRNO_MLINK;
  case EMSGSIZE       : return SABLE_WASI_ERRNO_MSGSIZE;
  case EMULTIHOP      : return SABLE_WASI_ERRNO_MULTIHOP;
  case ENAMETOOLONG   : return SABLE_WASI_ERRNO_NAMETOOLONG;
  case ENETDOWN       : return SABLE_WASI_ERRNO_NETDOWN;
  case ENETRESET      : return SABLE_WASI_ERRNO_NETRESET;
  case ENETUNREACH    : return SABLE_WASI_ERRNO_NETUNREACH;
  case ENFILE         : return SABLE_WASI_ERRNO_NFILE;
  case ENOBUFS        : return SABLE_WASI_ERRNO_NOBUFS;
  case ENODEV         : return SABLE_WASI_ERRNO_NODEV;
  case ENOENT         : return SABLE_WASI_ERRNO_NOENT;
  case ENOEXEC        : return SABLE_WASI_ERRNO_NOEXEC;
  case ENOLCK         : return SABLE_WASI_ERRNO_NOLCK;
  case ENOLINK        : return SABLE_WASI_ERRNO_NOLINK;
  case ENOMEM         : return SABLE_WASI_ERRNO_NOMEM;
  case ENOMSG         : return SABLE_WASI_ERRNO_MOMSG;
  case ENOPROTOOPT    : return SABLE_WASI_ERRNO_NOPROTOOPT;
  case ENOSPC         : return SABLE_WASI_ERRNO_NOSPC;
  case ENOSYS         : return SABLE_WASI_ERRNO_NOSYS;
  case ENOTCONN       : return SABLE_WASI_ERRNO_NOTCONN;
  case ENOTDIR        : return SABLE_WASI_ERRNO_NOTDIR;
  case ENOTEMPTY      : return SABLE_WASI_ERRNO_NOTEMPTY;
  case ENOTRECOVERABLE: return SABLE_WASI_ERRNO_NOTRECOVERABLE;
  case ENOTSOCK       : return SABLE_WASI_ERRNO_NOTSOCK;
  case ENOTSUP        : return SABLE_WASI_ERRNO_NOTSUP;
  case ENOTTY         : return SABLE_WASI_ERRNO_NOTTY;
  case ENXIO          : return SABLE_WASI_ERRNO_NXIO;
  case EOVERFLOW      : return SABLE_WASI_ERRNO_OVERFLOW;
  case EOWNERDEAD     : return SABLE_WASI_ERRNO_OWNERDEAD;
  case EPERM          : return SABLE_WASI_ERRNO_PERM;
  case EPIPE          : return SABLE_WASI_ERRNO_PIPE;
  case EPROTO         : return SABLE_WASI_ERRNO_PROTO;
  case EPROTONOSUPPORT: return SABLE_WASI_ERRNO_PROTONOSUPPORT;
  case EPROTOTYPE     : return SABLE_WASI_ERRNO_PROTOTYPE;
  case ERANGE         : return SABLE_WASI_ERRNO_RANGE;
  case EROFS          : return SABLE_WASI_ERRNO_ROFS;
  case ESPIPE         : return SABLE_WASI_ERRNO_SPIPE;
  case ESRCH          : return SABLE_WASI_ERRNO_SRCH;
  case ESTALE         : return SABLE_WASI_ERRNO_STALE;
  case ETIMEDOUT      : return SABLE_WASI_ERRNO_TIMEDOUT;
  case ETXTBSY        : return SABLE_WASI_ERRNO_TXTBSY;
  case EXDEV          : return SABLE_WASI_ERRNO_XDEV;
  default: utility::unreachable();
  }
  // clang-format on
}

#define BAD_SEEK_WHENCE std::numeric_limits<int>::max()
static_assert(
    (BAD_SEEK_WHENCE != SEEK_SET) && (BAD_SEEK_WHENCE != SEEK_CUR) &&
    (BAD_SEEK_WHENCE != SEEK_END));
int convertWASIWhenceToNative(std::int32_t Whence) {
  switch (Whence) {
  case 0: return SEEK_SET;
  case 1: return SEEK_CUR;
  case 2: return SEEK_END;
  default: utility::unreachable();
  }
}
} // namespace

void proc_exit(__sable_instance_t *, std::int32_t ExitCode) {
  throw exceptions::WASIExit(ExitCode);
}

std::int32_t fd_seek(
    __sable_instance_t *InstancePtr, std::int32_t FileDescriptor,
    std::int64_t Offset, std::int32_t Whence, std::int32_t ResultAddress) {
  auto fd = static_cast<int>(FileDescriptor);
  auto offset = static_cast<off64_t>(Offset);
  auto whence = convertWASIWhenceToNative(Whence);
  if (whence == BAD_SEEK_WHENCE) return SABLE_WASI_ERRNO_INVAL;
  auto result = lseek64(fd, offset, whence);
  if (result == -1) return convertNativeErrnoToWASI();
  auto *LinearMemory = getImplicitLinearMemory(InstancePtr);
  writeToLinearMemory<std::int64_t>(LinearMemory, ResultAddress, result);
  return SABLE_WASI_ERRNO_SUCCESS;
}

std::int32_t fd_close(__sable_instance_t *, std::int32_t FileDescriptor) {
  auto fd = static_cast<int>(FileDescriptor);
  if (close(fd) == -1) return convertNativeErrnoToWASI();
  return SABLE_WASI_ERRNO_SUCCESS;
}

std::int32_t fd_fdstat_get(__sable_instance_t *, std::int32_t, std::int32_t) {
  return SABLE_WASI_ERRNO_BADF;
}

std::int32_t fd_write(
    __sable_instance_t *InstancePtr, std::int32_t FileDescriptor,
    std::int32_t IOVectors, std::int32_t IOVectorCount,
    std::int32_t ResultAddress) {
  auto *LinearMemory = getImplicitLinearMemory(InstancePtr);
  auto &Memory = *WebAssemblyMemory::fromInstancePtr(LinearMemory);
  std::vector<iovec> NativeIOVectors;
  NativeIOVectors.reserve(IOVectorCount);
  for (std::int32_t I = 0; I < IOVectorCount; ++I) {
    auto Address = IOVectors + I * sizeof(wasi_ciovec_t);
    auto WASIIOVector =
        readFromLinearMemory<wasi_ciovec_t>(LinearMemory, Address);
    iovec NativeIOVector{
        .iov_base = Memory.data() + WASIIOVector.buf,
        .iov_len = WASIIOVector.buf_len};
    NativeIOVectors.push_back(NativeIOVector);
  }
  auto fd = static_cast<int>(FileDescriptor);
  auto result = writev(fd, NativeIOVectors.data(), NativeIOVectors.size());
  if (result == -1) return convertNativeErrnoToWASI();
  writeToLinearMemory<std::uint32_t>(LinearMemory, ResultAddress, result);
  return SABLE_WASI_ERRNO_SUCCESS;
}

std::int32_t args_sizes_get(
    __sable_instance_t *InstancePtr, std::int32_t NumArgAddress,
    std::int32_t BufSizeAddress) {
  auto *LinearMemory = getImplicitLinearMemory(InstancePtr);
  writeToLinearMemory<wasi_intptr_t>(LinearMemory, NumArgAddress, 0);
  writeToLinearMemory<wasi_intptr_t>(LinearMemory, BufSizeAddress, 0);
  return SABLE_WASI_ERRNO_SUCCESS;
}

std::int32_t args_get(__sable_instance_t *, std::int32_t, std::int32_t) {
  return SABLE_WASI_ERRNO_SUCCESS;
}

} // namespace runtime::wasi