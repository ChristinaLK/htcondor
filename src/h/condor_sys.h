/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#if defined(AIX31) || defined(AIX32)
#include "syscall.aix.h"
#endif

#if defined(IRIX331)
#include <sys.s>
#endif

#if !defined(AIX31) && !defined(AIX32)  && !defined(IRIX331)
#include <syscall.h>
#endif

#if defined(AIX31) || defined(AIX32)
extern int	CondorErrno;
#define SET_ERRNO errno = CondorErrno
#else
#define SET_ERRNO
#endif

extern int	errno;

/*
**	System calls can be either:
**		local or remote...
**		recorded or unrecorded...
*/

#define SYS_LOCAL		0x0001		/* Perform local system calls            */
#define SYS_REMOTE		0			/* Comment for calls to SetSyscalls     */
#define SYS_RECORDED	0x0002		/* Record information needed for restart */
#define SYS_UNRECORDED	0			/* Comment for calls to SetSyscalls     */
#define SYS_MAPPED		SYS_RECORDED    /* more descriptive name */
#define SYS_UNMAPPED	SYS_UNRECORDED   /* more descriptive name */

/*
extern int Syscalls;
*/
extern char *CONDOR_SyscallNames[];

/*
**	Fake CONDOR system calls are negative.
**	Virtual system call numbers for actual Unix
**	system calls start at 0.
*/

#define PSEUDO_pvm_task_info	-22
#define PSEUDO_new_connection   -21
#define PSEUDO_pvm_info         -20
#define PSEUDO_subproc_status 	-19
#define PSEUDO_work_request		-18
#define PSEUDO_send_core		-17
#define PSEUDO_perm_error		-16
#define PSEUDO_send_rusage		-15
#define PSEUDO_send_file		-14
#define PSEUDO_get_file			-13
#define PSEUDO_extern_name		-12
#define PSEUDO_free_fs_blocks	-11
#define PSEUDO_image_size		-10
#define PSEUDO_processlogging	-9
#define PSEUDO_reallyexit		-8
#define PSEUDO_updaterusage		-7
#define PSEUDO_getwd        	-6
#define PSEUDO_brk          	-5
#define PSEUDO_getegid      	-4
#define PSEUDO_geteuid      	-3
#define PSEUDO_getppid      	-2
#define PSEUDO_wait3        	-1

#define NFAKESYSCALLS    22

#define NREALSYSCALLS	 242
#define CONDOR_SYSCALLNAME(sysnum) \
			((((sysnum) >= -NFAKESYSCALLS) && (sysnum <= NREALSYSCALLS)) ? \
				CONDOR_SyscallNames[(sysnum)+NFAKESYSCALLS] : \
				"Unknown CONDOR system call number" )

#define CONDOR_accept        0
#define CONDOR_access        1
#define CONDOR_acct          2
#define CONDOR_adjtime       3
#define CONDOR_async_daemon  4
#define CONDOR_bind          5
#define CONDOR_chdir         6
#define CONDOR_chmod         7
#define CONDOR_chown         8
#define CONDOR_chroot        9
#define CONDOR_close         10
#define CONDOR_connect       11
#define CONDOR_creat         12
#define CONDOR_dup           13
#define CONDOR_dup2          14
#define CONDOR_execv         15
#define CONDOR_execve        16
#define CONDOR__exit         17
#define CONDOR_exportfs      18
#define CONDOR_fchmod        19
#define CONDOR_fchown        20
#define CONDOR_fcntl         21
#define CONDOR_flock         22
#define CONDOR_fork          23
#define CONDOR_fstat         24
#define CONDOR_fstatfs       25
#define CONDOR_fsync         26
#define CONDOR_ftruncate     27
#define CONDOR_getdirentries 28
#define CONDOR_getdomainname 29
#define CONDOR_getdopt       30
#define CONDOR_getdtablesize 31
#define CONDOR_getfh         32
#define CONDOR_getgid        33
#define CONDOR_getgroups     34
#define CONDOR_gethostid     35
#define CONDOR_gethostname   36
#define CONDOR_getitimer     37
#define CONDOR_getpagesize   38
#define CONDOR_getpeername   39
#define CONDOR_getpgrp       40
#define CONDOR_getpid        41
#define CONDOR_getpriority   42
#define CONDOR_getrlimit     43
#define CONDOR_getrusage     44
#define CONDOR_getsockname   45
#define CONDOR_getsockopt    46
#define CONDOR_gettimeofday  47
#define CONDOR_getuid        48
#define CONDOR_ioctl         49
#define CONDOR_kill          50
#define CONDOR_killpg        51
#define CONDOR_link          52
#define CONDOR_listen        53
#define CONDOR_lseek         54
#define CONDOR_lstat         55
#define CONDOR_madvise       56
#define CONDOR_mincore       57
#define CONDOR_mkdir         58
#define CONDOR_mknod         59
#define CONDOR_mmap          60
#define CONDOR_mount         61
#define CONDOR_mprotect      62
#define CONDOR_mremap        63
#define CONDOR_munmap        64
#define CONDOR_nfssvc        65
#define CONDOR_open          66
#define CONDOR_pipe          67
#define CONDOR_profil        68
#define CONDOR_ptrace        69
#define CONDOR_quotactl      70
#define CONDOR_read          71
#define CONDOR_readlink      72
#define CONDOR_readv         73
#define CONDOR_reboot        74
#define CONDOR_recv          75
#define CONDOR_recvfrom      76
#define CONDOR_recvmsg       77
#define CONDOR_rename        78
#define CONDOR_rmdir         79
#define CONDOR_sbrk          80
#define CONDOR_select        81
#define CONDOR_send          82
#define CONDOR_sendmsg       83
#define CONDOR_sendto        84
#define CONDOR_setdomainname 85
#define CONDOR_setdopt       86
#define CONDOR_setgroups     87
#define CONDOR_sethostid     88
#define CONDOR_sethostname   89
#define CONDOR_setitimer     90
#define CONDOR_setpgrp       91
#define CONDOR_setpriority   92
#define CONDOR_setregid      93
#define CONDOR_setreuid      94
#define CONDOR_setrlimit     95
#define CONDOR_setsockopt    96
#define CONDOR_settimeofday  97
#define CONDOR_shutdown      98
#define CONDOR_sigblock      99
#define CONDOR_sigpause      100
#define CONDOR_sigreturn     101
#define CONDOR_sigsetmask    102
#define CONDOR_sigstack      103
#define CONDOR_sigvec        104
#define CONDOR_socket        105
#define CONDOR_socketpair    106
#define CONDOR_sstk          107
#define CONDOR_stat          108
#define CONDOR_statfs        109
#define CONDOR_swapon        110
#define CONDOR_symlink       111
#define CONDOR_sync          112
#define CONDOR_truncate      113
#define CONDOR_umask         114
#define CONDOR_unlink        115
#define CONDOR_umount       116
#define CONDOR_utimes        117
#define CONDOR_vhangup       118
#define CONDOR_wait          119
#define CONDOR_write         120
#define CONDOR_writev        121
#define CONDOR_accessx		122
#define CONDOR_audit		123
#define CONDOR_auditbin		124
#define CONDOR_auditevents	125
#define CONDOR_auditlog		126
#define CONDOR_auditobj		127
#define CONDOR_auditproc	128
#define CONDOR_brk			129
#define CONDOR_chacl		130
#define CONDOR_chownx		131
#define CONDOR_chpriv		132
#define CONDOR_disclaim		133
#define CONDOR_faccessx		134
#define CONDOR_fchacl		135
#define CONDOR_fchownx		136
#define CONDOR_fchpriv		137
#define CONDOR_fclear		138
#define CONDOR_frevoke		139
#define CONDOR_fscntl		140
#define CONDOR_fstatacl		141
#define CONDOR_fstatpriv	142
#define CONDOR_fstatx		143
#define CONDOR_getargs		144
#define CONDOR_getdirent	145
#define CONDOR_getevars		146
#define CONDOR_getgidx		147
#define CONDOR_kgetpgrp		148
#define CONDOR_getpri		149
#define CONDOR_getpriv		150
#define CONDOR_getproc		151
#define CONDOR_getuidx		152
#define CONDOR_getuser		153
#define CONDOR_kfcntl		154
#define CONDOR_kioctl		155
#define CONDOR_knlist		156
#define CONDOR_kreadv		157
#define CONDOR_kwaitpid		158
#define CONDOR_kwritev		159
#define CONDOR_load			160
#define CONDOR_loadbind		161
#define CONDOR_loadquery	162
#define CONDOR_mntctl		163
#define CONDOR_openx		164
#define CONDOR_pause		165
#define CONDOR_plock		166
#define CONDOR_poll			167
#define CONDOR_privcheck	168
#define CONDOR_psdanger		169
#define CONDOR_revoke		170
#define CONDOR_absinterval	171
#define CONDOR_getinterval	172
#define CONDOR_gettimer		173
#define CONDOR_gettimerid	174
#define CONDOR_incinterval	175
#define CONDOR_reltimerid	176
#define CONDOR_resabs		177
#define CONDOR_resinc		178
#define CONDOR_restimer		179
#define CONDOR_settimer		180
#define CONDOR_nsleep		181
#define CONDOR_seteuid		182
#define CONDOR_setgid		183
#define CONDOR_setgidx		184
#define CONDOR_setpgid		185
#define CONDOR_setpri		186
#define CONDOR_setpriv		187
#define CONDOR_setsid		188
#define CONDOR_setuid		189
#define CONDOR_setuidx		190
#define CONDOR_shmctl		191
#define CONDOR_shmget		192
#define CONDOR_shmat		193
#define CONDOR_shmdt		194
#define CONDOR_msgctl		195
#define CONDOR_msgget		196
#define CONDOR_msgsnd		197
#define CONDOR_msgrcv		198
#define CONDOR_msgxrcv		199
#define CONDOR_semctl		200
#define CONDOR_semget		201
#define CONDOR_semop		202
#define CONDOR_sigaction	203
#define CONDOR_sigcleanup	204
#define CONDOR_sigprocmask	205
#define CONDOR_sigsuspend	206
#define CONDOR_sigpending	207
#define CONDOR_statacl		208
#define CONDOR_statpriv		209
#define CONDOR_statx		210
#define CONDOR_swapoff		211
#define CONDOR_swapqry		212
#define CONDOR_sysconfig	213
#define CONDOR_times		214
#define CONDOR_ulimit		215
#define CONDOR_uname		216
#define CONDOR_unameu		217
#define CONDOR_unamex		218
#define CONDOR_unload		219
#define CONDOR_usrinfo		220
#define CONDOR_ustat		221
#define CONDOR_uvmount		222
#define CONDOR_vmount		223
#define CONDOR_Trconflag	224
#define CONDOR_trchook		225
#define CONDOR_trchk		226
#define CONDOR_trchkt		227
#define CONDOR_trchkl		228
#define CONDOR_trchklt		229
#define CONDOR_trchkg		230
#define CONDOR_trchkgt		231
#define CONDOR_trcgen		232
#define CONDOR_trcgent		233
#define CONDOR_getppid		234
#define CONDOR_lockf		235
#define CONDOR_unmount		236
#define CONDOR_sigvector	237
#define CONDOR_getpgrp2		238
#define CONDOR_utssys		239
#define CONDOR_setresuid	240
#define CONDOR_setresgid	241
#define CONDOR_utime		242

#define CONDOR__load	CONDOR_load
