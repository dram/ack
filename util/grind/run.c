/* $Header$ */

/* Running a process and communication */

#include <signal.h>
#include <stdio.h>
#include <assert.h>
#include <alloc.h>

#include "ops.h"
#include "message.h"
#include "position.h"
#include "tree.h"
#include "file.h"
#include "symbol.h"
#include "idf.h"
#include "scope.h"
#include "type.h"
#include "expr.h"

#define MAXARG	128

extern char	*strncpy();
extern struct idf *str2idf();

extern char	*AObj;
extern FILE	*db_out;
extern int	debug;
extern long	pointer_size;
extern char	*progname;
extern int	child_interrupted;
extern int	interrupted;
extern int	stop_reason;
extern t_lineno	currline;

static int	child_pid;		/* process id of child */
static int	to_child, from_child;	/* file descriptors for communication */
static int	child_status;
static int	restoring;
static int	fild1[2], fild2[2];	/* pipe file descriptors */
int		disable_intr = 1;

int		db_ss;

static int	catch_sigpipe();
static int	stopped();
static int	uputm(), ugetm();
static t_addr	curr_stop;
p_tree		run_command;

int
init_run()
{
  /* take file descriptors so that listing cannot take them */
  int i;

  for (i = IN_FD; i <= OUT_FD; i++) close(i);
  if (pipe(fild1) < 0 ||
      pipe(fild2) < 0 ||
      fild1[0] != IN_FD ||
      fild2[1] != OUT_FD) {
	return 0;
  }
  to_child = fild1[1];
  from_child = fild2[0];
  child_pid = 0;
  if (currfile) CurrentScope = currfile->sy_file->f_scope;
  currline = 0;
  return 1;
}

extern int errno;

int
start_child(p)
  p_tree	p;
{
  /* start up the process to be debugged and set up communication */

  char *argp[MAXARG];				/* argument list */
  register p_tree pt = p->t_args[0], pt1;
  unsigned int	nargs = 1;			/* #args */
  char	*in_redirect = 0;			/* standard input redirected */
  char	*out_redirect = 0;			/* standard output redirected */

  signal_child(SIGKILL); /* like families in China, this debugger is only
			    allowed one child
			 */

  if (p != run_command) {
	freenode(run_command);
	run_command = p;
  }
  /* first check arguments and redirections and build argument list */
  while (pt) {
  	switch(pt->t_oper) {
	case OP_LINK:
		pt1 = pt->t_args[1];
		pt = pt->t_args[0];
		continue;
	case OP_NAME:
		if (nargs < (MAXARG-1)) {
			argp[nargs++] = pt->t_str;
		}
		else {
			error("too many arguments");
			return 0;
		}
		break;
	case OP_INPUT:
		if (in_redirect) {
			error("input redirected twice?");
			return 0;
		}
		in_redirect = pt->t_str;
		break;
	case OP_OUTPUT:
		if (out_redirect) {
			error("output redirected twice?");
			return 0;
		}
		out_redirect = pt->t_str;
		break;
  	}
	if (pt != pt1) pt = pt1;
	else break;
  }
  argp[0] = AObj;
  argp[nargs] = 0;

  /* create child process */
  child_pid = fork();
  if (child_pid < 0) {
	error("could not create child");
	return 0;
  }
  if (child_pid == 0) {
	/* this is the child process */
	close(fild1[1]);
	close(fild2[0]);

	signal(SIGINT, SIG_IGN);

	/* I/O redirection */
	if (in_redirect) {
		int fd;

		close(0);
		if ((fd = open(in_redirect, 0)) < 0 ||
		    (fd != 0 && dup2(fd, 0) < 0)) {
			perror(progname);
			exit(1);
		}
		if (fd != 0) {
			close(fd);
		}
	}
	if (out_redirect) {
		int fd;

		close(1);
		if ((fd = creat(out_redirect, 0666)) < 0 ||
		    (fd != 1 && dup2(fd, 1) < 0)) {
			perror(progname);
			exit(1);
		}
		if (fd != 1) {
			close(fd);
		}
	}

	/* and run process to be debugged */
	execv(AObj, argp);
	error("could not exec %s", AObj);
	exit(1);
  }

  /* debugger */
  close(fild1[0]);
  close(fild2[1]);

  pipe(fild1);		/* to occupy file descriptors */
  signal(SIGPIPE, catch_sigpipe);
  {
	struct message_hdr m;

  	if (! ugetm(&m)) {
		error("child not responding");
		init_run();
		return 0;
	}
	curr_stop = m.m_size;
	CurrentScope = get_scope_from_addr(curr_stop);
  }
  do_items();
  if (! restoring && ! item_addr_actions(curr_stop, OK, 1)) {
	send_cont(1);
  }
  else if (! restoring) {
	stopped("stopped", curr_stop);
  }
  return 1;
}

signal_child(sig)
{
  if (child_pid) {
	kill(child_pid, sig);
	if (sig == SIGKILL) {
		wait(&child_status);
		init_run();
	}
  }
}

static int
catch_sigpipe()
{
  child_pid = 0;
}

static int
ureceive(p, c)
  char	*p;
  long	c;
{
  int	i;
  char buf[0x1000];

  if (! child_pid) {
	error("no process");
  	return 0;
  }

  if (! p) p = buf;
  while (c >= 0x1000) {
	i = read(from_child, p, 0x1000);
	if (i <= 0) {
		if (i == 0) {
			child_pid = 0;
		}
		else	error("read failed");
		return 0;
	}
	if (p != buf) p += i;
	c -= i;
  }
  while (c > 0) {
	i = read(from_child, p, (int)c);
	if (i <= 0) {
		if (i == 0) {
			child_pid = 0;
		}
		else	error("read failed");
		return 0;
	}
	p += i;
	c -= i;
  }
  return 1;
}

static int
usend(p, c)
  char	*p;
  long	c;
{
  int	i;

  if (! child_pid) {
	error("no process");
	return 0;
  }
  while (c >= 0x1000) {
	i = write(to_child, p, 0x1000);
	if (i < 0) {
		if (child_pid) error("write failed");
		return 0;
	}
	p += i;
	c -= i;
  }
  while (c > 0) {
	i = write(to_child, p, (int)c);
	if (i < 0) {
		if (child_pid) error("write failed");
		return 0;
	}
	p += i;
	c -= i;
  }
  return 1;
}

static int
ugetm(message)
  struct message_hdr *message;
{
  if (! ureceive((char *) message, (long) sizeof(struct message_hdr))) {
  	return 0;
  }
  if (debug) printf("Got %d\n", message->m_type);
  return 1;
}

static int
uputm(message)
  struct message_hdr *message;
{
  if (! usend((char *) message, (long) sizeof(struct message_hdr))) {
  	return 0;
  }
  if (debug) printf("Sent %d\n", message->m_type);
  return 1;
}

static struct message_hdr	answer;
static int	single_stepping;

static int
stopped(s, a)
  char	*s;	/* stop message */
  t_addr a;	/* address where stopped */
{
  p_position pos;

  if (s && a) {
	fprintf(db_out, "%s ", s);
	pos = print_position(a, 1);
	if (stop_reason) {
		fprintf(db_out, " (status entry %d)", stop_reason);
	}
	fputs("\n", db_out);
	list_position(pos);
	handle_displays();
  }
  curr_stop = a;
  CurrentScope = get_scope_from_addr(a);
  return 1;
}

static int
could_send(m, stop_message)
  struct message_hdr	*m;
{
  int	type;
  t_addr a;
  static int level = 0;
  int child_dead = 0;

  level++;
  for (;;) {
  	if (! child_pid) {
		error("no process");
		return 0;
	}
	if (m->m_type & DB_RUN) {
		disable_intr = 0;
		stop_reason = 0;
	}
	if (!child_interrupted && (! uputm(m) || ! ugetm(&answer))) {
		child_dead = 1;
	}
	disable_intr = 1;
	if ((interrupted || child_interrupted) && ! child_dead) {
		while (child_interrupted && answer.m_type != INTR) {
			if (! ugetm(&answer)) {
				child_dead = 1;
				break;
			}
		}
		if (interrupted && ! child_dead) {
			level--;
			if (! level) {
				child_interrupted = 0;
				interrupted = 0;
				stopped("interrupted", (t_addr) answer.m_size);
			}
			return 1;
		}
	}
	if (child_dead) {
		wait(&child_status);
		if (child_status & 0177) {
			fprintf(db_out,
				"child died with signal %d\n",
				child_status & 0177);
		}
		else {
			fprintf(db_out,
				"child terminated, exit status %d\n",
				child_status >> 8);
		}
		init_run();
		level--;
		return 1;
	}
	a = answer.m_size;
	type = answer.m_type;
	if (m->m_type & DB_RUN) {
		/* run command */
		CurrentScope = get_scope_from_addr((t_addr) a);
	    	if (! item_addr_actions(a, type, stop_message) &&
	            ( type == DB_SS || type == OK)) {
			/* no explicit breakpoints at this position.
			   Also, child did not stop because of
			   SETSS or SETSSF, otherwise we would
			   have gotten END_SS.
			   So, continue.
			*/
			if ((m->m_type & ~ DB_SS) != CONT) {
				m->m_type = CONT | (m->m_type & DB_SS);
			}
			continue;
		}
		if (type != END_SS && single_stepping) {
			m->m_type = CLRSS;
			if (! uputm(m) || ! ugetm(&answer)) return 0;
		}
		single_stepping = 0;
	}
	if (stop_message) {
		stopped("stopped", a);
	}
	level--;
	return 1;
  }
  /*NOTREACHED*/
}

static int
getbytes(size, from, to, kind)
  long	size;
  t_addr from;
  char	*to;
{
  struct message_hdr	m;

  m.m_type = kind;
  m.m_size = size;
  put_int(m.m_buf, pointer_size, (long)from);

  if (! could_send(&m, 0)) {
	return 0;
  }

  switch(answer.m_type) {
  case FAIL:
	error("could not get value");
	return 0;
  case INTR:
	error("interrupted");
	return 0;
  case DATA:
  	return ureceive(to, answer.m_size);
  default:
	assert(0);
  }
  /*NOTREACHED*/
}

int
get_bytes(size, from, to)
  long	size;
  t_addr from;
  char	*to;
{
  return getbytes(size, from, to, GETBYTES);
}

int
get_string(size, from, to)
  long	size;
  t_addr from;
  char	*to;
{
  int retval = getbytes(size, from, to, GETSTR);

  to[(int)answer.m_size] = 0;
  return retval;
}

set_bytes(size, from, to)
  long	size;
  char	*from;
  t_addr to;
{
  struct message_hdr	m;

  m.m_type = SETBYTES;
  m.m_size = size;
  put_int(m.m_buf, pointer_size, (long) to);

  if (! uputm(&m) || ! usend(from, size) || ! ugetm(&m)) {
	return;
  }
  switch(answer.m_type) {
  case FAIL:
	error("could not handle this SET request");
	break;
  case INTR:
	error("interrupted");
	break;
  case OK:
	break;
  default:
	assert(0);
  }
}

int
get_dump(globmessage, globbuf, stackmessage, stackbuf)
  struct message_hdr *globmessage, *stackmessage;
  char **globbuf, **stackbuf;
{
  struct message_hdr	m;

  m.m_type = DUMP;
  if (! could_send(&m, 0)) {
	return 0;
  }
  switch(answer.m_type) {
  case FAIL:
	error("request for DUMP failed");
	return 0;
  case INTR:
	error("interrupted");
	return 0;
  case DGLOB:
	break;
  default:
	assert(0);
  }

  *globmessage = answer;
  *globbuf = malloc((unsigned) answer.m_size);
  if (! ureceive(*globbuf, answer.m_size) || ! ugetm(stackmessage)) {
	if (*globbuf) free(*globbuf);
	return 0;
  }
  assert(stackmessage->m_type == DSTACK);
  *stackbuf = malloc((unsigned) stackmessage->m_size);
  if (! ureceive(*stackbuf, stackmessage->m_size)) {
	if (*globbuf) free(*globbuf);
	if (*stackbuf) free(*stackbuf);
	return 0;
  }
  put_int(globmessage->m_buf+SP_OFF*pointer_size, pointer_size,
	 get_int(stackmessage->m_buf+SP_OFF*pointer_size, pointer_size, T_UNSIGNED));
  if (! *globbuf || ! *stackbuf) {
	error("could not allocate enough memory");
	if (*globbuf) free(*globbuf);
	if (*stackbuf) free(*stackbuf);
	return 0;
  }
  return 1;
}

int
put_dump(globmessage, globbuf, stackmessage, stackbuf)
  struct message_hdr *globmessage, *stackmessage;
  char *globbuf, *stackbuf;
{
  struct message_hdr m;
  int retval;

  if (! child_pid) {
	restoring = 1;
	start_child(run_command);
	restoring = 0;
  }
  retval =	uputm(globmessage) && usend(globbuf, globmessage->m_size) &&
		uputm(stackmessage) && usend(stackbuf, stackmessage->m_size) &&
		ugetm(&m) && stopped("restored", m.m_size);
  return retval;
}

t_addr *
get_EM_regs(level)
  int	level;
{
  struct message_hdr	m;
  static t_addr buf[5];
  register t_addr *to = &buf[0];

  m.m_type = GETEMREGS;
  m.m_size = level;

  if (! could_send(&m, 0)) {
	return 0;
  }
  switch(answer.m_type) {
  case FAIL:
	error("request for registers failed");
	return 0;
  case INTR:
	error("interrupted");
	return 0;
  case GETEMREGS:
	break;
  default:
	assert(0);
  }
  *to++ = (t_addr) get_int(answer.m_buf, pointer_size, T_UNSIGNED);
  *to++ = (t_addr) get_int(answer.m_buf+pointer_size, pointer_size, T_UNSIGNED);
  *to++ = (t_addr) get_int(answer.m_buf+2*pointer_size, pointer_size, T_UNSIGNED);
  *to++ = (t_addr) get_int(answer.m_buf+3*pointer_size, pointer_size, T_UNSIGNED);
  *to++ = (t_addr) get_int(answer.m_buf+4*pointer_size, pointer_size, T_UNSIGNED);
  return buf;
}

int
set_pc(PC)
  t_addr	PC;
{
  struct message_hdr	m;

  m.m_type = SETEMREGS;
  m.m_size = 0;
  put_int(m.m_buf+PC_OFF*pointer_size, pointer_size, (long)PC);
  if (! could_send(&m, 0)) return 0;
  switch(answer.m_type) {
  case FAIL:
	error("could not set PC to %lx", (long) PC);
	return 0;
  case INTR:
	error("interrupted");
	return 0;
  case OK:
	return 1;
  default:
	assert(0);
  }
  /*NOTREACHED*/
}

int
send_cont(stop_message)
  int	stop_message;
{
  struct message_hdr	m;

  m.m_type = (CONT | (db_ss ? DB_SS : 0));
  m.m_size = 0;
  return could_send(&m, stop_message) && child_pid;
}

int
do_single_step(type, count)
  int	type;
  long	count;
{
  struct message_hdr	m;

  m.m_type = type | (db_ss ? DB_SS : 0);
  m.m_size = count;
  single_stepping = 1;
  if (could_send(&m, 1) && child_pid) return 1;
  single_stepping = 0;
  return 0;
}

int
set_or_clear_breakpoint(a, type)
  t_addr	a;
  int	type;
{
  struct message_hdr m;

  m.m_type = type;
  m.m_size = a;
  if (debug) printf("%s breakpoint at 0x%lx\n", type == SETBP ? "setting" : "clearing", (long) a);
  if (child_pid && ! could_send(&m, 0)) {
  }

  return 1;
}

int
set_or_clear_trace(start, end, type)
  t_addr start, end;
  int	type;
{
  struct message_hdr m;

  m.m_type = type;
  put_int(m.m_buf, pointer_size, (long)start);
  put_int(m.m_buf+pointer_size, pointer_size, (long)end);
  if (debug) printf("%s trace at [0x%lx,0x%lx]\n", type == SETTRACE ? "setting" : "clearing", (long) start, (long) end);
  if (child_pid && ! could_send(&m, 0)) {
	return 0;
  }

  return 1;
}
