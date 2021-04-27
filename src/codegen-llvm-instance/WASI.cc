#include "WASI.h"
#include "WASITypes.h"
#include "WebAssemblyInstance.h"

#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cstdlib>
#include <ctime>
#include <random>
#include <span>
#include <vector>

namespace runtime::wasi {
namespace {
constexpr wasi_fd_t WASI_STDIN = 0;
constexpr wasi_fd_t WASI_STDOUT = 1;
constexpr wasi_fd_t WASI_STDERR = 2;

__sable_memory_t *getImplicitLinearMemory(__sable_instance_t *InstancePtr) {
  auto &Instance = *WebAssemblyInstance::fromInstancePtr(InstancePtr);
  return Instance.getMemory("memory").asInstancePtr();
}

template <typename T>
T read(__sable_memory_t *MemoryPtr, std::uint32_t Address) {
  __sable_memory_guard(MemoryPtr, Address + sizeof(T));
  auto &Memory = *WebAssemblyMemory::fromInstancePtr(MemoryPtr);
  T Result{};
  auto *Source = std::addressof(Memory[Address]);
  auto *Dest = std::addressof(Result);
  std::memcpy(Dest, Source, sizeof(T));
  return Result;
}

template <typename T>
void write(__sable_memory_t *MemoryPtr, std::uint32_t Address, T const &Value) {
  __sable_memory_guard(MemoryPtr, Address + sizeof(T));
  auto &Memory = *WebAssemblyMemory::fromInstancePtr(MemoryPtr);
  auto *Source = std::addressof(Value);
  auto *Dest = std::addressof(Memory[Address]);
  std::memcpy(Dest, Source, sizeof(T));
}
} // namespace

void proc_exit(__sable_instance_t *, std::int32_t ExitCode) {
  throw exceptions::WASIExit(ExitCode);
}

std::int32_t fd_seek(
    __sable_instance_t *, std::int32_t, std::int64_t, std::int32_t,
    std::int32_t) {
  return ERRNO_BADF;
}

std::int32_t fd_close(__sable_instance_t *, std::int32_t) { return ERRNO_BADF; }

std::int32_t fd_fdstat_get(__sable_instance_t *, std::int32_t, std::int32_t) {
  return ERRNO_BADF;
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
    auto WASIIOVector = read<wasi_ciovec_t>(LinearMemory, Address);
    iovec NativeIOVector{
        .iov_base = Memory.data() + WASIIOVector.buf,
        .iov_len = WASIIOVector.buf_len};
    NativeIOVectors.push_back(NativeIOVector);
  }
  auto fd = static_cast<int>(FileDescriptor);
  if ((fd == WASI_STDOUT) || (fd == WASI_STDERR)) {
    auto result = writev(fd, NativeIOVectors.data(), NativeIOVectors.size());
    if (result == -1) {
      // clang-format off
      switch (errno) {
      case EAGAIN      : return ERRNO_AGAIN;
      case EBADF       : return ERRNO_BADF;
      case EDESTADDRREQ: return ERRNO_DESTADDRRERQ;
      case EDQUOT      : return ERRNO_DQUOT;
      case EFAULT      : return ERRNO_FAULT;
      case EFBIG       : return ERRNO_FBIG;
      case EINTR       : return ERRNO_INTR;
      case EINVAL      : return ERRNO_INVAL;
      case EIO         : return ERRNO_IO;
      case ENOSPC      : return ERRNO_NOSPC;
      case EPIPE       : return ERRNO_PIPE;
      default: utility::unreachable();
      }
      // clang-format on
    }
    write<std::uint32_t>(LinearMemory, ResultAddress, result);
    return ERRNO_SUCCESS;
  }
  return ERRNO_BADF;
}

std::int32_t args_sizes_get(
    __sable_instance_t *InstancePtr, std::int32_t NumArgAddress,
    std::int32_t BufSizeAddress) {
  auto *LinearMemory = getImplicitLinearMemory(InstancePtr);
  write<wasi_size_t>(LinearMemory, NumArgAddress, 0);
  write<wasi_size_t>(LinearMemory, BufSizeAddress, 0);
  return ERRNO_SUCCESS;
}

std::int32_t args_get(__sable_instance_t *, std::int32_t, std::int32_t) {
  return ERRNO_SUCCESS;
}

std::int32_t clock_time_get(
    __sable_instance_t *InstancePtr, std::int32_t ClockID,
    std::int64_t /* precision */, std::int32_t ResultAddress) {
  auto *LinearMemory = getImplicitLinearMemory(InstancePtr);
  int clock_id{};
  // clang-format off
  switch (ClockID) {
  case CLOCKID_REALTIME          : clock_id = CLOCK_REALTIME          ; break;
  case CLOCKID_MONOTONIC         : clock_id = CLOCK_MONOTONIC         ; break;
  case CLOCKID_PROCESS_CPUTIME_ID: clock_id = CLOCK_PROCESS_CPUTIME_ID; break;
  case CLOCKID_THREAD_CPUTIME_ID : clock_id = CLOCK_THREAD_CPUTIME_ID ; break;
  default: return ERRNO_INVAL;
  }
  // clang-format on
  timespec Time{};
  if (clock_gettime(clock_id, &Time) == -1) {
    switch (errno) {
    case EFAULT: return ERRNO_FAULT;
    case EINVAL: return ERRNO_INVAL;
    default: utility::unreachable();
    }
  }
  wasi_timestamp_t Result = Time.tv_sec * 1'000'000'000 + Time.tv_nsec;
  write<wasi_timestamp_t>(LinearMemory, ResultAddress, Result);
  return ERRNO_SUCCESS;
}

std::int32_t random_get(
    __sable_instance_t *InstancePtr, std::int32_t Buffer,
    std::int32_t BufferLength) {
  auto *LinearMemory = getImplicitLinearMemory(InstancePtr);
  auto buf_len = static_cast<wasi_size_t>(BufferLength);
  std::random_device RandomSource;
  std::array<unsigned, 1> Bytes{};
  auto BytesView = std::as_bytes(std::span{Bytes});
  for (std::size_t I = 0; I < buf_len; ++I) {
    if (I % sizeof(unsigned) == 0) { Bytes[0] = RandomSource(); }
    auto RandomByte = BytesView[I % sizeof(unsigned)];
    write<std::byte>(LinearMemory, I + Buffer, RandomByte);
  }
  return ERRNO_SUCCESS;
}

} // namespace runtime::wasi