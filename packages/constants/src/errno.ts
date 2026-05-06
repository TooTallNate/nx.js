/** Not owner. */
export const EPERM = 1;
/** No such file or directory. */
export const ENOENT = 2;
/** No such process. */
export const ESRCH = 3;
/** Interrupted system call. */
export const EINTR = 4;
/** I/O error. */
export const EIO = 5;
/** No such device or address. */
export const ENXIO = 6;
/** Arg list too long. */
export const E2BIG = 7;
/** Exec format error. */
export const ENOEXEC = 8;
/** Bad file number. */
export const EBADF = 9;
/** No children. */
export const ECHILD = 10;
/** No more processes. */
export const EAGAIN = 11;
/** Not enough space. */
export const ENOMEM = 12;
/** Permission denied. */
export const EACCES = 13;
/** Bad address. */
export const EFAULT = 14;
/** Block device required. */
export const ENOTBLK = 15;
/** Device or resource busy. */
export const EBUSY = 16;
/** File exists. */
export const EEXIST = 17;
/** Cross-device link. */
export const EXDEV = 18;
/** No such device. */
export const ENODEV = 19;
/** Not a directory. */
export const ENOTDIR = 20;
/** Is a directory. */
export const EISDIR = 21;
/** Invalid argument. */
export const EINVAL = 22;
/** Too many open files in system. */
export const ENFILE = 23;
/** File descriptor value too large. */
export const EMFILE = 24;
/** Not a character device. */
export const ENOTTY = 25;
/** Text file busy. */
export const ETXTBSY = 26;
/** File too large. */
export const EFBIG = 27;
/** No space left on device. */
export const ENOSPC = 28;
/** Illegal seek. */
export const ESPIPE = 29;
/** Read-only file system. */
export const EROFS = 30;
/** Too many links. */
export const EMLINK = 31;
/** Broken pipe. */
export const EPIPE = 32;
/** Mathematics argument out of domain of function. */
export const EDOM = 33;
/** Result too large. */
export const ERANGE = 34;
/** No message of desired type. */
export const ENOMSG = 35;
/** Identifier removed. */
export const EIDRM = 36;
/** Channel number out of range. */
export const ECHRNG = 37;
/** Level 2 not synchronized. */
export const EL2NSYNC = 38;
/** Level 3 halted. */
export const EL3HLT = 39;
/** Level 3 reset. */
export const EL3RST = 40;
/** Link number out of range. */
export const ELNRNG = 41;
/** Protocol driver not attached. */
export const EUNATCH = 42;
/** No CSI structure available. */
export const ENOCSI = 43;
/** Level 2 halted. */
export const EL2HLT = 44;
/** Deadlock. */
export const EDEADLK = 45;
/** No lock. */
export const ENOLCK = 46;
/** Invalid exchange. */
export const EBADE = 50;
/** Invalid request descriptor. */
export const EBADR = 51;
/** Exchange full. */
export const EXFULL = 52;
/** No anode. */
export const ENOANO = 53;
/** Invalid request code. */
export const EBADRQC = 54;
/** Invalid slot. */
export const EBADSLT = 55;
/** File locking deadlock error. */
export const EDEADLOCK = 56;
/** Bad font file fmt. */
export const EBFONT = 57;
/** Not a stream. */
export const ENOSTR = 60;
/** No data (for no delay io). */
export const ENODATA = 61;
/** Stream ioctl timeout. */
export const ETIME = 62;
/** No stream resources. */
export const ENOSR = 63;
/** Machine is not on the network. */
export const ENONET = 64;
/** Package not installed. */
export const ENOPKG = 65;
/** The object is remote. */
export const EREMOTE = 66;
/** Virtual circuit is gone. */
export const ENOLINK = 67;
/** Advertise error. */
export const EADV = 68;
/** Srmount error. */
export const ESRMNT = 69;
/** Communication error on send. */
export const ECOMM = 70;
/** Protocol error. */
export const EPROTO = 71;
/** Multihop attempted. */
export const EMULTIHOP = 74;
/** Inode is remote (not really error). */
export const ELBIN = 75;
/** Cross mount point (not really error). */
export const EDOTDOT = 76;
/** Bad message. */
export const EBADMSG = 77;
/** Inappropriate file type or format. */
export const EFTYPE = 79;
/** Given log. name not unique. */
export const ENOTUNIQ = 80;
/** f.d. invalid for this operation. */
export const EBADFD = 81;
/** Remote address changed. */
export const EREMCHG = 82;
/** Can't access a needed shared lib. */
export const ELIBACC = 83;
/** Accessing a corrupted shared lib. */
export const ELIBBAD = 84;
/** .lib section in a.out corrupted. */
export const ELIBSCN = 85;
/** Attempting to link in too many libs. */
export const ELIBMAX = 86;
/** Attempting to exec a shared library. */
export const ELIBEXEC = 87;
/** Function not implemented. */
export const ENOSYS = 88;
/** No more files. */
export const ENMFILE = 89;
/** Directory not empty. */
export const ENOTEMPTY = 90;
/** File or path name too long. */
export const ENAMETOOLONG = 91;
/** Too many symbolic links. */
export const ELOOP = 92;
/** Operation not supported on socket. */
export const EOPNOTSUPP = 95;
/** Protocol family not supported. */
export const EPFNOSUPPORT = 96;
/** Connection reset by peer. */
export const ECONNRESET = 104;
/** No buffer space available. */
export const ENOBUFS = 105;
/** Address family not supported by protocol family. */
export const EAFNOSUPPORT = 106;
/** Protocol wrong type for socket. */
export const EPROTOTYPE = 107;
/** Socket operation on non-socket. */
export const ENOTSOCK = 108;
/** Protocol not available. */
export const ENOPROTOOPT = 109;
/** Can't send after socket shutdown. */
export const ESHUTDOWN = 110;
/** Connection refused. */
export const ECONNREFUSED = 111;
/** Address already in use. */
export const EADDRINUSE = 112;
/** Software caused connection abort. */
export const ECONNABORTED = 113;
/** Network is unreachable. */
export const ENETUNREACH = 114;
/** Network interface is not configured. */
export const ENETDOWN = 115;
/** Connection timed out. */
export const ETIMEDOUT = 116;
/** Host is down. */
export const EHOSTDOWN = 117;
/** Host is unreachable. */
export const EHOSTUNREACH = 118;
/** Connection already in progress. */
export const EINPROGRESS = 119;
/** Socket already connected. */
export const EALREADY = 120;
/** Destination address required. */
export const EDESTADDRREQ = 121;
/** Message too long. */
export const EMSGSIZE = 122;
/** Unknown protocol. */
export const EPROTONOSUPPORT = 123;
/** Socket type not supported. */
export const ESOCKTNOSUPPORT = 124;
/** Address not available. */
export const EADDRNOTAVAIL = 125;
/** Connection aborted by network. */
export const ENETRESET = 126;
/** Socket is already connected. */
export const EISCONN = 127;
/** Socket is not connected. */
export const ENOTCONN = 128;
/** Too many references: cannot splice. */
export const ETOOMANYREFS = 129;
/** Too many processes. */
export const EPROCLIM = 130;
/** Too many users. */
export const EUSERS = 131;
/** Disk quota exceeded. */
export const EDQUOT = 132;
/** Stale file handle. */
export const ESTALE = 133;
/** Not supported. */
export const ENOTSUP = 134;
/** No medium (in tape drive). */
export const ENOMEDIUM = 135;
/** No such host or network path. */
export const ENOSHARE = 136;
/** Filename exists with different case. */
export const ECASECLASH = 137;
/** Illegal byte sequence. */
export const EILSEQ = 138;
/** Value too large for defined data type. */
export const EOVERFLOW = 139;
/** Operation canceled. */
export const ECANCELED = 140;
/** State not recoverable. */
export const ENOTRECOVERABLE = 141;
/** Previous owner died. */
export const EOWNERDEAD = 142;
/** Streams pipe error. */
export const ESTRPIPE = 143;