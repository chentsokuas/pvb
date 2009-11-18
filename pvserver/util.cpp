/***************************************************************************
                          util.cpp  -  description
                             -------------------
    begin                : Sun Nov 12 2000
    copyright            : (C) 2000 by Rainer Lehrig
                         : Angel Maza
                         : Martin Bangieff
    email                : lehrig@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <locale.h>

/* #include "qimage.h" */
#include <time.h>
#include <sys/stat.h>

#ifndef IS_OLD_MSVCPP
#define AF_INET6_IS_AVAILABLE
#endif

#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#ifdef IS_OLD_MSVCPP
#include <winsock.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <io.h>
#include <direct.h>
#define  MSG_NOSIGNAL 0
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include "unistd.h"
#define  closesocket close
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include "processviewserver.h"
#include "pvbImage.h"
#ifdef __VMS
#define MSG_NOSIGNAL 0
#include "ucx$inetdef.h"
#include <pthread.h>
#endif

#ifdef PVUNIX
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <utime.h>
#include <unistd.h>
#include <dirent.h>
#endif

#ifdef PVWIN32
#include <sys/types.h>
#include <time.h>
#include <sys/utime.h>
#include <io.h>
#endif

#ifdef __VMS
#include <descrip.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <iodef.h>
#include <unixio.h>
#include <file.h>
#include <lib$routines.h>
#include <starlet.h>
#include <libdef.h>
#include <jpidef.h>
typedef struct
{
  short year;
  short month;
  short day;
  short hour;
  short min;
  short sec;
  short hth;
}
TDS;
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

int num_threads = 0;
int exit_after_last_client_terminates = 0; // can be set from command line
int pv_cache = 0;                          // can be set from command line
static int rl_ipversion = 4;               // default: use IPV4

#ifndef USE_INETD
extern pthread_mutex_t param_mutex;
#endif
static int serversocket = -1;
static int clientsocket[MAX_CLIENTS];
static char last_event[MAX_EVENT_LENGTH] = "";
static char this_event[MAX_EVENT_LENGTH] = "";

// gcc 4.4 workarounds
// they modified prototypes of str functions :-(
static char *mystrstr(const char *s1, const char *s2)
{
  char *ret;
  const char *cptr = strstr(s1,s2);
  memcpy(&ret, &cptr, sizeof(ret));
  return ret;
}

static char *mystrchr(const char *s1, int c)
{
  char *ret;
  const char *cptr = strchr(s1,c);
  memcpy(&ret, &cptr, sizeof(ret));
  return ret;
}

static char *mystrrchr(const char *s1, int c)
{
  char *ret;
  const char *cptr = strrchr(s1,c);
  memcpy(&ret, &cptr, sizeof(ret));
  return ret;
}

int pvlock(PARAM *p)
{
  if(p == NULL) return -1;
#ifndef USE_INETD
  if(p->my_pvlock_count == 0)
  {
    p->my_pvlock_count++;
    pvthread_mutex_lock(&param_mutex);
  }  
#endif
  return 0;
}

int pvunlock(PARAM *p)
{
  if(p == NULL) return -1;
#ifndef USE_INETD
  p->my_pvlock_count--;
  if(p->my_pvlock_count < 0) p->my_pvlock_count = 0;
  if(p->my_pvlock_count == 0)
  {
    pvthread_mutex_unlock(&param_mutex);
  }  
#endif
  return 0;
}

static int getLocalMilliseconds()
{
  int millisecond, second;
#ifdef PVUNIX
  time_t t;
  struct tm       *tms;
  struct timeval  tv;
  struct timezone tz;

  time(&t);
  tms = localtime(&t);
  gettimeofday(&tv, &tz);

  /* adjust year and month */
  tms->tm_year += 1900;
  tms->tm_mon  += 1;

  millisecond = (int)tv.tv_usec / 1000;
  second      = (int)tms->tm_sec;
#endif

#ifdef __VMS
  TDS    tds;
  sys$numtim(&tds, 0);
  millisecond  = (int)tds.hth * 10;
  second       = (int)tds.sec;
#endif

#ifdef PVWIN32
  SYSTEMTIME st;
  GetLocalTime(&st);
  millisecond  = st.wMilliseconds;
  second       = st.wSecond;
#endif

  return (second*1000) + millisecond;
}

static int pvFilePrefix(PARAM *p)
{
  int ret;
  char temp[32];
  char *cptr;
  FILE *fout;

  cptr = NULL;
  fout = NULL;
  sprintf(p->file_prefix,"PVTMP%06d",p->s);
  strcpy(temp,"PVTMPXXXXXX");

#ifdef PVUNIX
  ret = mkstemp(temp);
  if(ret < 0) return -1;
  close(ret);
  strcpy(p->file_prefix,temp);
#endif

#ifdef PVWIN32
  cptr = _mktemp(temp);
  if(cptr == NULL) return -1;
  fout = fopen(cptr,"w");
  if(fout == NULL) return -1;
  fclose(fout);
  strcpy(p->file_prefix,cptr);  
  ret = 0;
  if(ret) return 1;
#endif

#ifdef __VMS
  //printf("file_prefix=%s\n",p->file_prefix);
  fout = fopen(p->file_prefix,"w");
  if(fout == NULL) return -1;
  fclose(fout);
#endif

  return 0;
}

static const char *pvFilename(const char *path)
{
  const char *cptr;

  if((cptr = mystrrchr(path,'/'))  != NULL) { cptr++; return cptr; }
  if((cptr = mystrrchr(path,'\\')) != NULL) { cptr++; return cptr; }
  if((cptr = mystrrchr(path,']'))  != NULL) { cptr++; return cptr; }
  return path;
}

static int pvUnlink(PARAM *p)
{
#ifdef PVUNIX
  static DIR *dirp;
  static struct dirent *dp;

  dirp = opendir(".");
  while((dp = readdir(dirp)) != NULL)
  {
    if(dp->d_name[0] == '.') ;
    else if(strncmp(dp->d_name,p->file_prefix,5+6) == 0)
    {
      //printf("unlink=%s\n",dp->d_name);
      unlink(dp->d_name);
    }
  }
  closedir(dirp);
#endif

#ifdef PVWIN32
  static WIN32_FIND_DATA wfd;
  static HANDLE hFindFile;

  hFindFile = FindFirstFile(p->file_prefix,&wfd);
  if(hFindFile == INVALID_HANDLE_VALUE) return 0;
  //printf("unlink=%s\n",(const char *) &wfd.cFileName);
  unlink((const char *) &wfd.cFileName);
  while(1)
  {
    if(FindNextFile(hFindFile,&wfd) == TRUE) 
    {
      //printf("unlink=%s\n",(const char *) &wfd.cFileName);
      unlink((const char *) &wfd.cFileName);
    }
    else                                   
    { 
      FindClose(hFindFile); 
      break;
    }
  }
#endif
  
#ifdef __VMS
  int  ret,context;
  char freturn[1024];
  char wildcard[80];
  struct dsc$descriptor_s dwildcard;
  struct dsc$descriptor_s dfreturn;
  char *cptr;

  context = 0;
  while(1)
  {
    strcpy(wildcard,p->file_prefix);

    dwildcard.dsc$w_length  = strlen(wildcard);
    dwildcard.dsc$a_pointer = wildcard;
    dwildcard.dsc$b_dtype   = DSC$K_DTYPE_T;
    dwildcard.dsc$b_class   = DSC$K_CLASS_S;

    dfreturn.dsc$w_length  = sizeof(freturn) - 1;
    dfreturn.dsc$a_pointer = &freturn[0];
    dfreturn.dsc$b_dtype   = DSC$K_DTYPE_T;
    dfreturn.dsc$b_class   = DSC$K_CLASS_S;

    ret = LIB$FIND_FILE(&dwildcard,&dfreturn,&context,0,0,0,0);
    freturn[dfreturn.dsc$w_length] = '\0';
    cptr = mystrstr(freturn," ");
    if(cptr != NULL) *cptr = '\0';
    if     (ret == RMS$_NMF)    break; // no more files found
    else if(ret != RMS$_NORMAL) break;
    else if(mystrstr(freturn,p->file_prefix) != NULL)
    {
      //printf("unlink=%s\n",freturn);
      unlink(freturn);
    }
  }
#endif
  
  return 0;
}

static void pv_length_check(PARAM *p, const char *buf)
{
  if(strlen(buf) >= MAX_PRINTF_LENGTH-1)
  {
    char message[80];
    strncpy(message,buf,10);
    message[10] = '\0';
    strcat(message,"... is too long -> exit");
    pvThreadFatal(p,message);
  }
}

static int sendBmpToSocket(PARAM *p, PVB_IMAGE *image)
{
char       buf[800];
char      *cptr;
LUT       *rgb;
int       y,i,w,h,depth;

  w = -1;
  h = -1;
  if(image == NULL)
  {
    sprintf(buf,"sendBmpToSocket image==(NULL)");
    pvWarning(p, buf);
    return -1;
  }
  w = image->w;
  h = image->h;
  depth = image->bpp;
  if(depth == 8)
  {
    cptr = (char *) image->image;
    for(y=0; y<h; y++) // send image
    {
      pvtcpsend_binary(p, cptr, w);
      cptr += image->w;
    }
    rgb = image->lut;
    for(i=0; i<image->nLut; i++) // send color table
    {
      sprintf(buf,"qRgb(%d,%d,%d,%d)\n", i, rgb[i].r, rgb[i].g, rgb[i].b);
      pvtcpsend(p,buf,strlen(buf));
    }
    sprintf(buf,"end\n");
    pvtcpsend(p,buf,strlen(buf));
  }
  else
  {
    sprintf(buf,"only 8bpp Images are supported for the moment depth=%d",depth);
    pvWarning(p, buf);
    return -1;
  }
  return 0;
}

int pvWarning(PARAM *p, const char *text)
{
  printf("Warning: %s s=%d\n",text,p->s);
  return 0;
}

int pvMainFatal(PARAM *p, const char *text)
{
  int i;

  printf("MainFatal1: %s s=%d\n",text,p->s);
  pvlock(p);
  num_threads = 0;
  if(p->clipboard != NULL) free(p->clipboard);
  if(p->x != NULL) free(p->x);
  if(p->y != NULL) free(p->y);
  p->clipboard        = NULL;
  p->clipboard_length = 0;
  if(p->cleanup != NULL) p->cleanup(p->app_data);
  //printf("MainFatal2: %s s=%d\n",text,p->s);
  for(i=0; i<MAX_CLIENTS; i++)
  {
    if(clientsocket[i] != -1) closesocket(clientsocket[i]);
  }
  if(serversocket != -1) closesocket(serversocket);
  if(p->free == 1 && p->exit_on_bind_error == 0) free(p);
  pvunlock(p);
  exit(0);
  return 0;
}

int pvThreadFatal(PARAM *p, const char *text)
{
  int i;

  pvlock(p);
  printf("Thread finished: %s s=%d\n",text,p->s);
  if(num_threads > 0) num_threads--;
  if(p->clipboard != NULL) free(p->clipboard);
  if(p->x != NULL) free(p->x);
  if(p->y != NULL) free(p->y);
  p->clipboard        = NULL;
  p->clipboard_length = 0;
  pvUnlink(p);
  if(p->cleanup != NULL) p->cleanup(p->app_data);
  //printf("Thread finished2: %s s=%d\n",text,p->s);
  for(i=0; i<MAX_CLIENTS; i++)
  {
    if(clientsocket[i] == p->s)
    {
      clientsocket[i] = -1;
      break;
    }
  }
  //printf("Thread finished3: %s s=%d\n",text,p->s);
  closesocket(p->s);
  //printf("Thread finished4: %s s=%d free=%d\n",text,p->s,p->free);
  if(p->free == 1) free(p);
  //printf("Thread finished5: %s\n",text);
  printf("pvthread_exit\n");
  p->my_pvlock_count = 0; // ensure that mutex gets unlocked
  pvunlock(p);
  //printf("Thread finished6: %s\n",text);
  if(exit_after_last_client_terminates == 1 && num_threads <= 0) exit(0);
  //printf("Thread finished7: %s\n",text);
#ifdef USE_INETD
  exit(0);
#else
  pvthread_exit(NULL);
  //printf("Thread finished8: %s\n",text);
#endif
  return 0;
}

int pvSetMouseShape(PARAM *p, int shape)
{
char buf[80];

  sprintf(buf,"setMouseShape(%d)\n",shape);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvScreenHint(PARAM *p, int w, int h)
{
char buf[80];

  sprintf(buf,"screenHint(%d,%d)\n",w,h);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

/* windows stuff */
static int wsa()
{
#ifdef PVWIN32
  WORD    wVersionRequested;
  WSADATA wsadata;
  int     err;

#ifdef IS_OLD_MSVCPP
  wVersionRequested = MAKEWORD(1,1);
#else
  wVersionRequested = MAKEWORD(2,0);
#endif
  err = WSAStartup(wVersionRequested, &wsadata);
  if(err != 0)
  {
    printf("Startup error=%d on windows\n",err);
    exit(0);
  }
#endif
  return 0;
}

static void sighandler(int sig)
{
  int i, s;

  if(sig == 0) sig = 0;
  for(i=0; i<MAX_CLIENTS; i++)
  {
    s = clientsocket[i];
    clientsocket[i] = -1;
    if(s != -1) closesocket(s);
  }
  if(serversocket != -1) closesocket(serversocket);
  exit(0);
}

int pvXYAllocate(PARAM *p, int n)
{
  p->nxy = 0;
  pvlock(p);
  if(p->x != NULL) free(p->x);
  if(p->y != NULL) free(p->y);
  p->x = (float *) malloc(sizeof(float)*n);
  p->y = (float *) malloc(sizeof(float)*n);
  pvunlock(p);
  if(p->x == NULL) return -1;
  if(p->y == NULL) return -1;
  p->nxy = n;
  return 0;
}

int pvSetXY(PARAM *p, int i, float x, float y)
{
  if(i <       0) return -1;
  if(i >= p->nxy) return -1;
  p->x[i] = x;
  p->y[i] = y;
  return 0;
}

int pvAccept(PARAM *p)
{
static int first = 1;
int ret,i;
struct sockaddr_in localAddr;
char buf[132];
int option = 1;
#ifdef AF_INET6_IS_AVAILABLE
  static struct addrinfo    hints, *res, *ressave;
  int n;
  char portstr[32];
#endif

  if(rl_ipversion == 4)
  {
    if(first == 1)
    {
      wsa(); /* windows stuff */
      /* create a socket                     */
      p->os = socket(AF_INET,SOCK_STREAM,0);
      if(p->os == -1) pvMainFatal(p,"could not create socket");
#ifdef PVWIN32
      setsockopt(p->os,SOL_SOCKET,SO_REUSEADDR,(const char *) &option,sizeof(option));
#else
      setsockopt(p->os,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(option));
#endif
      /* Bind our server to the agreed upon port number.      */
      memset(&localAddr,0,sizeof(localAddr));
      localAddr.sin_port = htons(p->port);
      localAddr.sin_family = AF_INET;
bind:
      ret = bind(p->os, (struct sockaddr *) &localAddr, sizeof(localAddr));
      if(ret == -1)
      {
        sprintf(buf,"could not bind to port %d",p->port);
        pvWarning(p,buf);
        if(p->exit_on_bind_error == 1) pvMainFatal(p,"exit");
        pvSleep(10*1000);
        goto bind;
      }
      /* Prepare to accept client connections.  Allow up to 5 pending  */
      /* connections.                                                  */
      ret = listen(p->os, 5);
      if(ret == -1) pvWarning(p,"listen failed");
      first = 0;
      serversocket = p->os;
      for(i=0; i<MAX_CLIENTS; i++) clientsocket[i] = -1;
      signal(SIGINT ,sighandler);
      signal(SIGTERM,sighandler);
      printf("Info: going to accept on port %d\n",p->port);
    }
    p->s = accept(p->os, NULL, NULL);
  }
#ifdef AF_INET6_IS_AVAILABLE
  else if(rl_ipversion == 6)
  {
    if(first == 1)
    {
      wsa(); /* windows stuff */
bindv6:
      memset(&hints,0,sizeof(hints));
      hints.ai_flags = AI_PASSIVE;
      //hints.ai_family = AF_UNSPEC;
      hints.ai_family = AF_INET6;
      hints.ai_socktype = SOCK_STREAM;
      sprintf(portstr,"%d",p->port);
      //::printf("server getaddrinfo(%s,%s)\n", adr, portstr);
      n = getaddrinfo("localhost", portstr, &hints, &res);
      if(n != 0)
      {
#ifndef PVWIN32
        ::printf("tcp_listen error for %s port=%s : %s\n", "localhost", portstr, gai_strerror(n));
#endif
        return -1;
      }
      //::printf("done\n");
      ressave = res;
      do
      {
        p->os = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if(p->os < 0)                                            continue; // error, try next one
#ifdef __VMS
        option = 1;
        if(setsockopt(p->os,SOL_SOCKET,SO_KEEPALIVE,&option,sizeof(option)) < 0)
        {
          printf("could not set socket option SO_KEEPALIVE\n");
          exit(0);
        }
#endif
        option = 1;
#ifdef PVWIN32
        setsockopt(p->os,SOL_SOCKET,SO_REUSEADDR,(const char *) &option,sizeof(option));
#else
        setsockopt(p->os,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(option));
#endif
        if(bind(p->os, res->ai_addr, res->ai_addrlen) == 0)      break;    // success
        p->s = p->os;
      }
      while((res = res->ai_next) != NULL);
      if(res == NULL)                                                   // errno from final socket() or bind()
      {
        if(p->exit_on_bind_error == 1) pvMainFatal(p,"exit");
        ::printf("warning: could not bind to port=%d\n", p->port);
        pvSleep(10*1000);
        goto bindv6;
      }
      // Prepare to accept client connections.  Allow up to 5 pending
      // connections
      ret = listen(p->os, 5);
      freeaddrinfo(ressave);
      if(ret == -1)
      {
#ifndef PVWIN32
        ::printf("tcp_listen error for %s port=%s : %s\n", "localhost", portstr, gai_strerror(n));
#endif
        exit(0);
      }
      first = 0;
      serversocket = p->os;
      for(i=0; i<MAX_CLIENTS; i++) clientsocket[i] = -1;
      signal(SIGINT ,sighandler);
      signal(SIGTERM,sighandler);
      printf("Info: going to accept on port %d\n",p->port);
    }
    p->s = accept(p->os, NULL, NULL);
  }
#endif
  else
  {
    printf("error: invalid ipversion=%d or ipv6 not available on this platform\n", rl_ipversion);
    exit(0);
  }

  pvlock(p);
  for(i=0; i<MAX_CLIENTS; i++)
  {
    if(clientsocket[i] == -1)
    {
      clientsocket[i] = p->s;
      break;
    }
  }
  pvunlock(p);
  if(i >= MAX_CLIENTS)
  {
    closesocket(p->s);
    sprintf(buf,"MAX_CLIENTS=%d exceeded",MAX_CLIENTS);
    pvWarning(p,buf);
    return -1;
  }
  return p->s;
}

#ifdef __VMS // If critical customer can not be convinced that IO's are normal
static char globalBuffer[4096+1024];
static int  globalBufferEnd = 0;

int setGlobalBuffer(const char *buf, int len)
{
  if(buf == NULL) return 1;
  memcpy(&globalBuffer[globalBufferEnd],buf,len);
  globalBufferEnd += len;
  if(globalBufferEnd < 1024) return 0;
  return 1;
}
#endif

/* send a packet to tcp or a file */
int pvtcpsend_binary(PARAM *p, const char *buf, int len)
{
  int ret,bytes_left,first_byte;
  // set p->os == -2 if tcp communication fails -> exit in next eventloop

#ifdef __VMS // If critical customer can not be convinced that IO's are normal
  if(setGlobalBuffer(buf,len) == 0) return 0;
  buf = &globalBuffer[0];
  len = globalBufferEnd;
  globalBufferEnd = 0;
  if(len <= 0) return 0;
#endif

  if(p->fp != NULL) /* if file is open we only write to file */
  {
    fputs(buf,p->fp);
    return 0;
  }
  if(p->s == -1 && p->os != -2) pvThreadFatal(p,"exit");;
  bytes_left = len;
  first_byte = 0;
  while(bytes_left > 0)
  {
#if defined(USE_INETD) && !defined(__VMS)
    ret = write(pv_STDOUT,&buf[first_byte],bytes_left);
    if(ret <= 0 && p->os != -2) pvMainFatal(p,"exit");
    if(ret <= 0)                p->os = -2;
#else
    ret = send(p->s,&buf[first_byte],bytes_left,MSG_NOSIGNAL);
    if(ret <= 0 && p->os != -2) pvThreadFatal(p,"exit");
    if(ret <= 0)                p->os = -2;
#endif
    bytes_left -= ret;
    first_byte += ret;
  }
  return 0;
}

/* send a packet to tcp or a file with range check */
int pvtcpsend(PARAM *p, const char *buf, int len)
{
  if(len >= MAX_PRINTF_LENGTH-1)
  {
    char message[80];
    strncpy(message,buf,10);
    message[10] = '\0';
    strcat(message,"... is too long -> exit");
    if(p->os != -2) pvThreadFatal(p,message);
  }
  pvtcpsend_binary(p,buf,len);
  return 0;
}

/* receive a packet from tcp */
int pvtcpreceive(PARAM *p, char *buf, int maxlen)
{
  int i,ret;

  i = 0;
  while(i < maxlen-1)
  {
#if defined(USE_INETD) && !defined(__VMS)
    ret = read(pv_STDIN,&buf[i],1);
    if(ret <= 0) pvMainFatal(p,"exit");
#else
    ret = recv(p->s,&buf[i],1,0);
    if(ret <= 0) pvThreadFatal(p,"exit");
#endif
    if(buf[i] == '\n') break;
    i += ret;
  }
  i++;
  buf[i] = '\0';
  return i;
}

/* receive a packet from tcp */
int pvtcpreceive_binary(PARAM *p, char *buf, int maxlen)
{
  int i,ret;

  i = 0;
  while(i < maxlen)
  {
#if defined(USE_INETD) && !defined(__VMS)
    ret = read(pv_STDIN,&buf[i],1);
    if(ret <= 0) pvMainFatal(p,"exit");
#else
    ret = recv(p->s,&buf[i],1,0);
    if(ret <= 0) pvThreadFatal(p,"exit");
#endif
    i += ret;
  }
  return i;
}

static int show_usage()
{
  printf("pvserver 4.4.8 (C) by Lehrig Software Engineering 2000-2009       lehrig@t-online.de\n\n");
  printf("usage: pvserver -port=5050 -sleep=500 -cd=/working/directory -exit_on_bind_error -exit_after_last_client_terminates -noforcenullevent -cache -ipv6\n");
  printf("default:\n");
  printf("-port=5050\n");
  printf("-sleep=500 milliseconds\n");
  printf("do NOT exit_after_last_client_terminates\n\n");
  printf("-cd=/working/directory\n");
  return 0;
}

int pvInitInternal(PARAM *p)
{
int i;

  setlocale(LC_NUMERIC, "C");
#ifdef USE_INETD
  p->s = 1;
#ifdef __VMS
#if defined(UCX$C_AUXS) || defined(TCPIP$C_AUXS)
#ifdef UCX$C_AUXS
  p->s = socket(UCX$C_AUXS,0,0);
#else
  p->s = socket(TCPIP$C_AUXS,0,0);
#endif
#else
#error "in util.cpp pvInit(): neither UCX$C_AUXS nor TCPIP$C_AUXS defined"
#endif
#endif
#else  // other OS than __VMS
  p->s = -1;
#endif // end __VMS
  p->os = -1;
  p->port = 5050;
  p->language = DEFAULT_LANGUAGE;
  p->convert_units = 0;
  p->allow_pause = 1;
  p->pause = 0;
  p->fp = NULL;
  p->sleep = 500;
  p->user = NULL;
  p->cleanup = NULL;
  p->app_data = NULL;
  p->clipboard = NULL;
  p->clipboard_length = 0;
  p->modal = 0;
  p->x = NULL;
  p->y = NULL;
  p->nxy = 0;
	p->url[0] = '\0';
  p->initial_mask[0] = '\0';
  p->free = 1;
  strcpy(p->version,"unknown");
  p->exit_on_bind_error = 0;
  p->hello_counter = 0;
  p->local_milliseconds = 0;
  p->force_null_event = 1;
  p->my_pvlock_count = 0;
  p->num_additional_widgets = 0;
  p->mouse_x  = p->mouse_y = -1;
  for(i=0; i<MAX_CLIENTS; i++) clientsocket[i] = -1;
#ifndef USE_INETD
  pvthread_mutex_init(&param_mutex, NULL);
#endif
#ifdef USE_INETD  
  pvFilePrefix(p);
#endif  

  return 0;
}

int pvInit(int ac, char **av, PARAM *p)
{
int i,ret;

  ret = 0;
  pvInitInternal(p);
  for(i=1; i<ac; i++)
  {
    if     (strncmp(av[i],"--h",3)                             == 0) { show_usage(); exit(0); }
    if     (strncmp(av[i],"-h",2)                              == 0) { show_usage(); exit(0); }
    else if(strncmp(av[i],"-port=",6)                          == 0) sscanf(av[i],"-port=%d",&p->port);
    else if(strncmp(av[i],"-sleep=",7)                         == 0) sscanf(av[i],"-sleep=%d",&p->sleep);
    else if(strncmp(av[i],"-cd=",4)                            == 0) ret = chdir(&av[i][4]);
    else if(strcmp(av[i],"-exit_on_bind_error")                == 0) p->exit_on_bind_error = 1;
    else if(strcmp(av[i],"-noforcenullevent")                  == 0) p->force_null_event = 0;
    else if(strcmp(av[i],"-exit_after_last_client_terminates") == 0) exit_after_last_client_terminates = 1;
    else if(strcmp(av[i],"-cache")                             == 0) pv_cache = 1;
    else if(strcmp(av[i],"-ipv6")                              == 0)
    {
      printf("useing IPV6 adresses\n");
      rl_ipversion = 6;
    }
    else if(strncmp(av[i],"-",1)                               == 0) printf("unknown option %s\n", av[i]);
  }
  return ret;
}

int pvGetInitialMask(PARAM *p)
{
  struct timeval timeout;
  fd_set wset,rset,eset;
  int    ret,maxfdp1;
  char   event[MAX_EVENT_LENGTH],text[MAX_EVENT_LENGTH];

  p->initial_mask[0] = '\0';

  maxfdp1 = p->s + 1;
  FD_ZERO(&rset);
  FD_SET(p->s, &rset);
  FD_ZERO(&wset);
  FD_ZERO(&eset);
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  /* call select */
  ret = select(maxfdp1,&rset,&wset,&eset,&timeout);
  if(ret == 0) return 0; /* timeout */
  pvtcpreceive(p, event, MAX_EVENT_LENGTH);

  if(strncmp(event,"initial(",8) == 0)
  {
    char *cptr;
    cptr = mystrstr(event,"version=");
    if(cptr != NULL)
    {
      sscanf(cptr,"version=%s",p->version);
    }
    printf("version of pvbrowser client = %s\n",p->version);
    pvGetText(event, text);
    text[MAX_PRINTF_LENGTH-1] = '\0';
    strcpy(p->initial_mask,text);
  }

  maxfdp1 = p->s + 1;
  FD_ZERO(&rset);
  FD_SET(p->s, &rset);
  FD_ZERO(&wset);
  FD_ZERO(&eset);
  timeout.tv_sec  = 1;
  timeout.tv_usec = 0;

  /* call select */
  ret = select(maxfdp1,&rset,&wset,&eset,&timeout);
  if(ret == 0) return 0; /* timeout */
  pvtcpreceive(p, event, MAX_EVENT_LENGTH);

  if(strncmp(event,"@url=",5) == 0)
  {
	  char *cptr;
    strcpy(p->url,&event[5]);
		cptr = mystrchr(p->url,'\n');
		if(cptr != NULL) *cptr = '\0';
  }
  return 0;
}

int pvSendUserEvent(PARAM *p, int id, const char *text)
{
  if(p == NULL) return -1;
#ifdef PVUNIX  
  snprintf(last_event,sizeof(last_event)-1,"user(%d,\"%s\")",id,text);
#endif  
#ifdef PVWIN32  
  _snprintf(last_event,sizeof(last_event)-1,"user(%d,\"%s\")",id,text);
#endif  
#ifdef __VMS
  sprintf(last_event,"user(%d,\"%s\")",id,text);
#endif  
  return 0;
}

int pvWriteFile(PARAM *p, const char *filename, int width, int height)
{
  if(p->fp != NULL) fclose(p->fp);
  p->fp = fopen(filename,"w");
  if(p->fp == NULL) 
  {
    printf("could not write %s\n",filename);
    return 0;  /* open failed */
  }
  fprintf(p->fp,"pvMetafile(%d,%d)\n",width,height);
  return 1; /* open success */
}

int pvCloseFile(PARAM *p)
{
  if(p->fp != NULL) fclose(p->fp);
  p->fp = NULL;
  return 0;
}

int pvSetCleanup(PARAM *p, int (*cleanup)(void *), void *app_data)
{
  p->cleanup  = cleanup;
  p->app_data = app_data;
  return 0;
}

char *pvGetEvent(PARAM *p)
{
  pvPollEvent(p,this_event);
  return this_event;
}

int pvPollEvent(PARAM *p, char *event)
{
  struct timeval timeout;
  fd_set wset,rset,eset;
  int    ret,maxfdp1,local_milliseconds;

  // if last tcp communication failed
  if(p->os == -2) pvMainFatal(p,"exit pvPollEvent");

  // perhaps force null_event
  local_milliseconds = getLocalMilliseconds();
  if(p->force_null_event == 1)
  {
    if((local_milliseconds - p->local_milliseconds) >= p->sleep)
    {
      //printf("force null_event\n");
      *event = '\0';
      p->local_milliseconds = local_milliseconds;
      return 0; // force null_event
    }
  }  

  if(last_event[0] != '\0')
  {
    strcpy(event,last_event);
    last_event[0] = '\0';
    return 0;
  }

#ifdef __VMS // Critical customer can not be convinced that IO's are normal
  pvtcpsend_binary(p, NULL, 0);
#endif

start_poll:  // only necessary for pause
  maxfdp1 = p->s + 1;
  FD_ZERO(&rset);
  FD_SET(p->s, &rset);
  FD_ZERO(&wset);
  FD_ZERO(&eset);
  timeout.tv_sec  =  p->sleep / 1000;
  timeout.tv_usec = (p->sleep % 1000) * 1000;

  /* call select */
  ret = select(maxfdp1,&rset,&wset,&eset,&timeout);
  if(ret == 0) /* timeout */
  {
    p->local_milliseconds = getLocalMilliseconds();
    *event = '\0';
    p->hello_counter += p->sleep;
    if(p->hello_counter > 1000*50) // client will say @hello every 10 seconds
    {                              // we wait 50 seconds max
      pvThreadFatal(p,"client does no longer send hello -> terminate thread");
    }
    if(p->allow_pause && p->pause)
    {
      //printf("pause s=%d\n",p->s);
      goto start_poll;
    }
    else
    {
      return 0;
    }  
  }
  p->hello_counter = 0;
  if(ret <  0) /* error condition */
  {
#ifdef PVWIN32
      Sleep(1000);
#endif
    *event = '\0';
    return 0;
  }

  pvtcpreceive(p, event, MAX_EVENT_LENGTH);
  p->mouse_x = p->mouse_y = -1;
  if(*event == '@')
  {
    if(strncmp(event,"@hello",6) == 0)
    {
      if(strncmp(event,"@hello=",7) == 0)
      {
        sscanf(event,"@hello=%s",p->version);
      }
      *event = '\0';
    }
    else if(strncmp(event,"@clipboard(",11) == 0)
    {
      int id,size;
      sscanf(event,"@clipboard(%d,%d",&id,&size);
      pvlock(p);
      if(p->clipboard != NULL) free(p->clipboard);
      p->clipboard = (char *) malloc(size+1);
      pvunlock(p);
      if(p->clipboard == NULL) pvThreadFatal(p,"not enough memory for clipboard");
      p->clipboard_length = size;
      pvtcpreceive_binary(p, p->clipboard, size);
      p->clipboard[size] = '\0';
    }
    else if(strncmp(event,"@pause(",7) == 0)
    {
      sscanf(event,"@pause(%d", &p->pause);
      *event = '\0';
    }
  }
  else if(event[0] == 'Q' && event[1] == 'P' && event[2] == 'u')
  {
    char *cptr = mystrstr(event,"-xy=");
    if(cptr != NULL)
    {
      sscanf(cptr,"-xy=%d,%d", &p->mouse_x, &p->mouse_y);
      //printf("mouse_xy = %d,%d\n", p->mouse_x, p->mouse_y);
    }
  }
  if(p->allow_pause && p->pause) 
  {
    //printf("pause because user hided tab\n");
    goto start_poll; // we pause
  }  
  return 0;
}

int pvWait(PARAM *p,const char *pattern)
{
  int  len;
  char line[MAX_EVENT_LENGTH];

  len = strlen(pattern);
  while(1)
  {
    pvtcpreceive(p, line, MAX_EVENT_LENGTH);
    if(strncmp(line,pattern,len) == 0) return 0;
    strcpy(last_event,line);
  }
}

int pvGlUpdate(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"pvGlUpdate(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

#ifndef USE_INETD
static void *send_thread(void *ptr)
{
  PARAM *p;
  p = (PARAM *) ptr;
  pvFilePrefix(p);
  pvMain(p);
  return NULL;
}
#endif

int pvCreateThread(PARAM *p, int s)
{
#ifndef USE_INETD
  pthread_t tid; /* this is the thread_id */
#endif
  PARAM *ptr;

  num_threads++;
  pvlock(p);
  ptr = (PARAM *) malloc(sizeof(PARAM));
  pvunlock(p);
  if(ptr == NULL) pvMainFatal(p,"out of memory");
  memcpy(ptr,p,sizeof(PARAM));
  ptr->s = s;
#ifndef USE_INETD
  printf("pvCreateThread s=%d\n",ptr->s);
  pvthread_create(&tid, NULL, send_thread, (void *) ptr);
#ifndef PVWIN32
  pthread_detach(tid);
#endif
  pvthread_close_handle(&tid);
#endif
  return 0;
}

int pvSleep(int msec)
{
#ifdef PVWIN32
  Sleep(msec);
  return 0;
#endif

#ifdef PVUNIX
  fd_set wset,rset,eset;
  struct timeval timeout;

  FD_ZERO(&rset);
  FD_ZERO(&wset);
  FD_ZERO(&eset);
  timeout.tv_sec  = msec / 1000;
  timeout.tv_usec = (msec % 1000) * 1000;
  select(1,&rset,&wset,&eset,&timeout);
  return 0;
#endif

#ifdef __VMS
  struct timespec interval;

  interval.tv_sec  =  msec / 1000;
  interval.tv_nsec = (msec % 1000) * 1000 * 1000; /* wait msec msec */
  pthread_delay_np(&interval);
  return 0;
#endif
}

int pvStartDefinition(PARAM *p, int num_objects)
{
char buf[80];

  if(p->num_additional_widgets > 0) num_objects += p->num_additional_widgets;
  sprintf(buf,"startDefinition(%d)\n",num_objects);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQLayoutVbox(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QLayoutVbox(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQLayoutHbox(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QLayoutHbox(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQLayoutGrid(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QLayoutGrid(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQWidget(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QWidget(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQLabel(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QLabel(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQLineEdit(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QLineEdit(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQComboBox(PARAM *p, int id, int parent, int editable, int policy)
{
char buf[80];

  sprintf(buf,"QComboBox(%d,%d,%d,%d)\n",id,parent,editable,policy);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQLCDNumber(PARAM *p, int id, int parent, int numDigits, int segmentStyle, int mode)
{
char buf[80];

  sprintf(buf,"QLCDNumber(%d,%d,%d,%d,%d)\n",id,parent,numDigits,segmentStyle,mode);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQPushButton(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QPushButton(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQDraw(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QDrawWidget(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQSlider(PARAM *p, int id, int parent, int minValue, int maxValue, int pageStep, int value, int orientation)
{
char buf[132];

  sprintf(buf,"QSlider(%d,%d,%d,%d,%d,%d,%d)\n",id,parent,minValue,maxValue,pageStep,value,orientation);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQButtonGroup(PARAM *p, int id, int parent, int columns, int o, const char *title)
{
char buf[132];

  sprintf(buf,"QButtonGroup(%d,%d,%d,%d,\"%s\")\n",id,parent,columns,o,title);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQRadioButton(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QRadio(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQCheckBox(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QCheckBox(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQFrame(PARAM *p, int id, int parent, int shape, int shadow, int line_width, int margin)
{
char buf[132];

  sprintf(buf,"QFrame(%d,%d,%d,%d,%d,%d)\n",id,parent,shape,shadow,line_width,margin);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQTabWidget(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QTabWidget(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQToolBox(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QToolBox(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQGroupBox(PARAM *p, int id, int parent, int columns, int orientation, const char *title)
{
char buf[80];

  sprintf(buf,"QGroupBox(%d,%d,%d,%d,\"%s\")\n",id,parent,columns,orientation,title);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQListBox(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QListBox(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQTable(PARAM *p, int id, int parent, int rows, int columns)
{
char buf[80];

  sprintf(buf,"QTable(%d,%d,%d,%d)\n",id,parent,rows,columns);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQSpinBox(PARAM *p, int id, int parent, int min, int max, int step)
{
char buf[80];

  sprintf(buf,"QSpinBox(%d,%d,%d,%d,%d)\n",id,parent,min,max,step);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQDial(PARAM *p, int id, int parent, int min, int max, int page_step, int value)
{
char buf[80];

  sprintf(buf,"QDial(%d,%d,%d,%d,%d,%d)\n",id,parent,min,max,page_step,value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQProgressBar(PARAM *p, int id, int parent, int total_steps)
{
char buf[80];

  sprintf(buf,"QProgressBar(%d,%d,%d)\n",id,parent,total_steps);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQMultiLineEdit(PARAM *p, int id, int parent, int editable, int max_lines)
{
char buf[80];

  sprintf(buf,"QMultiLineEdit(%d,%d,%d,%d)\n",id,parent,editable,max_lines);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQTextBrowser(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QTextBrowser(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQListView(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QListView(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQIconView(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QIconView(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQVtkTclWidget(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QVtkTclWidget(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQwtPlotWidget(PARAM *p, int id, int parent, int nCurves, int nMarker)
{
char buf[80];

  sprintf(buf,"QwtPlotWidget(%d,%d,%d,%d)\n",id,parent,nCurves,nMarker);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQwtScale(PARAM *p, int id, int parent, int pos)
{
char buf[80];

  sprintf(buf,"QwtScale(%d,%d,%d)\n",id,parent,pos);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQwtThermo(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QwtThermo(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQwtKnob(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QwtKnob(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQwtCounter(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QwtCounter(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQwtWheel(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QwtWheel(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQwtSlider(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QwtSlider(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQwtCompass(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QwtCompass(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQDateEdit(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QDateEdit(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQTimeEdit(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QTimeEdit(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvQDateTimeEdit(PARAM *p, int id, int parent)
{
char buf[80];

  sprintf(buf,"QDateTimeEdit(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvEndDefinition(PARAM *p)
{
char buf[80];

  sprintf(buf,"endDefinition()\n");
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvAddWidgetOrLayout(PARAM *p, int id, int item, int row, int col)
{
char buf[80];

  sprintf(buf,"addWidgetOrLayout(%d,%d,%d,%d)\n",id,item,row,col);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvAddStretch(PARAM *p, int id, int param)
{
char buf[80];

  sprintf(buf,"addStretch(%d,%d)\n",id,param);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvTabOrder(PARAM *p, int id1, int id2)
{
char buf[80];

  sprintf(buf,"tabOrder(%d,%d)\n",id1,id2);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvToolTip(PARAM *p, int id, const char *text)
{
char buf[1024],*cptr;

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"QToolTip(%d,\"%s\")\n",id,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvText(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"text(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSelection(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"selection(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvRequestSvgBoundsOnElement(PARAM *p, int id, const char *objectname)
{
char buf[MAX_PRINTF_LENGTH+80];

  pv_length_check(p,objectname);
  sprintf(buf,"requestSvgBoundsOnElement(%d,\"%s\")\n",id,objectname);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvBeep(PARAM *p)
{
char buf[80];

  sprintf(buf,"beep()\n");
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvPlaySound(PARAM *p, const char *filename)
{
char buf[MAX_PRINTF_LENGTH];

  pv_length_check(p,filename);
  sprintf(buf,"playSound(\"%s\")\n",filename);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvClientCommand(PARAM *p, const char *command, const char *filename)
{
char buf[MAX_PRINTF_LENGTH];

  pv_length_check(p,command);
  pv_length_check(p,filename);
  strcpy(buf,"view.");
  strcat(buf, command);
  buf[16] = '\0';
  strcat(buf," ");
  strcat(buf,filename);
  strcat(buf,"\n");
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvStatusMessage(PARAM *p, int r, int g, int b, const char *format, ...)
{
char text[MAX_PRINTF_LENGTH+40],*cptr;
char buf[MAX_PRINTF_LENGTH+40];

  va_list ap;
  va_start(ap,format);
#ifdef PVWIN32
  _vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
#ifdef __VMS
  vsprintf(text, format, ap);
#endif
#ifdef PVUNIX
  vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
  va_end(ap);

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"statusMessage(%d,%d,%d,\"%s\")\n",r,g,b,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetText(PARAM *p, int id, const char *text)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;
int len;

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  len = strlen(text);
  if(len < MAX_PRINTF_LENGTH-1)
  {
    pv_length_check(p,text);
    //sprintf(buf,"setText(%d,\"%s\")\n",id,text);
    //pvtcpsend(p, buf, strlen(buf));
    sprintf(buf,"setText(%d)\n",id);
    pvtcpsend(p, buf, strlen(buf));
    sprintf(buf,"%s\n",text);
    pvtcpsend(p, buf, strlen(buf));
  }
  else
  {
    sprintf(buf,"setText(%d)\n",id);
    pvtcpsend(p, buf, strlen(buf));
    sprintf(buf,"alloc(%d)\n",len);
    pvtcpsend(p, buf, strlen(buf));
    pvtcpsend_binary(p, text, len);
  }
  return 0;
}

int pvSetWhatsThis(PARAM *p, int id, const char *_text)
{
char buf[MAX_PRINTF_LENGTH+40],text[MAX_PRINTF_LENGTH+40],*cptr;
int len;

  pv_length_check(p,_text);
  strcpy(text,_text);
  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  len = strlen(text);
  if(len < MAX_PRINTF_LENGTH-1)
  {
    sprintf(buf,"setWhatsThis(%d)\n",id);
    pvtcpsend(p, buf, strlen(buf));
    sprintf(buf,"%s\n",text);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvWhatsThisPrintf(PARAM *p, int id, const char *format, ...)
{
  char text[MAX_PRINTF_LENGTH+40];

  va_list ap;
  va_start(ap,format);
#ifdef PVWIN32
  _vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
#ifdef __VMS
  vsprintf(text, format, ap);
#endif
#ifdef PVUNIX
  vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
  va_end(ap);

  pvSetWhatsThis(p,id,text);
  return 0;
}

int pvSetCaption(PARAM *p, const char *text)
{
  pvSetText(p,0,text);
  return 0;
}

int pvSetValue(PARAM *p, int id, int value)
{
char buf[80];

  sprintf(buf,"setValue(%d,%d)\n",id,value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvClear(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"clear(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvChangeItem(PARAM *p, int id, int index, const char *bmp_file, const char *text, int download_icon)
{
char buf[MAX_PRINTF_LENGTH+40];

  pv_length_check(p,text);

  if(bmp_file == NULL)
  {
    sprintf(buf,"changeItem(%d,%d,0,\"%s\")\n",id,index,text);
    pvtcpsend(p, buf, strlen(buf));
  }
  else if(mystrstr(bmp_file,".png") != NULL || mystrstr(bmp_file,".PNG") != NULL)
  {
    if(download_icon==1) pvDownloadFile(p,bmp_file);
    sprintf(buf,"changeItem(%d,%d,1,\"%s\")\n",id,index,text);
    pvtcpsend(p, buf, strlen(buf));
    sprintf(buf,"(-2,-2)\n%s\n",pvFilename(bmp_file));
    pvtcpsend(p, buf, strlen(buf));
  }
  else
  {
    PVB_IMAGE *image;
    image = pvbImageRead(bmp_file);
    if(image != NULL && image->bpp == 8)
    {
      sprintf(buf,"changeItem(%d,%d,1,\"%s\")\n",id,index,text);
      pvtcpsend(p, buf, strlen(buf));
      sprintf(buf,"(%d,%d)\n",image->w,image->h);
      pvtcpsend(p, buf, strlen(buf));
      sendBmpToSocket(p, image);
    }
    pvbImageFree(image);
  }
  return 0;
}

int pvInsertItem(PARAM *p, int id, int index, const char *bmp_file, const char *text, int download_icon)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  if(bmp_file == NULL)
  {
    sprintf(buf,"insertItem(%d,%d,0,\"%s\")\n",id,index,text);
    pvtcpsend(p, buf, strlen(buf));
  }
  else if(mystrstr(bmp_file,".png") != NULL || mystrstr(bmp_file,".PNG") != NULL)
  {
    if(download_icon==1) pvDownloadFile(p,bmp_file);
    sprintf(buf,"insertItem(%d,%d,1,\"%s\")\n",id,index,text);
    pvtcpsend(p, buf, strlen(buf));
    sprintf(buf,"(-2,-2)\n%s\n",pvFilename(bmp_file));
    pvtcpsend(p, buf, strlen(buf));
  }
  else
  {
    PVB_IMAGE *image;
    image = pvbImageRead(bmp_file);
    if(image != NULL && image->bpp == 8)
    {
      sprintf(buf,"insertItem(%d,%d,1,\"%s\")\n",id,index,text);
      pvtcpsend(p, buf, strlen(buf));
      sprintf(buf,"(%d,%d)\n",image->w,image->h);
      pvtcpsend(p, buf, strlen(buf));
      sendBmpToSocket(p, image);
    }
    pvbImageFree(image);
  }
  return 0;
}

int pvRemoveItem(PARAM *p, int id, int index)
{
char buf[80];

  sprintf(buf,"removeItem(%d,%d)\n",id,index);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvRemoveItemByName(PARAM *p, int id, const char *name)
{
char buf[MAX_PRINTF_LENGTH+40];

  pv_length_check(p,name);
  sprintf(buf,"removeItemByName(%d,\"%s\")\n",id,name);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetListViewPixmap(PARAM *p, int id, const char *path, const char *bmp_file, int column, int download_icon)
{
char buf[MAX_PRINTF_LENGTH+40];

  pv_length_check(p,path);

  if(bmp_file == NULL)
  {
    sprintf(buf,"setListViewPixmap(%d,0,0,%d,\"%s\")\n",id,column,path);
    pvtcpsend(p, buf, strlen(buf));
  }
  else if(mystrstr(bmp_file,".png") != NULL || mystrstr(bmp_file,".PNG") != NULL)
  {
    if(download_icon==1) pvDownloadFile(p,bmp_file);
    sprintf(buf,"setListViewPixmap(%d,%d,%d,%d,\"%s\")\n%s\n",id,-2,-2,column,path,pvFilename(bmp_file));
    pvtcpsend(p, buf, strlen(buf));
  }
  else
  {
    PVB_IMAGE *image;
    image = pvbImageRead(bmp_file);
    if(image != NULL && image->bpp == 8)
    {
      sprintf(buf,"setListViewPixmap(%d,%d,%d,%d,\"%s\")\n",id,image->w,image->h,column,path);
      pvtcpsend(p, buf, strlen(buf));
      sendBmpToSocket(p, image);
    }
    pvbImageFree(image);
  }
  return 0;
}

int pvRemoveListViewItem(PARAM *p, int id, const char *path)
{
char buf[MAX_PRINTF_LENGTH+40];

  pv_length_check(p,path);
  sprintf(buf,"removeListViewItem(%d,\"%s\")\n",id,path);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvRemoveIconViewItem(PARAM *p, int id, const char *text)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);
  while(1)
  {
    cptr = mystrchr(buf,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"removeIconViewItem(%d,\"%s\")\n",id,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetIconViewItem(PARAM *p, int id, const char *bmp_file, const char *text, int download_icon)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  if(bmp_file == NULL)
  {
    sprintf(buf,"setIconViewItem(%d,0,0,\"%s\")\n",id,text);
    pvtcpsend(p, buf, strlen(buf));
  }
  else if(mystrstr(bmp_file,".png") != NULL || mystrstr(bmp_file,".PNG") != NULL)
  {
    if(download_icon==1) pvDownloadFile(p,bmp_file);
    sprintf(buf,"setIconViewItem(%d,%d,%d,\"%s\")\n%s\n",id,-2,-2,text,pvFilename(bmp_file));
    pvtcpsend(p, buf, strlen(buf));
  }
  else
  {
    PVB_IMAGE *image;
    image = pvbImageRead(bmp_file);
    if(image != NULL && image->bpp == 8)
    {
      sprintf(buf,"setIconViewItem(%d,%d,%d,\"%s\")\n",id,image->w,image->h,text);
      pvtcpsend(p, buf, strlen(buf));
      sendBmpToSocket(p, image);
    }
    pvbImageFree(image);
  }
  return 0;
}

int pvSetDateOrder(PARAM *p, int id, int order)
{
char buf[80];

  sprintf(buf,"setDateOrder(%d,%d)\n",id,order);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetDate(PARAM *p, int id, int year, int month, int day)
{
char buf[80];

  sprintf(buf,"setDate(%d,%d,%d,%d)\n",id,year,month,day);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetMinDate(PARAM *p, int id, int year, int month, int day)
{
char buf[80];

  sprintf(buf,"setMinDate(%d,%d,%d,%d)\n",id,year,month,day);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetMaxDate(PARAM *p, int id, int year, int month, int day)
{
char buf[80];

  sprintf(buf,"setMaxDate(%d,%d,%d,%d)\n",id,year,month,day);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetTime(PARAM *p, int id, int hour, int minute, int second, int msec)
{
char buf[80];

  sprintf(buf,"setTime(%d,%d,%d,%d,%d)\n",id,hour,minute,second,msec);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetMinTime(PARAM *p, int id, int hour, int minute, int second, int msec)
{
char buf[80];

  sprintf(buf,"setMinTime(%d,%d,%d,%d,%d)\n",id,hour,minute,second,msec);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetMaxTime(PARAM *p, int id, int hour, int minute, int second, int msec)
{
char buf[80];

  sprintf(buf,"setMaxTime(%d,%d,%d,%d,%d)\n",id,hour,minute,second,msec);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvEnsureCellVisible(PARAM *p, int id, int row, int col)
{
char buf[80];

  sprintf(buf,"ensureCellVisible(%d,%d,%d)\n",id,row,col);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvMoveCursor(PARAM *p, int id, int cursor)
{
char buf[80];

  sprintf(buf,"moveCursor(%d,%d)\n",id,cursor);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvAddColumn(PARAM *p, int id, const char *text, int size)
{
char buf[80],*cptr;

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"addColumn(%d,%d,\"%s\")\n",id,size,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetTableText(PARAM *p, int id, int x, int y, const char *text)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);
  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"setTableText(%d,%d,%d,\"%s\")\n",id,y,x,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvTablePrintf(PARAM *p, int id, int x, int y, const char *format, ...)
{
  char text[MAX_PRINTF_LENGTH+40];

  va_list ap;
  va_start(ap,format);
#ifdef PVWIN32
  _vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
#ifdef __VMS
  vsprintf(text, format, ap);
#endif
#ifdef PVUNIX
  vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
  va_end(ap);

  pvSetTableText(p,id,x,y,text);
  return 0;
}

int pvSetTableTextAlignment(PARAM *p, int id, int x, int y, int alignment)
{
  char buf[128];
  sprintf(buf,"setTableTextAlignment(%d,%d,%d,%d)\n",id,y,x,alignment);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetTableButton(PARAM *p, int id, int x, int y, const char *text)
{
char buf[MAX_PRINTF_LENGTH+40];

  sprintf(buf,"setTableButton(%d,%d,%d,\"%s\")\n",id,y,x,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetTableCheckBox(PARAM *p, int id, int x, int y, int state, const char *text)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"setTableCheckBox(%d,%d,%d,%d,\"%s\")\n",id,y,x,state,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetTableComboBox(PARAM *p, int id, int x, int y, int editable, const char *textlist)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  while(1)
  {
    cptr = mystrchr(textlist,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"setTableComboBox(%d,%d,%d,%d,\"%s\")\n",id,y,x,editable,textlist);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetTableLabel(PARAM *p, int id, int x, int y, const char *text)
{
char buf[MAX_PRINTF_LENGTH+40];

  sprintf(buf,"setTableLabel(%d,%d,%d,\"%s\")\n",id,y,x,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvMysqldump(PARAM *p, int id, const char *command)
{
  FILE *fin;
  char line[MAX_EVENT_LENGTH],filename[32+14+1],*cptr1,*cptr2;
  int ret,row_cnt,field_cnt,first,x,y;

  if(strlen(command) > (sizeof(line) - 80)) 
  {
    printf("pvMysqldump:: command too long\n");
    return -1;
  }

  // run mysqldump
  sprintf(filename,"%smysqldump.xml",p->file_prefix);
  sprintf(line,"mysqldump -X %s > %s",command,filename);
  ret = pvsystem(line);
  if(ret == -1) 
  {
    printf("pvMysqldump:: failed: %s\n",line);
    return -1;
  }

  // calculate dimension of table
  fin = fopen(filename,"r");
  if(fin == NULL)
  {
    printf("pvMysqldump:: could not open: %s\n",filename);
    return -1;
  }
  row_cnt = field_cnt = 0;
  while(fgets(line,sizeof(line)-1,fin) != NULL)
  {
    if(mystrstr(line,"<row>")        != NULL) row_cnt++;
    if(mystrstr(line,"<field name=") != NULL) field_cnt++;
  }
  if(row_cnt == 0) return -1;
  field_cnt = field_cnt / row_cnt;

  // set table dimension
  pvSetNumRows(p,id,row_cnt);
  pvSetNumCols(p,id,field_cnt);

  fseek(fin, 0L, SEEK_SET);

  // fill rows and header
  first = 1;
  x = y = 0;
  while(fgets(line,sizeof(line)-1,fin) != NULL)
  {
    if(mystrstr(line,"</row>") != NULL)
    {
      first = 0;
      x = 0;
      y++;
    }
    if(mystrstr(line,"<field name=") != NULL)
    {
      // set row
      cptr1 = mystrstr(line,"\">");
      cptr1++; cptr1++;
      cptr2 = mystrstr(cptr1,"</field>");
      if(cptr2 != NULL) *cptr2 = '\0';
      pvSetTableText(p, id, x, y, cptr1);
      if(first == 1) // set header
      {
        cptr1 = mystrstr(line,"=");
        cptr1++; cptr1++;
        cptr2 = mystrstr(cptr1,"\">");
        if(cptr2 != NULL) *cptr2 = '\0';
        pvSetTableText(p, id, x, -1, cptr1);
      }
      x++;
    }
  }

  fclose(fin);
  return 0;
}

static int pvReadLine(PARAM *p, int id, int y, const char *line, char delimitor)
{
  int x,i,tab1,tab2;
  char *celltext;

  tab1 = tab2 = -1;
  i = x = 0;
  while(line[i] != '\0')
  {
    if(line[i] == delimitor || line[i] == '\n') // normally delimitor='\t'
    {
      tab1 = tab2;
      tab2 = i;
      celltext = new char[tab2-tab1+1];
      if(tab2>tab1) 
      {
        strncpy((char *) celltext,(const char *) &line[tab1+1],tab2-tab1);
        celltext[tab2-tab1-1] = '\0';
      }
      else
      {
        celltext[0] = '\0';
      }
      pvSetTableText(p, id, x++, y, celltext);
      delete [] celltext;
    }
    i++;
  }
  return 0;
}

int pvCSVdump(PARAM *p, int id, const char *filename, char delimitor)
{
  FILE *fp;
  char *line;
  int y = -1;
  int i,count;
  if(filename == NULL) return -1;

  // read table
  line = new char[256*256+1];
  sprintf(line,"%s%s",p->file_prefix,filename);
  fp = fopen(line,"r");
  if(fp == NULL) 
  {
    printf("could not open %s\n",line);
    delete [] line;
    return -1;
  }
  while(fgets((char *) line,256*256,fp) != NULL)
  {
    if(y == -1)
    {
      i = 0;
      count = 1;
      while(line[i] != '\0') 
      {
        if(line[i] == delimitor) count++;
        i++;
      }
      pvSetNumCols(p,id,count);
    }
    pvSetNumRows(p,id,y+1);
    pvReadLine(p, id, y++, line, delimitor);
  }
  delete [] line;
  fclose(fp);
  return 0;
}

int pvCSVcreate(PARAM *p, const char *command, const char *filename)
{
  char line[MAX_EVENT_LENGTH];

  sprintf(line,"%s > %s%s", command, p->file_prefix, filename);
  return pvsystem(line);
}

int pvCSV(PARAM *p, int id, const char *command, char delimitor)
{
  pvCSVcreate(p, command, "out.csv");
  pvCSVdump(p, id, "out.csv", delimitor);
  return 0;
}

int pvSetListViewText(PARAM *p, int id, const char *path, int column, const char *text)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);
  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"setListViewText(%d,%d,\"%s\")\n",id,column,path);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"(\"%s\")\n",text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvListViewPrintf(PARAM *p, int id, const char *path, int column, const char *format, ...)
{
  char text[MAX_PRINTF_LENGTH+40];

  va_list ap;
  va_start(ap,format);
#ifdef PVWIN32
  _vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
#ifdef __VMS
  vsprintf(text, format, ap);
#endif
#ifdef PVUNIX
  vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
  va_end(ap);

  pvSetListViewText(p,id,path,column,text);
  return 0;
}

int pvListViewSetSelected(PARAM *p, int id, const char *path, int column, int selected)
{
char buf[MAX_PRINTF_LENGTH+40];

  pv_length_check(p,path);
  sprintf(buf, "setSelected(%d,%d,%d,\"%s\")\n", id, column, selected, path);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetColumnWidth(PARAM *p, int id, int column, int width)
{
char buf[80];

  sprintf(buf,"setColumnWidth(%d,%d,%d)\n",id,column,width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetRowHeight(PARAM *p, int id, int row, int height)
{
char buf[80];

  sprintf(buf,"setRowHeight(%d,%d,%d)\n",id,row,height);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetWordWrap(PARAM *p, int id, int wrap)
{
char buf[80];

  sprintf(buf,"tableWrap(%d,%d)\n",id,wrap);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetMultiSelection(PARAM *p, int id, int mode)
{
char buf[80];

  if(mode == 0 || mode == 1 || mode == 2)
  {
    sprintf(buf,"setMultiSelection(%d,%d)\n",id,mode);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvSetEchoMode(PARAM *p, int id, int mode)
{
char buf[80];

  if(mode == 0 || mode == 1 || mode == 2)
  {
    sprintf(buf,"setEchoMode(%d,%d)\n",id,mode);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvSetEditable(PARAM *p, int id, int editable)
{
char buf[80];

  if(editable == 0 || editable == 1)
  {
    sprintf(buf,"setEditable(%d,%d)\n",id,editable);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvSetEnabled(PARAM *p, int id, int enabled)
{
char buf[80];

  if(enabled == 0 || enabled == 1)
  {
    sprintf(buf,"setEnabled(%d,%d)\n",id,enabled);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvTableSetEnabled(PARAM *p, int id, int x, int y, int enabled)
{
char buf[80];

  if(enabled == 0 || enabled == 1)
  {
    sprintf(buf,"tableSetEnabled(%d,%d,%d,%d)\n",id,x,y,enabled);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvTableSetHeaderResizeEnabled(PARAM *p, int id, int horizontal ,int enabled, int section)
{
char buf[80];

  if(enabled == 0 || enabled == 1)
  {
    sprintf(buf,"tableSetHeaderResizeEnabled(%d,%d,%d,%d)\n",id,horizontal,enabled,section);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvSetSorting(PARAM *p, int id, int column, int mode)
{
char buf[80];

  if(column >= -1 && (mode == 0 || mode == 1))
  {
    sprintf(buf,"setSorting(%d,%d,%d)\n",id,column,mode);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvSetTabPosition(PARAM *p, int id, int pos)
{
char buf[80];

  if(pos == 0 || pos == 1)
  {
    sprintf(buf,"setTabPosition(%d,%d)\n",id,pos);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvSetNumRows(PARAM *p, int id, int num)
{
char buf[80];

  if(num >= 0)
  {
    sprintf(buf,"setNumRows(%d,%d)\n",id,num);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvSetNumCols(PARAM *p, int id, int num)
{
char buf[80];

  if(num >= 0)
  {
    sprintf(buf,"setNumCols(%d,%d)\n",id,num);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvInsertRows(PARAM *p, int id, int row, int count)
{
char buf[80];

  if(row >= 0)
  {
    sprintf(buf,"insertRows(%d,%d,%d)\n",id,row,count);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvInsertColumns(PARAM *p, int id, int col, int count)
{
char buf[80];

  if(col >= 0)
  {
    sprintf(buf,"insertCols(%d,%d,%d)\n",id,col,count);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvRemoveRow(PARAM *p, int id, int row)
{
char buf[80];

  if(row >= 0)
  {
    sprintf(buf,"removeRow(%d,%d)\n",id,row);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvRemoveColumn(PARAM *p, int id, int col)
{
char buf[80];

  if(col >= 0)
  {
    sprintf(buf,"removeCol(%d,%d)\n",id,col);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvSetCurrentItem(PARAM *p, int id, int index)
{
char buf[80];

  if(index >= 0)
  {
    sprintf(buf,"setCurrentItem(%d,%d)\n",id,index);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int pvSetTimeEditDisplay(PARAM *p, int id, int hour, int minute, int second, int ampm)
{
char buf[80];

  sprintf(buf,"setTimeEditDisplay(%d,%d,%d,%d,%d)\n",id,hour,minute,second,ampm);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvListViewEnsureVisible(PARAM *p, int id, const char *path)
{
char buf[80];

  sprintf(buf,"ensureListVisible(%d,\"%s\")\n",id,path);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvListViewSetOpen(PARAM *p, int id, const char *path, int open)
{
char buf[80];

  sprintf(buf,"setOpen(%d,%d,\"%s\")\n",id,open,path);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetStyle(PARAM *p, int id, int shape, int shadow, int line_width, int margin)
{
char buf[80];

  sprintf(buf,"setStyle(%d,%d,%d,%d,%d)\n",id,shape,shadow,line_width,margin);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetMovie(PARAM *p, int id, int background, const char *filename)
{
char buf[MAX_PRINTF_LENGTH+40];

  sprintf(buf,"movie(%d,0,%d,\"%s\")\n",id,background,filename);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvMovieControl(PARAM *p, int id, int step)
{
char buf[80];

  sprintf(buf,"movie(%d,1,%d\"\")\n",id,step);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvMovieSpeed(PARAM *p, int id, int speed)
{
char buf[80];

  sprintf(buf,"movie(%d,2,%d,\"\")\n",id,speed);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetPixmap(PARAM *p, int id, const char *bmp_file, int download_icon)
{
char buf[MAX_PRINTF_LENGTH+40];
PVB_IMAGE *image;

  if(bmp_file == NULL)
  {
    sprintf(buf,"setPixmap(%d,0,0)\n",id);
    pvtcpsend(p, buf, strlen(buf));
    return 0;
  }
  else if(mystrstr(bmp_file,".png") != NULL || mystrstr(bmp_file,".PNG") != NULL)
  {
    if(download_icon==1) pvDownloadFile(p,bmp_file);
    sprintf(buf,"setPixmap(%d,-2,-2)\n%s\n",id,pvFilename(bmp_file));
    pvtcpsend(p, buf, strlen(buf));
    return 0;
  }

  image = pvbImageRead(bmp_file);
  if(image != NULL && image->bpp == 8)
  {
    sprintf(buf,"setPixmap(%d,%d,%d)\n",id,image->w,image->h);
    pvtcpsend(p, buf, strlen(buf));
    sendBmpToSocket(p, image);
  }
  pvbImageFree(image);
  return 0;
}

int pvSetTablePixmap(PARAM *p, int id, int x, int y, const char *bmp_file, int download_icon)
{
char buf[MAX_PRINTF_LENGTH+40];
PVB_IMAGE *image;

  if(bmp_file == NULL)
  {
    sprintf(buf,"setTablePixmap(%d,%d,%d)\n",id,x,y);
    pvtcpsend(p, buf, strlen(buf));
    sprintf(buf,"(0,0)\n");
    pvtcpsend(p, buf, strlen(buf));
    return 0;
  }
  else if(mystrstr(bmp_file,".png") != NULL || mystrstr(bmp_file,".PNG") != NULL)
  {
    if(download_icon==1) pvDownloadFile(p,bmp_file);
    sprintf(buf,"setTablePixmap(%d,%d,%d)\n",id,x,y);
    pvtcpsend(p, buf, strlen(buf));
    sprintf(buf,"(-2,-2)\n%s\n",pvFilename(bmp_file));
    pvtcpsend(p, buf, strlen(buf));
    return 0;
  }

  image = pvbImageRead(bmp_file);
  if(image != NULL && image->bpp == 8)
  {
    sprintf(buf,"setTablePixmap(%d,%d,%d)\n",id,x,y);
    pvtcpsend(p, buf, strlen(buf));
    sprintf(buf,"(%d,%d)\n",image->w,image->h);
    pvtcpsend(p, buf, strlen(buf));
    sendBmpToSocket(p, image);
  }
  pvbImageFree(image);
  return 0;
}

int pvSetSource(PARAM *p, int id, const char *html_file)
{
char buf[80];

  sprintf(buf,"setSource(%d,\"%s\")\n",id,html_file);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvMoveContent(PARAM *p, int id, int pos)
{
char buf[80];

  sprintf(buf,"moveContent(%d,%d)\n",id,pos);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetChecked(PARAM *p, int id, int state)
{
char buf[80];

  sprintf(buf,"setChecked(%d,%d)\n",id,state);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvPrintf(PARAM *p, int id, const char *format, ...)
{
  char text[MAX_PRINTF_LENGTH+40];

  va_list ap;
  va_start(ap,format);
#ifdef PVWIN32
  _vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
#ifdef __VMS
  vsprintf(text, format, ap);
#endif
#ifdef PVUNIX
  vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
  va_end(ap);

  pvSetText(p,id,text);
  return 0;
}

int pvHide(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"hide(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvShow(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"show(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvPrint(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"print(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSave(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"save(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetGeometry(PARAM *p, int id, int x, int y, int w, int h)
{
char buf[80];

  sprintf(buf,"setGeometry(%d,%d,%d,%d,%d)\n",id,x,y,w,h);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetMinSize(PARAM *p, int id, int w, int h)
{
char buf[80];

  sprintf(buf,"setMinimumSize(%d,%d,%d)\n",id,w,h);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetMaxSize(PARAM *p, int id, int w, int h)
{
char buf[80];

  sprintf(buf,"setMaximumSize(%d,%d,%d)\n",id,w,h);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetAlignment(PARAM *p, int id, int alignment)
{
char buf[80];

  sprintf(buf,"setAlignment(%d,%d)\n",id,alignment);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetBackgroundColor(PARAM *p, int id, int r, int g, int b)
{
char buf[80];

  sprintf(buf,"setBackgroundColor(%d,%d,%d,%d)\n",id,r,g,b);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetPaletteBackgroundColor(PARAM *p, int id, int r, int g, int b)
{
char buf[80];

  sprintf(buf,"setPaletteBackgroundColor(%d,%d,%d,%d)\n",id,r,g,b);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetPaletteForegroundColor(PARAM *p, int id, int r, int g, int b)
{
char buf[80];

  sprintf(buf,"setPaletteForegroundColor(%d,%d,%d,%d)\n",id,r,g,b);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetFontColor(PARAM *p, int id, int r, int g, int b)
{
char buf[80];

  sprintf(buf,"setFontColor(%d,%d,%d,%d)\n",id,r,g,b);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetFont(PARAM *p, int id, const char *family, int pointsize, int bold, int italic , int underline, int strikeout)
{
char buf[MAX_PRINTF_LENGTH];

  sprintf(buf,"setFont(%d,%d,%d,%d,%d,%d,\"%s\")\n",id,pointsize,bold,italic,underline,strikeout,family);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetImage(PARAM *p, int id, const char *filename)
{
char buf[MAX_PRINTF_LENGTH];

  pv_length_check(p,filename);
  sprintf(buf,"setImage(%d,\"%s\")\n",id,pvFilename(filename));
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

char *pvGetTextParam(PARAM *p, const char *command)
{
  pvGetText(command,p->parse_event_struct.text);
  return p->parse_event_struct.text;
}

int pvGetText(const char *command, char *text)
{
  *text = '\0';
  while(*command != '"' && *command != '\0') command++;
  if(*command == '\0') return 0;
  command++;
  while(*command != '"' && *command != '\0') *text++ = *command++;
  *text = '\0';
  return 0;
}

PARSE_EVENT_STRUCT *pvParseEventStruct(PARAM *p, const char *event)
{
  int i,str_end;

  p->parse_event_struct.event = pvParseEvent(event, &p->parse_event_struct.i,
                                                    &p->parse_event_struct.text[0]);
  str_end = 0;
  for(i=0; i < MAX_EVENT_LENGTH; i++)
  {
    if(p->parse_event_struct.text[i] == '\0') str_end = 1;
    if(str_end == 1) p->parse_event_struct.text[i] = '\0';
  }

  return &p->parse_event_struct;
}

int pvParseEvent(const char *event, int *id, char *text)
{
  *id = 0;
  *text = '\0';
  if(event[0] == '\0') return NULL_EVENT;
  else if(strncmp(event,"QPushButton(",12) == 0)
  {
    sscanf(event,"QPushButton(%d",id);
    pvGetText(event, text);
    return BUTTON_EVENT;
  }
  else if(strncmp(event,"QPushButtonPressed(",19) == 0)
  {
    sscanf(event,"QPushButtonPressed(%d)",id);
    return BUTTON_PRESSED_EVENT;
  }
  else if(strncmp(event,"QPushButtonReleased(",20) == 0)
  {
    sscanf(event,"QPushButtonReleased(%d)",id);
    return BUTTON_RELEASED_EVENT;
  }
  else if(strncmp(event,"text",4) == 0)
  {
    sscanf(event,"text(%d",id);
    pvGetText(event, text);
    return TEXT_EVENT;
  }
  else if(strncmp(event,"idleGL(",7) == 0)
  {
    sscanf(event,"idleGL(%d)",id);
    return GL_IDLE_EVENT;
  }
  else if(strncmp(event,"paintGL(",8) == 0)
  {
    sscanf(event,"paintGL(%d)",id);
    return GL_PAINT_EVENT;
  }
  else if(strncmp(event,"initializeGL(",13) == 0)
  {
    sscanf(event,"initializeGL(%d)",id);
    return GL_INITIALIZE_EVENT;
  }
  else if(strncmp(event,"resizeGL(",9) == 0)
  {
    int w,h;
    sscanf(event,"resizeGL(%d,%d,%d)",id,&w,&h);
    sprintf(text,"(%d,%d)",w,h);
    return GL_RESIZE_EVENT;
  }
  else if(strncmp(event,"slider(",7) == 0)
  {
    int   ival;
    float fval;
    if(mystrstr(event,".") == NULL)
    {
      sscanf(event,"slider(%d,%d)",id,&ival);
      sprintf(text,"(%d)",ival);
    }
    else
    {
      sscanf(event,"slider(%d,%f)",id,&fval);
      sprintf(text,"(%f)",fval);
    }
    return SLIDER_EVENT;
  }
  else if(strncmp(event,"check(",6) == 0)
  {
    int val;
    sscanf(event,"check(%d,%d)",id,&val);
    sprintf(text,"(%d)",val);
    return CHECKBOX_EVENT;
  }
  else if(strncmp(event,"radio(",6) == 0)
  {
    int val;
    sscanf(event,"radio(%d,%d)",id,&val);
    sprintf(text,"(%d)",val);
    return RADIOBUTTON_EVENT;
  }
  else if(strncmp(event,"tab(",4) == 0)
  {
    int val;
    sscanf(event,"tab(%d,%d)",id,&val);
    sprintf(text,"(%d)",val);
    return TAB_EVENT;
  }
  else if(strncmp(event,"QListBox(",9) == 0)
  {
    sscanf(event,"QListBox(%d",id);
    pvGetText(event, text);
    return TEXT_EVENT;
  }
  else if(strncmp(event,"QTableValue(",12) == 0)
  {
    char text2[MAX_EVENT_LENGTH];
    int row,col;
    sscanf(event,"QTableValue(%d,%d,%d",id,&row,&col);
    pvGetText(event, text2);
    sprintf(text,"(%d,%d,\"%s\")",col,row,text2);
    return TABLE_TEXT_EVENT;
  }
  else if(strncmp(event,"QTable(",7) == 0)
  {
    int row,col,button;
    sscanf(event,"QTable(%d,%d,%d,%d",id,&row,&col,&button);
    sprintf(text,"(%d,%d,%d)",col,row,button);
    return TABLE_CLICKED_EVENT;
  }
  else if(strncmp(event,"selected(",9) == 0)
  {
    int col;
    char text2[MAX_EVENT_LENGTH];
    sscanf(event,"selected(%d,%d",id,&col);
    pvGetText(event, text2);
    sprintf(text,"(%d,\"%s\")",col,text2);
    return SELECTION_EVENT;
  }
  else if(strncmp(event,"@clipboard(",11) == 0)
  {
    int size;
    sscanf(event,"@clipboard(%d,%d",id,&size);
    sprintf(text,"(%d)",size);
    return CLIPBOARD_EVENT;
  }
  else if(strncmp(event,"QMouseRight(",12) == 0)
  {
    sscanf(event,"QMouseRight(%d)",id);
    pvGetText(event, text);
    return RIGHT_MOUSE_EVENT;
  }
  else if(strncmp(event,"key(",4) == 0)
  {
    int key;
    char text2[MAX_EVENT_LENGTH];
    sscanf(event,"key(%d,%d",id,&key);
    pvGetText(event, text2);
    sprintf(text,"(%d,\"%s\")",key,text2);
    return KEYBOARD_EVENT;
  }
  else if( strncmp( event, "QPlotMouseMoved(",16) == 0 )
  {
    float fvalx,fvaly;
    sscanf( event, "QPlotMouseMoved(%d,%f,%f)", id, &fvalx, &fvaly );
    sprintf(text,"(%f,%f)",fvalx,fvaly);
    return PLOT_MOUSE_MOVED_EVENT;
  }
  else if( strncmp( event, "QPlotMousePressed(",18) == 0 )
  {
    float fvalx,fvaly;
    sscanf( event, "QPlotMousePressed(%d,%f,%f)", id, &fvalx, &fvaly );
    sprintf(text,"(%f,%f)",fvalx,fvaly);
    return PLOT_MOUSE_PRESSED_EVENT;
  }
  else if( strncmp( event, "QPlotMouseReleased(",19) == 0 )
  {
    float fvalx,fvaly;
    sscanf( event, "QPlotMouseReleased(%d,%f,%f)", id, &fvalx, &fvaly );
    sprintf(text,"(%f,%f)",fvalx,fvaly);
    return PLOT_MOUSE_RELEASED_EVENT;
  }
  else if( strncmp( event, "mouseEnterLeave(",16) == 0 )
  {
    int enter;
    sscanf( event, "mouseEnterLeave(%d,%d)", id, &enter );
    sprintf(text,"%d",enter);
    return MOUSE_OVER_EVENT;
  }
  else if(strncmp(event,"user(",5) == 0)
  {
    sscanf(event,"user(%d",id);
    pvGetText(event, text);
    return USER_EVENT;
  }

  return NULL_EVENT;
}

/* own version supporting only bmp files with 8bpp */
int pvQImage(PARAM *p, int id, int parent, const char *imagename, int *w, int *h, int *depth)
{
PVB_IMAGE *image;
char buf[MAX_PRINTF_LENGTH+40];

  if(mystrstr(imagename,".bmp") == NULL && mystrstr(imagename,".BMP") == NULL)
  { // image format other than 8bpp bmp
    *w = *h = *depth = 0;
    sprintf(buf,"QImage(%d,%d,%d,%d,%d,\"%s\")\n",id,parent,*w,*h,*depth,pvFilename(imagename));
    pvtcpsend(p, buf, strlen(buf));
    return 0;
  }
  else
  {
    image = pvbImageRead(imagename);
    if(image != NULL && image->bpp == 8)
    {
      *w = image->w;
      *h = image->h;
      *depth = image->bpp;
      sprintf(buf,"QImage(%d,%d,%d,%d,%d)\n",id,parent,*w,*h,*depth);
      pvtcpsend(p, buf, strlen(buf));
      sendBmpToSocket(p, image);
    }
    pvbImageFree(image);
  }
  if(*depth != 8) return -1;
  return 0;
}

int pvQGL(PARAM *p, int id, int parent)
{
char buf[80];

#ifndef DO_NOT_USE_OPEN_GL
  glencode_set_param(p);
#endif
  sprintf(buf,"QPvGlWidget(%d,%d)\n",id,parent);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvMove(PARAM *p, int id, int x, int y)
{
char buf[80];

  sprintf(buf,"move(%d,%d,%d)\n",id,x,y);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvResize(PARAM *p, int id, int w, int h)
{
char buf[80];

  sprintf(buf,"resize(%d,%d,%d)\n",id,w,h);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvCopyToClipboard(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"gCopyToClipboard(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSave(PARAM *p, int id, const char *filename)
{
char buf[MAX_PRINTF_LENGTH+40];

  pv_length_check(p,filename);
  sprintf(buf,"gSave(%d,\"%s\")\n",id,filename);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSaveAsBmp(PARAM *p, int id, const char *filename)
{
char buf[MAX_PRINTF_LENGTH];

  pv_length_check(p,filename);
  sprintf(buf,"gSaveAsBmp(%d,\"%s\")\n",id,filename);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetZoomX(PARAM *p, int id, float zoom)
{
char buf[80];

  sprintf(buf,"gSetZoomX(%d,%f)\n",id,zoom);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSetZoomY(PARAM *p, int id, float zoom)
{
char buf[80];

  sprintf(buf,"gSetZoomY(%d,%f)\n",id,zoom);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvDisplayNum(PARAM *p, int id, int num)
{
char buf[80];

  sprintf(buf,"displayNum(%d,%d)\n",id,num);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvDisplayFloat(PARAM *p, int id, float val)
{
char buf[80];

  sprintf(buf,"displayFloat(%d,%f)\n",id,val);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvDisplayStr(PARAM *p, int id, const char *str)
{
char buf[80];

  sprintf(buf,"displayStr(%d,\"%s\")\n",id,str);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvAddTab(PARAM *p, int id, int id_child, const char *str)
{
char buf[80];

  sprintf(buf,"addTab(%d,%d,\"%s\")\n",id,id_child,str);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvFileDialog(PARAM *p, int id_return, int type)
{
char buf[80];

  if(type < 0 || type >= 3)
  {
    printf("error pvFileDialog: type=0|1|2 FileOpenDialog|FileSaveDialog|FindDirectoryDialog\n");
    return -1;
  }
  sprintf(buf,"fileDialog(%d,%d)\n",id_return,type);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvPopupMenu(PARAM *p, int id_return, const char *text)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);
  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"popupMenu(%d,\"%s\")\n",id_return,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvMessageBox(PARAM *p, int id_return, int type, const char *text, int button0, int button1, int button2)
{
char buf[MAX_PRINTF_LENGTH+40],text2[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  if(type < 0 || type > 2)
  {
    printf("error pvMessageBox: type=0|1|2 BoxInformation|BoxWarning|BoxCritical\n");
    return -1;
  }
  strcpy(text2,text);
  while(1)
  {
    cptr = mystrchr(text2,'\n');
    if(cptr != NULL) *cptr = 12; // insert FF instead
    else             break;
  }
  sprintf(buf,"messageBox(%d,%d,%d,%d,%d,\"%s\")\n",id_return,type,button0,button1,button2,text2);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvInputDialog(PARAM *p, int id_return, const char *text, const char *default_text)
{
char buf[MAX_PRINTF_LENGTH+40],text2[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);
  pv_length_check(p,default_text);

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  strcpy(text2,text);
  while(1)
  {
    cptr = mystrchr(text2,'\n');
    if(cptr != NULL) *cptr = 12; // FF instead
    else             break;
  }
  sprintf(buf,"inputDialog(%d,\"%s\")\n",id_return,text2);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"%s\n",default_text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvHyperlink(PARAM *p, const char *link)
{
char buf[MAX_PRINTF_LENGTH+40];

  if(strlen(link) > MAX_PRINTF_LENGTH)
  {
    printf("link tto long %s\n",link);
    return -1;
  }
  sprintf(buf,"hyperlink(\"%s\")\n",link);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

/********* vtk functions ************************************************************/
int pvVtkTcl(PARAM *p, int id, const char *tcl_command)
{
  char buf[MAX_PRINTF_LENGTH+40];
  int len;

  pv_length_check(p,tcl_command);
  len = strlen(tcl_command);
  if(len>0)
  {
    sprintf(buf,"vtkTcl(%d)\n",id);
    pvtcpsend(p, buf, strlen(buf));
    pvtcpsend(p, tcl_command, strlen(tcl_command));
    if(tcl_command[len-1] != '\n') pvtcpsend(p,"\n", 1);
  }
  return 0;
}

int pvVtkTclPrintf(PARAM *p, int id, const char *format, ...)
{
  char text[MAX_PRINTF_LENGTH+40];

  va_list ap;
  va_start(ap,format);
#ifdef PVWIN32
  _vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
#ifdef __VMS
  vsprintf(text, format, ap);
#endif
#ifdef PVUNIX
  vsnprintf(text, MAX_PRINTF_LENGTH - 1, format, ap);
#endif
  va_end(ap);

  return pvVtkTcl(p,id,text);
}

int pvVtkTclScript(PARAM *p, int id, const char *filename)
{
  char buf[MAX_PRINTF_LENGTH+40];

  pv_length_check(p,filename);

  pvDownloadFile(p,filename);
  sprintf(buf,"vtkTclScript(%d,\"%s\")\n",id,pvFilename(filename));
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

/********* graphic functions ********************************************************/
static FILE *gfp = NULL;

int gWriteFile(const char *file)
{
  gfp = fopen(file,"w");
  if(gfp == NULL)
  {
    printf("could not write %s\n",file);
    return -1;
  }
  return 0;
}

int gCloseFile()
{
  if(gfp != NULL) fclose(gfp);
  gfp = NULL;
  return 0;
}

int gBeginDraw(PARAM *p, int id)
{
char buf[80];

  sprintf(buf,"gbeginDraw(%d)\n",id);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int gBox(PARAM *p, int x, int y, int w, int h)
{
char buf[80];

  sprintf(buf,"gbox(%d,%d,%d,%d)\n",x,y,w,h);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gRect(PARAM *p, int x, int y, int w, int h)
{
char buf[80];

  sprintf(buf,"grect(%d,%d,%d,%d)\n",x,y,w,h);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gEndDraw(PARAM *p)
{
char buf[80];

  sprintf(buf,"gendDraw\n");
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  gSetFont(p, "arial", 10, Normal, NORMAL);
  return 0;
}

int gLineTo(PARAM *p, int x, int y)
{
char buf[80];

  sprintf(buf,"glineTo(%d,%d)\n",x,y);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gLine(PARAM *p, float *x, float *y, int n)
{
char buf[80];
int  i;

  sprintf(buf,"gline({\n");
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  for(i=0; i<n; i++)
  {
    sprintf(buf,"%f,%f\n",x[i],y[i]);
    if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
    else            fprintf(gfp,"%s",buf);
  }
  sprintf(buf,"})\n");
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gBufferedLine(PARAM *p)
{
  gLine(p,p->x,p->y,p->nxy);
  return 0;
}

int gMoveTo(PARAM *p, int x, int y)
{
char buf[80];

  sprintf(buf,"gmoveTo(%d,%d)\n",x,y);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gRightYAxis(PARAM *p, float start, float delta, float end, int draw)
{
char buf[80];

  sprintf(buf,"grightYAxis(%f,%f,%f,%d)\n",start,delta,end,draw);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gSetColor(PARAM *p, int r, int g, int b)
{
char buf[80];

  sprintf(buf,"gsetColor(%d,%d,%d)\n",r,g,b);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gSetWidth(PARAM *p, int w)
{
char buf[80];

  sprintf(buf,"gsetWidth(%d)\n",w);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gSetStyle(PARAM *p, int style)
{
char buf[80];

  sprintf(buf,"gsetStyle(%d)\n",style);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gDrawArc(PARAM *p, int x, int y, int w, int h, int start_angle, int angle_length)
{
char buf[80];

  sprintf(buf,"gdrawArc(%d,%d,%d,%d,%d,%d)\n",x,y,w,h,start_angle,angle_length);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gDrawPie(PARAM *p, int x, int y, int w, int h, int start_angle, int angle_length)
{
char buf[80];

  sprintf(buf,"gdrawPie(%d,%d,%d,%d,%d,%d)\n",x,y,w,h,start_angle,angle_length);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gDrawPolygon(PARAM *p, int *x, int *y, int n)
{
char buf[80];
int  i;

  sprintf(buf,"gdrawPolygon(%d){\n",n);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  for(i=0; i<n; i++)
  {
    sprintf(buf,"%d,%d\n",x[i],y[i]);
    if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
    else            fprintf(gfp,"%s",buf);
  }
  if(gfp == NULL) pvtcpsend(p,"}\n",2);
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gSetFont(PARAM *p, const char *family, int size, int weight, int italic)
{
char buf[80];

  sprintf(buf,"gsetFont(\"%s\",%d,%d,%d)\n",family,size,weight,italic);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gSetLinestyle(PARAM *p, int style)
{
char buf[80];

  sprintf(buf,"gsetLinestyle(%d)\n",style);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gText(PARAM *p, int x, int y, const char *text, int alignment)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);
  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"gtext(%d,%d,%d,\"%s\")\n",x,y,alignment,text);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gTextInAxis(PARAM *p, float x, float y, const char *text, int alignment)
{
char buf[MAX_PRINTF_LENGTH+40],*cptr;

  pv_length_check(p,text);
  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"gtextInAxis(%f,%f,%d,\"%s\")\n",x,y,alignment,text);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gSetFloatFormat(PARAM *p, const char *text)
{
char buf[80],txt[40];

  if(strlen(text) > 64)
  {
    char message[80];
    strncpy(message,text,10);
    message[10] = '\0';
    strcat(message,"... is too long -> exit");
    pvThreadFatal(p,message);
  }

  strncpy(txt,text,sizeof(txt)-1);
  txt[sizeof(txt)-1] = '\0';
  sprintf(buf,"gsetFloatFormat(\"%s\")\n",txt);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gXAxis(PARAM *p, float start, float delta, float end, int draw)
{
char buf[80];

  sprintf(buf,"gxAxis(%f,%f,%f,%d)\n",start,delta,end,draw);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gYAxis(PARAM *p, float start, float delta, float end, int draw)
{
char buf[80];

  sprintf(buf,"gyAxis(%f,%f,%f,%d)\n",start,delta,end,draw);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gXGrid(PARAM *p)
{
char buf[80];

  sprintf(buf,"gxGrid\n");
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gYGrid(PARAM *p)
{
char buf[80];

  sprintf(buf,"gyGrid\n");
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gBoxWithText(PARAM *p, int x, int y, int w, int h, int fontsize,
                 const char *xlabel, const char *ylabel, const char *rylabel)
{
  gBox(p,x,y,w,h);
  if(xlabel  != NULL) gText(p,x+w/2,y+h+fontsize*2,xlabel,ALIGN_CENTER);
  if(ylabel  != NULL) gText(p,x-fontsize*4,y+h/2,ylabel,ALIGN_VERT_CENTER);
  if(rylabel != NULL) gText(p,x+w+fontsize*4,y+h/2,rylabel,ALIGN_VERT_CENTER);
  return 0;
}

int gComment(PARAM *p, const char *comment)
{
char buf[MAX_PRINTF_LENGTH];

  pv_length_check(p,comment);
  sprintf(buf,"# %s\n",comment);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gPlaySVG(PARAM *p, const char *filename)
{
char buf[MAX_PRINTF_LENGTH];

  pv_length_check(p,filename);
  sprintf(buf,"gplaySVG(\"%s\")\n",filename);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gSocketPlaySVG(PARAM *p, const char *svgstring)
{
char buf[MAX_PRINTF_LENGTH];

  sprintf(buf,"gplaySVGsocket()\n");
  pvtcpsend(p, buf, strlen(buf));
  pvtcpsend_binary(p, svgstring, strlen(svgstring));
  sprintf(buf,"\n<svgend></svgend>\n");
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int gTranslate(PARAM *p, float x, float y)
{
char buf[MAX_PRINTF_LENGTH];

  sprintf(buf,"gtranslate(%f,%f)\n",x,y);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gRotate(PARAM *p, float angle)
{
char buf[MAX_PRINTF_LENGTH];

  sprintf(buf,"grotate(%f)\n",angle);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int gScale(PARAM *p, float sx, float sy)
{
char buf[MAX_PRINTF_LENGTH];

  sprintf(buf,"gscale(%f,%f)\n",sx,sy);
  if(gfp == NULL) pvtcpsend(p, buf, strlen(buf));
  else            fprintf(gfp,"%s",buf);
  return 0;
}

int pvSetSelector(PARAM *p, int id, int state)
{
char buf[80];

  sprintf(buf,"setSelector(%d,%d)\n",id,state);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvSendFile(PARAM *p, const char *filename)
{
FILE *fp;
char line[1024];

  fp = fopen(filename,"r");
  if(fp == NULL)
  {
    printf("pvSendFile: could not open %s",filename);
    return -1;
  }
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    pvtcpsend(p,line,strlen(line));
  }
  fclose(fp);
  return 0;
}

int pvSendOpenGL(PARAM *p, const char *filename,
                           GLuint *listarray, int max_array,
                           glFont *proportional, glFont *fixed)
{
FILE *fp;
char  line[1024], *cptr;
int   ind, opcode;
int   height, pitch, bold, italic, underlined, width_scale, flags;
int   oblique, rotation;
float x, y, fzoom;
glFont *font = NULL;

  fzoom = 1.0f;
  if(proportional != NULL)
  {
    proportional->setFontSize(10, fzoom);
  }  
  if(fixed        != NULL) 
  {
    fixed->setFontSize(10, fzoom);
  }  
  ind = opcode = -1;
  fp = fopen(filename,"r");
  if(fp == NULL)
  {
    printf("pvSendOpenGL: could not open %s",filename);
    return -1;
  }
  while(fgets(line,sizeof(line)-1,fp) != NULL)
  {
    if(line[0] != '/')
    {
      if(line[0]=='g' && line[1]=='l')
      {
        if(strncmp(line,"glNewList(",10) == 0)
        {
          sscanf(line,"glNewList(listarray[%d", &ind);
          cptr = mystrchr(line,',');
          if(cptr != NULL)
          {
            cptr++;
            sscanf(cptr,"%d", &opcode);
            if(ind < max_array) glNewList(listarray[ind],opcode);  // eg. GL_COMPILE
          }
        }
        else if(strncmp(line,"glEndList(",10) == 0)
        {
          glEndList();
        }
        else
        {
          pvtcpsend(p,line,strlen(line));
        }
      }
      else
      {
        if(strncmp(line,"listarray[",10) == 0)
        {
          sscanf(line,"listarray[%d", &ind);
          if(ind >= 0 && ind < max_array) listarray[ind] = glGenLists(1);
          else { printf("pvSendOpenGL ind=%d is too big\n", ind); return -1; }
        }
        else if(strncmp(line,"setFont(",8) == 0)
        {
          sscanf(line,"setFont(%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &height,
            &pitch,
            &bold,
            &italic,
            &underlined,
            &width_scale,
            &flags,
            &oblique,
            &rotation
            );
          /*
          printf("setFont: height=%d pitch=%d width_scale=%d oblique=%d rotation=%d\n", 
                  height,
                  pitch,
                  width_scale,
                  oblique,
                  rotation
                  );
          */        
          if(pitch == 2)
          {
            if     (proportional != NULL) font = proportional;
          }            
          else
          {
            if     (fixed        != NULL) font = fixed;
            else if(proportional != NULL) font = proportional;
          }
          if(font != NULL && height < 1000)
          {
            font->setFontSize(height, fzoom);
            font->setRotation(rotation);
          }  
        }
        else if(strncmp(line,"drawText(",9) == 0)
        {
          sscanf(line,"drawText(%f,%f", &x, &y);
          cptr = mystrrchr(line,'\"');
          if(cptr != NULL) *cptr = '\0';    // text termination
          cptr = mystrchr(line,'\"');
          if(cptr != NULL) cptr++;          // text begin
          else             cptr = &line[0];
          if(font != NULL) font->drawString(x,y,cptr);
          //printf("drawText: x=%f y=%f \"%s\"\n", x, y, cptr);
        }
      }
    }
    else
    {
      if(strncmp(line,"argv[",5) == 0)
      {
        cptr = mystrstr(line,"-zoom=");
        if(cptr != NULL)
        {
          sscanf(cptr,"-zoom=%f", &fzoom);
        }
      }
      else if(line[4] == 'a')
      {
        cptr = mystrstr(line,"zoom=");
        if(cptr != NULL)
        {
          sscanf(cptr,"zoom=%f", &fzoom);
        }
      }
    }
  }
  fclose(fp);
  return ind+1;
}

int pvDownloadFileAs(PARAM *p, const char *filename, const char *newname)
{
char  buf[MAX_PRINTF_LENGTH+40];
int   ret,fhdl;
short len;

  if(filename == NULL)      return 1;
  if(newname == NULL)       return 1;
  if(strlen(filename) <= 0) return 1;

  //#ifdef pv_CACHE
  if(pv_cache == 1)
  {
#ifndef __VMS
    struct tm *local;
    struct stat statbuf;

    //printf("test time of %s\n", filename);
    ret = stat(filename, &statbuf);
    if(ret >= 0)
    {
      local = localtime(&statbuf.st_mtime);
      // send local file modification time
      sprintf(buf,"fileCache(%d,%d,%d,%d,%d,%d,%d,%d,%d,\"%s\")\n"
      ,local->tm_sec
      ,local->tm_min
      ,local->tm_hour
      ,local->tm_mon
      ,local->tm_year
      ,local->tm_mday
      ,local->tm_wday
      ,local->tm_yday
      ,local->tm_isdst
      ,newname);
      pvtcpsend(p, buf, strlen(buf));
      while(1) // wait for response
      {
        pvtcpreceive(p, buf, sizeof(buf) - 1);
        if(strncmp(buf,"cache(1)",8) == 0) return 0; // file was cached
        if(strncmp(buf,"cache(",6)   == 0) break;    // file must be resend again
      }
    }
#endif
  //#endif
  }
  //printf("download %s again\n", filename);

  pv_length_check(p,newname);

  fhdl = pvopenBinary(filename);
  if(fhdl <= 0)
  {
    printf("pvDownloadFile: could not open %s\n",filename);
    return -1;
  }

  sprintf(buf,"downloadFile(\"%s\")\n",pvFilename(newname));
  pvtcpsend(p, buf, strlen(buf));
  while(1)
  {
    ret = pvmyread(fhdl, buf, sizeof(buf));
    if(ret <= 0) break;
    len = htons(ret);
    pvtcpsend_binary(p, (const char *) &len, 2);
    pvtcpsend_binary(p, buf, ret);
  }
  len = htons(0);
  pvtcpsend_binary(p, (const char *) &len, 2);
  close(fhdl);
  return 0;
}

int pvDownloadFile(PARAM *p, const char *filename)
{
  return pvDownloadFileAs(p,filename,filename);
}

int pvTerminateModalDialog(PARAM *p)
{
  char buf[80];

  if(p->modal != 1) return -1;
  p->modal = 0;
  sprintf(buf,"modalTerminate()\n");
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvRunModalDialogScript(PARAM *p, int width, int height)
{
  char buf[80];

  if(p->modal != 0)
  {
    printf("you can not call a ModalDialog from a ModalDialog");
    return -1;
  }
  p->modal = 1;
  sprintf(buf,"modalInit(%d,%d)\n",width,height);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvRunModalDialog(PARAM *p, int width, int height, int (*showMask)(PARAM *p), void *userData,
                     int (*readData)(void *d),
                     int (*showData)(PARAM *p, void *d),
                     void *d)
{
  int ret;
  char buf[80];

  if(p->modal != 0)
  {
    printf("you can not call a ModalDialog from a ModalDialog");
    return -1;
  }
  p->modal = 1;
  sprintf(buf,"modalInit(%d,%d)\n",width,height);
  pvtcpsend(p, buf, strlen(buf));
  p->readData = readData;
  p->showData = showData;
  p->modalUserData = userData;
  p->modal_d = d;
  ret = showMask(p);
  pvTerminateModalDialog(p);
  return ret;
}

int pvUpdateBaseWindow(PARAM *p)
{
  char buf[80];

  if(p->modal    != 1)    return -1;
  //if(p->readData == NULL) return -1;
  //if(p->showData == NULL) return -1;
  if(p->modal_d  == NULL) return -1;
  sprintf(buf,"modalBase()\n");
  pvtcpsend(p, buf, strlen(buf));
  if(p->readData != NULL) p->readData(p->modal_d);
  if(p->showData != NULL) p->showData(p,p->modal_d);
  sprintf(buf,"modalModal()\n");
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int pvUpdateBaseWindowOnOff(PARAM *p, int onoff)
{
  char buf[80];

  if(onoff == 1)
  {
    sprintf(buf,"modalBase()\n");
    pvtcpsend(p, buf, strlen(buf));
  }
  else
  {
    sprintf(buf,"modalModal()\n");
    pvtcpsend(p, buf, strlen(buf));
  }  
  return 0;
}

int pvAddDockWidget(PARAM *p,const char *title, int dock_id, int root_id, int allow_close, int floating, int allow_left, int allow_right, int allow_top, int allow_bottom)
{
  char buf[80];
  if(dock_id <  ID_DOCK_WIDGETS )                   return -1;
  if(dock_id >= ID_DOCK_WIDGETS + MAX_DOCK_WIDGETS) return -1;

  sprintf(buf,"addDockWidget(%d,%d,%d,%d,%d,%d,%d,%d,)\n", dock_id, root_id, allow_close, floating, allow_left, allow_right, allow_top, allow_bottom);
  pvtcpsend(p, buf, strlen(buf));
  pvtcpsend(p, title, strlen(title));
  pvtcpsend(p, "\n", strlen("\n"));
  return 0;
}

//-------------------------------------------------------
// QwtPlotWidget
//-------------------------------------------------------
int qpwSetCurveData(PARAM *p, int id, int c, int count, double *x, double *y)
{
  char buf[80];
  int i;

  if(count <= 0) return -1;
  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setCurveData(%d,%d)\n",c,count);
  pvtcpsend(p, buf, strlen(buf));
  for(i=0; i<count; i++)
  {
    sprintf(buf,"%lf %lf\n",x[i],y[i]);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int qpwSetBufferedCurveData(PARAM *p, int id, int c)
{
  char buf[80];
  int i;

  if(p->nxy <= 0) return -1;
  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setCurveData(%d,%d)\n",c,p->nxy);
  pvtcpsend(p, buf, strlen(buf));
  for(i=0; i<p->nxy; i++)
  {
    sprintf(buf,"%f %f\n",p->x[i],p->y[i]);
    pvtcpsend(p, buf, strlen(buf));
  }
  return 0;
}

int qpwReplot(PARAM *p, int id)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"replot()\n");
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetTitle(PARAM *p, int id, const char *text)
{
  char buf[1024],*cptr;

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setTitle(\"%s\")\n",text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetCanvasBackground(PARAM *p, int id, int r, int g, int b)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setCanvasBackground(%d,%d,%d)\n",r,g,b);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwEnableOutline(PARAM *p, int id, int val)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"enableOutline(%d)\n",val);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetOutlinePen(PARAM *p, int id, int r, int g, int b)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setOutlinePen(%d,%d,%d)\n",r,g,b);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetAutoLegend(PARAM *p, int id, int val)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setAutoLegend(%d)\n",val);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwEnableLegend(PARAM *p, int id, int val)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"enableLegend(%d)\n",val);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetLegendPos(PARAM *p, int id, int pos)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setLegendPos(%d)\n",pos);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetLegendFrameStyle(PARAM *p, int id, int style)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setLegendFrameStyle(%d)\n",style);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwEnableGridXMin(PARAM *p, int id)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"enableGridXMin()\n");
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetGridMajPen(PARAM *p, int id, int r, int g, int b, int style)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setGridMajPen(%d,%d,%d,%d)\n",r,g,b,style);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetGridMinPen(PARAM *p, int id, int r, int g, int b, int style)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setGridMinPen(%d,%d,%d,%d)\n",r,g,b,style);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwEnableAxis(PARAM *p, int id, int pos)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"enableAxis(%d)\n",pos);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetAxisTitle(PARAM *p, int id, int pos, const char *text)
{
  char buf[1024],*cptr;

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setAxisTitle(%d,\"%s\")\n",pos,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetAxisOptions(PARAM *p, int id, int pos, int val)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setAxisOptions(%d,%d)\n",pos,val);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetAxisMaxMajor(PARAM *p, int id, int pos, int val)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setAxisMaxMajor(%d,%d)\n",pos,val);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetAxisMaxMinor(PARAM *p, int id, int pos, int val)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setAxisMaxMinor(%d,%d)\n",pos,val);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwInsertCurve(PARAM *p, int id, int index, const char *text)
{
  char buf[MAX_EVENT_LENGTH],*cptr;

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
#ifdef PVUNIX  
  snprintf(buf,sizeof(buf)-1,"insertCurve(%d,\"%s\")\n",index,text);
#endif  
#ifdef PVWIN32  
  _snprintf(buf,sizeof(buf)-1,"insertCurve(%d,\"%s\")\n",index,text);
#endif  
#ifdef __VMS
  sprintf(buf,"insertCurve(%d,\"%s\")\n",index,text);
#endif  
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwRemoveCurve(PARAM *p, int id, int index)
{
  char buf[MAX_EVENT_LENGTH];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"removeCurve(%d)\n",index);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetCurvePen(PARAM *p, int id, int index, int r, int g, int b, int width, int style)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setCurvePen(%d,%d,%d,%d,%d,%d)\n",index,r,g,b,width,style);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetCurveSymbol(PARAM *p, int id, int index, int symbol, int r1, int g1, int b1,
                                                               int r2, int g2, int b2,
                                                               int w, int h)
{
  char buf[200];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setCurveSymbol(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",index,symbol,
                               r1,g1,b1,
                               r2,g2,b2,
                               w,h);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetCurveYAxis(PARAM *p, int id, int index, int pos)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setCurveYAxis(%d,%d)\n",index,pos);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwInsertMarker(PARAM *p, int id, int index)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"insertMarker(%d)\n",index);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetMarkerLineStyle(PARAM *p, int id, int index, int style)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMarkerLineStyle(%d,%d)\n",index,style);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetMarkerPos(PARAM *p, int id, int index, float x, float y)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMarkerPos(%d,%f,%f)\n",index,x,y);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetMarkerLabelAlign(PARAM *p, int id, int index, int align)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMarkerLabelAlign(%d,%d)\n",index,align);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetMarkerLabel(PARAM *p, int id, int pos, const char * text)
{
  char buf[256],*cptr;
  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf, "qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf, "setMarkerLabel(%d,\"%s\")\n", pos, text );
  pvtcpsend(p, buf, strlen(buf));
  printf(buf );
  return 0;
}

int qpwSetMarkerPen(PARAM *p, int id, int index, int r, int g, int b, int style)
{
  char buf[80];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMarkerPen(%d,%d,%d,%d,%d)\n",index,r,g,b,style);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetMarkerFont(PARAM *p, int id, int index, const char *family, int size, int style)
{
  char buf[1024];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMarkerFont(%d,%d,%d,\"%s\")\n",index,size,style,family);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetMarkerSymbol(PARAM *p, int id, int index, int symbol, int r1, int g1, int b1,
                                                                int r2, int g2, int b2,
                                                                int w, int h)
{
  char buf[200];

  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMarkerSymbol(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",
                               index, symbol,
                               r1,g1,b1,
                               r2,g2,b2,
                               w,h);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwInsertLineMarker(PARAM *p, int id, int index, const char *text, int pos)
{
  char buf[80],*cptr;

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"qpw(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"insertLineMarker(%d,%d,\"%s\")\n",index,pos,text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qpwSetAxisScaleDraw( PARAM *p, int id, int pos, const char * text )
{
  char buf[256],*cptr;
  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf( buf, "qpw(%d)\n",id);
  pvtcpsend( p, buf, strlen(buf));
  sprintf( buf, "setAxisScaleDraw(%d,\"%s\")\n", pos, text );
  pvtcpsend( p, buf, strlen(buf));
  return 0;
}

int qpwSetAxisScale( PARAM *p, int id, int pos, float min, float max, float step)
{
  char buf[256];
  sprintf( buf, "qpw(%d)\n",id);
  pvtcpsend( p, buf, strlen(buf));
  sprintf( buf, "setAxisScale(%d,%f,%f,%f)\n", pos, min, max, step);
  pvtcpsend( p, buf, strlen(buf));
  return 0;
}

// --- QwtScale ----------------------------------------------------------------
int qwtScaleSetTitle(PARAM *p, int id, const char *text)
{
  char buf[1024],*cptr;

  while(1)
  {
    cptr = mystrchr(text,'\n');
    if(cptr != NULL) *cptr = 27; // Escape
    else break;
  }
  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setTitle(\"%s\")\n",text);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtScaleSetTitleColor(PARAM *p, int id, int r, int g, int b)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setTitleColor(%d,%d,%d)\n",r,g,b);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtScaleSetTitleFont(PARAM *p, int id, const char *family, int pointsize, int bold, int italic, int underline, int strikeout)
{
  char buf[1024];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setTitleFont(%d,%d,%d,%d,%d,\"%s\")\n",
             pointsize,
             bold,
             italic,
             underline,
             strikeout,
             family);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtScaleSetTitleAlignment(PARAM *p, int id, int flags)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setTitleAlignment(%d)\n",flags);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtScaleSetBorderDist(PARAM *p, int id, int start, int end)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setBorderDist(%d,%d)\n",start,end);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtScaleSetBaselineDist(PARAM *p, int id, int bd)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setBaselineDist(%d)\n",bd);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtScaleSetScaleDiv(PARAM *p, int id, float lBound, float hBound, int maxMaj, int maxMin, int log, float step, int ascend)
{
  char buf[200];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setScaleDiv(%f,%f,%d,%d,%d,%f,%d)\n",
                           lBound,
                           hBound,
                           maxMaj,
                           maxMin,
                           log,
                           step,
                           ascend);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

/*
int qwtScaleSetScaleDraw(PARAM *p, int id)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setScaleDraw()\n");
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}
*/

int qwtScaleSetLabelFormat(PARAM *p, int id, int f, int prec, int fieldWidth)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setLabelFormat(%d,%d,%d)\n",f,prec,fieldWidth);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtScaleSetLabelAlignment(PARAM *p, int id, int align)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setLabelAlignment(%d)\n",align);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtScaleSetLabelRotation(PARAM *p, int id, float rotation)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setLabelRotation(%f)\n",rotation);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtScaleSetPosition(PARAM *p, int id, int position)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setPosition(%d)\n",position);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

// --- QwtThermo ----------------------------------------------------------------
int qwtThermoSetScale(PARAM *p, int id, float min, float max, float step, int logarithmic)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setScale(%lf,%lf,%lf,%d)\n",min,max,step,logarithmic);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetOrientation(PARAM *p, int id, int orientation, int position)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setOrientation(%d,%d)\n",orientation,position);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetBorderWidth(PARAM *p, int id, int width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setBorderWidth(%d)\n",width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetFillColor(PARAM *p, int id, int r, int g, int b)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setFillColor(%d,%d,%d)\n",r,g,b);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetAlarmColor(PARAM *p, int id, int r, int g, int b)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setAlarmColor(%d,%d,%d)\n",r,g,b);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetAlarmLevel(PARAM *p, int id, float level)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setAlarmLevel(%f)\n",level);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetAlarmEnabled(PARAM *p, int id, int tf)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setAlarmEnabled(%d)\n",tf);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetPipeWidth(PARAM *p, int id, int width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setPipeWidth(%d)\n",width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetRange(PARAM *p, int id, float vmin, float vmax)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setRange(%f,%f)\n",vmin,vmax);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetMargin(PARAM *p, int id, int margin)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMargin(%d)\n",margin);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtThermoSetValue(PARAM *p, int id, float value)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setValue(%f)\n",value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

// --- QwtKnob ----------------------------------------------------------------
int qwtKnobSetScale(PARAM *p, int id, float min, float max, float step, int logarithmic)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setScale(%lf,%lf,%lf,%d)\n",min,max,step,logarithmic);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtKnobSetMass(PARAM *p, int id, float mass)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMass(%f)\n",mass);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtKnobSetOrientation(PARAM *p, int id, int o)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setOrientation(%o)\n",o);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtKnobSetReadOnly(PARAM *p, int id, int rdonly)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setReadOnly(%o)\n",rdonly);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtKnobSetKnobWidth(PARAM *p, int id, int width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setValue(%d)\n",width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtKnobSetTotalAngle(PARAM *p, int id, float angle)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setTotalAngle(%f)\n",angle);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtKnobSetBorderWidth(PARAM *p, int id, int width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setBorderWidth(%d)\n",width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtKnobSetSymbol(PARAM *p, int id, int symbol)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setSymbol(%d)\n",symbol);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtKnobSetValue(PARAM *p, int id, float value)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setValue(%f)\n",value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

// --- QwtCounter ----------------------------------------------------------------
int qwtCounterSetStep(PARAM *p, int id, float step)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setStep(%f)\n",step);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCounterSetMinValue(PARAM *p, int id, float value)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMinValue(%f)\n",value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCounterSetMaxValue(PARAM *p, int id, float value)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMaxValue(%f)\n",value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCounterSetStepButton1(PARAM *p, int id, int n)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setStepButton1(%d)\n",n);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCounterSetStepButton2(PARAM *p, int id, int n)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setStepButton2(%d)\n",n);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCounterSetStepButton3(PARAM *p, int id, int n)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setStepButton3(%d)\n",n);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCounterSetNumButtons(PARAM *p, int id, int n)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setNumButtons(%d)\n",n);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCounterSetIncSteps(PARAM *p, int id, int button, int n)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setIncSteps(%d,%d)\n",button,n);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCounterSetValue(PARAM *p, int id, float value)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setValue(%f)\n",value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

// --- QwtWheel ----------------------------------------------------------------
int qwtWheelSetMass(PARAM *p, int id, float mass)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMass(%f)\n",mass);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtWheelSetOrientation(PARAM *p, int id, int o)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setOrientation(%o)\n",o);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtWheelSetReadOnly(PARAM *p, int id, int rdonly)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setReadOnly(%o)\n",rdonly);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtWheelSetTotalAngle(PARAM *p, int id, float angle)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setTotalAngle(%f)\n",angle);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtWheelSetTickCnt(PARAM *p, int id, int cnt)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setTickCnt(%d)\n",cnt);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtWheelSetViewAngle(PARAM *p, int id, float angle)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setViewAngle(%f)\n",angle);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtWheelSetInternalBorder(PARAM *p, int id, int width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setInternalBorder(%d)\n",width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtWheelSetWheelWidth(PARAM *p, int id, int width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setWheelWidth(%d)\n",width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtWheelSetValue(PARAM *p, int id, float value)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setValue(%f)\n",value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

// --- QwtSlider ----------------------------------------------------------------
int qwtSliderSetScale(PARAM *p, int id, float min, float max, float step, int logarithmic)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setScale(%lf,%lf,%lf,%d)\n",min,max,step,logarithmic);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetMass(PARAM *p, int id, float mass)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMass(%f)\n",mass);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetOrientation(PARAM *p, int id, int o)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setOrientation(%o)\n",o);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetReadOnly(PARAM *p, int id, int rdonly)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setReadOnly(%o)\n",rdonly);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetBgStyle(PARAM *p, int id, int style)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setBgStyle(%d)\n",style);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetScalePos(PARAM *p, int id, int pos)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setScalePos(%d)\n",pos);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetThumbLength(PARAM *p, int id, int length)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setThumbLength(%d)\n",length);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetThumbWidth(PARAM *p, int id, int width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setThumbWidth(%d)\n",width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetBorderWidth(PARAM *p, int id, int width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setBorderWidth(%d)\n",width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetMargins(PARAM *p, int id, int x, int y)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMargins(%d,%d)\n",x,y);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtSliderSetValue(PARAM *p, int id, float value)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setValue(%f)\n",value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

// --- QwtCompass ----------------------------------------------------------------
int qwtCompassSetSimpleCompassRose(PARAM *p, int id, int numThorns, int numThornLevels, float width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setSimpleCompassRose(%d,%d,%f)\n",numThorns,numThornLevels,width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

/*
      else if(strncmp(qwtcommand,"setLabelMap(",12) == 0)
      {
        //setLabelMap(const QMap<double, QString> &map);
      }
*/

int qwtCompassSetMass(PARAM *p, int id, float mass)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMass(%f)\n",mass);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetOrientation(PARAM *p, int id, int o)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setOrientation(%o)\n",o);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetReadOnly(PARAM *p, int id, int rdonly)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setReadOnly(%o)\n",rdonly);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetFrameShadow(PARAM *p, int id, int shadow)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setFrameShadow(%d)\n",shadow);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassShowBackground(PARAM *p, int id, int show)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"showBackground(%d)\n",show);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetLineWidth(PARAM *p, int id, int width)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setLineWidth(%d)\n",width);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetMode(PARAM *p, int id, int mode)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setMode(%d)\n",mode);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetWrapping(PARAM *p, int id, int wrap)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setWrapping(%d)\n",wrap);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetScale(PARAM *p, int id, int maxMajIntv, int maxMinIntv, float step)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setScale(%d,%d,%f)\n",maxMajIntv,maxMinIntv,step);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetScaleArc(PARAM *p, int id, float min, float max)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setScaleArc(%f,%f)\n",min,max);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetOrigin(PARAM *p, int id, float o)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setOrigin(%f)\n",o);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

int qwtCompassSetNeedle(PARAM *p, int id, int which, int r1, int g1, int b1, int r2, int g2, int b2, int r3, int g3, int b3)
{
  char buf[200];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setNeedle(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",which,
                            r1,g1,b1,r2,g2,b2,r3,g3,b3);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

/*
      else if(strncmp(qwtcommand,"setNeedle(",10) == 0)
      {
        //wi->setNeedle(QwtDialNeedle *needle);
      }
*/

int qwtCompassSetValue(PARAM *p, int id, float value)
{
  char buf[80];

  sprintf(buf,"qwt(%d)\n",id);
  pvtcpsend(p, buf, strlen(buf));
  sprintf(buf,"setValue(%f)\n",value);
  pvtcpsend(p, buf, strlen(buf));
  return 0;
}

float unit(PARAM *p, float val, int conversion)
{
  float ret;
  if(p->convert_units == 0) return val;
  switch(conversion)
  {
    case MM2INCH:
      ret = val / 25.4f;
      break;
    case INCH2MM:
      ret = val * 25.4f;
      break;
    case CM2FOOT:
      ret = val / 30.48f;
      break;
    case FOOT2CM:
      ret = val * 30.48f;
      break;
    case CM2YARD:
      ret = val / 91.44f;
      break;
    case YARD2CM:
      ret = val * 91.44f;
      break;
    case KM2MILE:
      ret = val / 1.609f;
      break;
    case MILE2KM:
      ret = val * 1.609f;
      break;
    case KM2NAUTICAL_MILE:
      ret = val / 1.852f;
      break;
    case NAUTICAL_MILE2KM:
      ret = val * 1.852f;
      break;

    case QMM2SQINCH:
      ret = val / 645.16f;
      break;
    case SQINCH2QMM:
      ret = val * 645.16f;
      break;
    case QCM2SQFOOT:
      ret = val / 929.0f;
      break;
    case SQFOOT2QCM:
      ret = val * 929.0f;
      break;
    case QM2SQYARD:
      ret = val / 0.8361f;
      break;
    case SQYARD2QM:
      ret = val * 0.8361f;
      break;
    case QM2ACRE:
      ret = val / 4046.0f;
      break;
    case ACRE2QM:
      ret = val * 4046.0f;
      break;
    case QKM2SQMILE:
      ret = val / 2.590f;
      break;
    case SQMILE2QKM:
      ret = val * 2.590f;
      break;

    case ML2TEASPOON:
      ret = val / 4.929f;
      break;
    case TEASPOON2ML:
      ret = val * 4.929f;
      break;
    case ML2TABLESPOON:
      ret = val / 14.79f;
      break;
    case TABLESPOON2ML:
      ret = val * 14.79f;
      break;
    case ML2OUNCE:
      ret = val / 29.57f;
      break;
    case OUNCE2ML:
      ret = val * 29.57f;
      break;
    case L2CUP:
      ret = val / 0.2365f;
      break;
    case CUP2L:
      ret = val * 0.2365f;
      break;
    case L2PINT:
      ret = val / 0.4732f;
      break;
    case PINT2L:
      ret = val * 0.4732f;
      break;
    case L2QUART:
      ret = val / 0.9464f;
      break;
    case QUART2L:
      ret = val * 0.9464f;
      break;
    case L2GALLON:
      ret = val / 3.785f;
      break;
    case GALLON2L:
      ret = val * 3.785f;
      break;

    case GR2OUNCE:
      ret = val / 28.35f;
      break;
    case OUNCE2GR:
      ret = val * 28.35f;
      break;
    case KG2POUND:
      ret = val / 0.4536f;
      break;
    case POUND2KG:
      ret = val * 0.4536f;
      break;
    case T2TON:
      ret = val / 0.9072f;
      break;
    case TON2T:
      ret = val * 0.9072f;
      break;

    case C2FAHRENHEIT:
      ret = (val * 9.0f / 5.0f) + 32.0f;
      break;
    case FAHRENHEIT2C:
      ret = (val - 32.0f) * 5.0f / 9.0f;
      break;

    default:
      ret = val;
  }
  return ret;
}

int textEventType(const char *text)
{
  if     (strncmp(text,"svgPressedLeftButton="  ,21) == 0) return SVG_LEFT_BUTTON_PRESSED;
  else if(strncmp(text,"svgPressedMidButton="   ,20) == 0) return SVG_MIDDLE_BUTTON_PRESSED;
  else if(strncmp(text,"svgPressedRightButton=" ,22) == 0) return SVG_RIGHT_BUTTON_PRESSED;
  else if(strncmp(text,"svgReleasedLeftButton=" ,22) == 0) return SVG_LEFT_BUTTON_RELEASED;
  else if(strncmp(text,"svgReleasedMidButton="  ,21) == 0) return SVG_MIDDLE_BUTTON_RELEASED;
  else if(strncmp(text,"svgReleasedRightButton=",23) == 0) return SVG_RIGHT_BUTTON_RELEASED;
  else if(strncmp(text,"svgBoundsOnElement:"    ,19) == 0) return SVG_BOUNDS_ON_ELEMENT;
  return PLAIN_TEXT_EVENT;
}

const char *svgObjectName(const char *text)
{
  const char *cptr;
  if(strncmp(text,"svg",3) == 0)
  {
    cptr = mystrchr(text,'=');
    if(cptr != NULL)
    {
      cptr++;
      return cptr;
    }
  }
  return null_string;
}

int getSvgBoundsOnElement(const char *text, float *x, float *y, float *width, float *height)
{
  const char *cptr;
  if(strncmp(text,"svgBoundsOnElement:",19) == 0)
  {
    cptr = mystrchr(text,':');
    if(cptr != NULL)
    {
      cptr++;
      sscanf(cptr,"%f,%f,%f,%f",x,y,width,height);
      return 0;
    }
  }
  *x = *y = *width = *height = -1.0f;
  return -1;
}

int pvsystem(const char *command)
{
#ifdef PVWIN32
  int ret;
  STARTUPINFO         si = { sizeof(si)};
  PROCESS_INFORMATION pi;
  char cmd[4096];

  if(strncmp(command,"start",5) == 0 || strncmp(command,"START",5) == 0)
  {
    ExpandEnvironmentStrings(command,cmd,sizeof(cmd)-1);
    ret = system(cmd);
  }
  else
  {
    ExpandEnvironmentStrings(command,cmd,sizeof(cmd)-1);
    ret = (int) CreateProcess( NULL, cmd
                             , NULL, NULL
                             , FALSE, CREATE_NO_WINDOW
                             , NULL, NULL
                             , &si, &pi);
  }
  return ret;
#else
  return system(command);
#endif
}
