/* 
   Copyright (C) 2013

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <asterisk.h>
#include <asterisk/utils.h>     /* ast_free() */

#include "channel.h"
#include "at_log.h"

#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>


static int log_fd = -1;


static int
get_logfd (void)
{
  if (log_fd < 0)
    log_fd =
      open ("/var/log/asterisk/dongle.log", O_WRONLY | O_APPEND | O_CREAT,
            0666);
  if (log_fd < 0)
    return -1;
  return 0;

}


static char *
now (void)
{
  static char ret[1024];
  char *ptr;

  time_t t;
  time (&t);


  ret[0] = 0;
  ptr = ctime (&t);

  if (ptr)
    strncpy (ret, ptr, sizeof (ret));

  ptr = index ((char *) ret, '\n');
  if (ptr)
    *ptr = 0;

  ptr = index ((char *) ret, '\r');
  if (ptr)
    *ptr = 0;

  return ret;
}



static void
append_str (unsigned char **buf, size_t * len, const char *_ptr)
{
  const unsigned char *ptr = (unsigned char *) _ptr;
  while (*ptr && *len)
    {
      *((*buf)++) = *(ptr++);
      (*len)--;
    }
}

static void
append_escape (unsigned char **buf, size_t * len, unsigned char c)
{
  char e[8];

  switch (c)
    {
    case '\0':
      append_str (buf, len, "\\0");
      break;
    case '\r':
      append_str (buf, len, "\\r");
      break;
    case '\n':
      append_str (buf, len, "\\n");
      break;
    case '\t':
      append_str (buf, len, "\\t");
      break;
    default:
      snprintf (e, sizeof (e), "\\x%02x", c);
      append_str (buf, len, e);
    }
}

static void
append_buf (unsigned char **buf, size_t * len, unsigned char *ptr,
            size_t alen)
{
  while (alen && *len)
    {
      if (((*ptr < 32) || (*ptr > 126)) && (*ptr != '\\'))
        {
          append_escape (buf, len, *(ptr++));
        }
      else
        {
          *((*buf)++) = *(ptr++);
          (*len)--;
        }
      alen--;
    }
}




EXPORT_DECL void
at_logv (struct pvt *pvt, const char *prefix, const struct iovec *iov,
         size_t iov_count)
{
  unsigned char buf[4096];
  unsigned char *ptr = buf;
  size_t blen = sizeof (buf);

  if (get_logfd ())
    return;

  blen-=2;

  append_str (&ptr, &blen, now ());

  append_str (&ptr, &blen, " [");
  if (pvt)
    append_str (&ptr, &blen, PVT_ID (pvt));
  else
    append_str (&ptr, &blen, "unknown");
  append_str (&ptr, &blen, "] ");
  append_str (&ptr, &blen, prefix);
  append_str (&ptr, &blen, " ");

  while (iov_count--)
    {
      append_buf (&ptr, &blen, (unsigned char *) iov->iov_base, iov->iov_len);
      iov++;
    }
  *(ptr++) = '\n';

  write (log_fd, buf, (size_t) (ptr - buf));
}

EXPORT_DECL void
at_log (struct pvt *pvt, const char *prefix, const void *buf, size_t len)
{
  struct iovec iov[1];

  iov->iov_base = buf;
  iov->iov_len = len;

  at_logv (pvt, prefix, iov, 1);
}
