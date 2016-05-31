
/*------------------------------------------------------------------------------------------------------------------
 *
 *  LOG FUNCTION:   RAIN_DBGLOG
 *
 * ---------------------------------------------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <execinfo.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef __cplusplus
#include <cxxabi.h>
#endif

#define RAINLILOG_FILENAME    "rainli.log"

#define RAIN_DBGLOG(fmt,arg...)     do{                                     \
    struct timespec    now;    clock_gettime(CLOCK_REALTIME,&now);         \
    struct tm day   = *localtime(&now.tv_sec);                              \
    printf("%02d:%02d:%02d.%03d [%s:%d] "fmt,day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000,__func__,__LINE__,##arg);    \
    rainlog2file("%02d:%02d:%02d.%03d [%s:%d] "fmt,day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000,__func__,__LINE__,##arg);    \
}while(0)

#define RAIN_DBGTRACE(fmt,arg...)   do{                                     \
    struct timespec    now;    clock_gettime(CLOCK_REALTIME,&now);         \
    struct tm day   = *localtime(&now.tv_sec);                              \
    printf("%02d:%02d:%02d.%03d [%s:%d] "fmt,day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000,__func__,__LINE__,##arg);    \
    Backtrace("%02d:%02d:%02d.%03d [%s:%d] "fmt,day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000,__func__,__LINE__,##arg);    \
}while(0)

static void rainlog2file(const char *fmt,...)
{
    int fd = openat(AT_FDCWD, RAINLILOG_FILENAME, O_CREAT|O_APPEND| O_WRONLY,0666);
    if( fd != -1 ){
        va_list arg_ptr;
        va_start(arg_ptr, fmt);
        vdprintf(fd,fmt,arg_ptr);
        //vprintf(fmt,arg_ptr);
        va_end(arg_ptr);
        close(fd);
    }
}

static void Backtrace(const char *fmt,...)
{
    int skip            = 1;
    void *callstack[128];
    const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
    int nFrames         = backtrace(callstack, nMaxFrames);
    char **symbols      = backtrace_symbols(callstack, nFrames);
    va_list arg_ptr;
    int i = skip;

    int fd = openat(AT_FDCWD, RAINLILOG_FILENAME, O_CREAT|O_APPEND| O_WRONLY,0666);
    if( fd == -1 ){
        return ;
    }    

    va_start(arg_ptr, fmt);
    vdprintf(fd,fmt,arg_ptr);
    va_end(arg_ptr);

    dprintf(fd,"backtrace:\n");

    for (; i < nFrames; i++) {
#if 0       //  simple mode,only print address
        dprintf(fd,"\t<%s>\t", symbols[i]);
#else       //  complex mode, try to resolve address name,need -ldl link option
    Dl_info info;

        if (dladdr(callstack[i], &info) && info.dli_sname) {
            char *demangled = NULL;
            int status = -1;
#ifdef __cplusplus
            if (info.dli_sname[0] == '_')
                demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
#endif
            dprintf(fd, "%-3d %*p %s + %zd\n",
                    i, (int)(2 + sizeof(void*) * 2), callstack[i],
                    status == 0 ? demangled :
                    info.dli_sname == 0 ? symbols[i] : info.dli_sname,
                    (char *)callstack[i] - (char *)info.dli_saddr);
            if( demangled )
                free(demangled);
#endif
        } else {
            dprintf(fd, "%-3d %*p %s\n",
                    i, (int)(2 + sizeof(void*) * 2), callstack[i], symbols[i]);
        }
    }
    free(symbols);
    if (nFrames == nMaxFrames)
        dprintf(fd, "[truncated]\n");
    else
        write(fd,"\n",1);
    close(fd);
}

#ifdef __JAVA__
void trace(){
    //Thread.currentThread().getStackTrace()
    StringWriter sw = new StringWriter();
    new Throwable("").printStackTrace(new PrintWriter(sw));
    String stackTrace = sw.toString();
}
#endif


int tsk_create(void *(*start_routine) (void *), void *arg)
{
    pthread_t slave_tid;
    int ret = pthread_create(&slave_tid, NULL, start_routine, arg);
    pthread_detach(slave_tid);
    return ret;
}

static inline int file_recreate(const char *fname)
{
    int fd;
    unlinkat(AT_FDCWD,fname,0);
    fd  = openat(AT_FDCWD,fname,O_CREAT,0666);
    if( fd != -1 )
        close(fd);
    return fd != -1 ? 0: -1;
}

/*------------------------------------------------------------------------------------------------------------------
 *
 *  INOTIFY EXAMPLE, provide inotifyapi_xxx
 *
 * ---------------------------------------------------------------------------------------------------------------*/
#include <unistd.h>  
#include <sys/inotify.h>  
#include <stdio.h>  
#include <error.h>  
#include <errno.h>  
#include <string.h>  
#include <poll.h>

#define ERROR(text) error(1, errno, "%s", text)  

struct EventMask {  
    int        flag;  
    const char *name;  

};  
struct EventMask event_masks[] = {  
    {IN_ACCESS        , "IN_ACCESS"}        ,    
    {IN_ATTRIB        , "IN_ATTRIB"}        ,    
    {IN_CLOSE_WRITE   , "IN_CLOSE_WRITE"}   ,    
    {IN_CLOSE_NOWRITE , "IN_CLOSE_NOWRITE"} ,    
    {IN_CREATE        , "IN_CREATE"}        ,    
    {IN_DELETE        , "IN_DELETE"}        ,    
    {IN_DELETE_SELF   , "IN_DELETE_SELF"}   ,    
    {IN_MODIFY        , "IN_MODIFY"}        ,    
    {IN_MOVE_SELF     , "IN_MOVE_SELF"}     ,    
    {IN_MOVED_FROM    , "IN_MOVED_FROM"}    ,    
    {IN_MOVED_TO      , "IN_MOVED_TO"}      ,    
    {IN_OPEN          , "IN_OPEN"}          ,    

    {IN_DONT_FOLLOW   , "IN_DONT_FOLLOW"}   ,    
    {IN_EXCL_UNLINK   , "IN_EXCL_UNLINK"}   ,    
    {IN_MASK_ADD      , "IN_MASK_ADD"}      ,    
    {IN_ONESHOT       , "IN_ONESHOT"}       ,    
    {IN_ONLYDIR       , "IN_ONLYDIR"}       ,    

    {IN_IGNORED       , "IN_IGNORED"}       ,    
    {IN_ISDIR         , "IN_ISDIR"}         ,    
    {IN_Q_OVERFLOW    , "IN_Q_OVERFLOW"}    ,    
    {IN_UNMOUNT       , "IN_UNMOUNT"}       ,    
};  





#define INOTIFYAPI_SUCC                 0
#define INOTIFYAPI_TIMEOUT              -1      
#define INOTIFYAPI_SHORTREC             -2  

#define INOTIFYAPI_INITFAIL             -10
#define INOTIFYAPI_WATCHFAIL            -11
#define INOTIFYAPI_WAITFAIL             -12

struct inotify_handle{
    int                     fd;
    int                     wd;
    struct inotify_event    *ev;
    char                    cache[1024];
    int                     cachelen;
    int                     offset;
};

static int inotifyapi_init(struct inotify_handle *handle,const char *target,int mode)
{
    memset(handle,0,sizeof(*handle));
    handle->wd  = -1;
    handle->fd  = inotify_init();

    if( handle->fd == -1 ){
        RAIN_DBGLOG("monitor init fail, errno:%d\n",errno);
        return INOTIFYAPI_INITFAIL;
    }

    handle->wd = inotify_add_watch(handle->fd, target, mode);  
    if( handle->wd == -1 ){
        RAIN_DBGLOG("inotify_add_watch %s fail, errno:%d\n",target, errno);
        close(handle->fd);
        handle->fd  = -1;
        return INOTIFYAPI_WATCHFAIL;
    }

    return 0;
}

static void inotifyapi_release(struct inotify_handle *handle)
{
    inotify_rm_watch(handle->fd,handle->wd);
    close(handle->fd);
    handle->fd  = -1;
    handle->wd  = -1;
}

static int inotifyapi_readrec(struct inotify_handle *handle,int timeout_ms)
{
    handle->ev          = NULL;
    while( 1 ){
        if( handle->offset + sizeof(struct inotify_event) > handle->cachelen ){
            struct pollfd pfd = { handle->fd, POLLIN, 0 };
            int ret = poll(&pfd, 1, timeout_ms);
            if( ret < 0 ){
                return INOTIFYAPI_WAITFAIL;
            }
            else if( ret == 0 ){
                return INOTIFYAPI_TIMEOUT;
            }

            handle->offset          = 0;
            handle->cachelen        = read(handle->fd,handle->cache,sizeof(handle->cache));
            if (handle->cachelen < (int)sizeof(struct inotify_event)) {
                handle->cachelen    = 0;

                if (errno == EINTR)
                    continue;

                RAIN_DBGLOG("***** ERROR! inotifyapi_getrec() got a short event!");
                return INOTIFYAPI_SHORTREC;
            }
        }

        if( handle->offset + sizeof(struct inotify_event) > handle->cachelen ){
            return INOTIFYAPI_TIMEOUT;
        }

        handle->ev  = (struct inotify_event *)(handle->cache + handle->offset);
        handle->offset  += sizeof(struct inotify_event) + handle->ev->len;
        break;
    }
    RAIN_DBGTRACE("get a rec:");
    return 0;
}

int main(int argc, char *argv[])  
{  
    const char              *target  = (argc ==1) ? "." : argv[1];  
    struct inotify_handle   handle;

    int ret = inotifyapi_init(&handle, target,  IN_DELETE | IN_DELETE_SELF /*IN_ALL_EVENTS */);
    if( ret < 0 ){
        RAIN_DBGLOG("monitor init fail, errno %d:%d\n",ret,errno);
        return 1;
    }

    /* event:inotify_event -> name:char[event.len] */  
    while (1) {  
        ret = inotifyapi_readrec(&handle,0xffff);
        if( ret < 0 ){
            if( ret == INOTIFYAPI_TIMEOUT )
                continue;
            RAIN_DBGLOG("get inotify rec error:%d<%d>\n",ret,errno);
            break;
        }

        struct inotify_event    *event = handle.ev;

        if ( !event->len ) {  
            sprintf(event->name, "FD: %d\n", event->wd);  
        }  

        printf("<%s:%d/0x%08x>:\n",event->name,event->len, event->mask);

        /* 閺勫墽銇歟vent閻ㄥ埓ask閻ㄥ嫬鎯堟稊?*/  
        for (int i=0; i<sizeof(event_masks)/sizeof(struct EventMask); ++i) {  
            if (event->mask & event_masks[i].flag) {  
                printf("\t%s\n", event_masks[i].name);  
            }  
        }  
    }  

    inotifyapi_release(&handle);
    return 0;  
}
