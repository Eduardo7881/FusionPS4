#include "syscall/freebsd/FreeBsd.hpp"

#include <array>
#include <utility>

namespace fusionps4::syscall::freebsd {

namespace {

constexpr std::pair<std::int64_t, const char*> kErrnoNames[] = {
    {kOk, "0"},           {kEperm, "EPERM"},       {kEnoent, "ENOENT"},
    {kEsrch, "ESRCH"},    {kEintr, "EINTR"},       {kEio, "EIO"},
    {kEnxio, "ENXIO"},    {kE2big, "E2BIG"},       {kEnoexec, "ENOEXEC"},
    {kEbadf, "EBADF"},    {kEchild, "ECHILD"},     {kEagain, "EAGAIN"},
    {kEnomem, "ENOMEM"},  {kEacces, "EACCES"},     {kEfault, "EFAULT"},
    {kEnotblk, "ENOTBLK"},{kEbusy, "EBUSY"},       {kEexists, "EEXIST"},
    {kExdev, "EXDEV"},    {kEnodev, "ENODEV"},     {kEnotdir, "ENOTDIR"},
    {kEisdir, "EISDIR"},  {kEinval, "EINVAL"},     {kEnfile, "ENFILE"},
    {kEmfile, "EMFILE"},  {kEnotty, "ENOTTY"},     {kEtxtbsy, "ETXTBSY"},
    {kEfbig, "EFBIG"},    {kEnospc, "ENOSPC"},     {kEspipe, "ESPIPE"},
    {kErofs, "EROFS"},    {kEmlink, "EMLINK"},     {kEpipe, "EPIPE"},
    {kEdom, "EDOM"},      {kErange, "ERANGE"},     {kEdeadlk, "EDEADLK"},
    {kEnolck, "ENOLCK"},  {kEnosys, "ENOSYS"},     {kEnametoolong, "ENAMETOOLONG"},
    {kEnotempty, "ENOTEMPTY"}, {kElibbad, "ELIBBAD"}, {kEopnotsupp, "EOPNOTSUPP"},
};

} // namespace

const char* ErrnoName::name(std::int64_t e) {
    for (const auto& [code, str] : kErrnoNames) {
        if (code == e) return str;
    }
    return "UNKNOWN";
}

} // namespace fusionps4::syscall::freebsd
