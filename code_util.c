#define TEST_RAIN_LOG
#undef TEST_OSAPI
#undef TEST_INOTIFY
#undef TEST_STLQUEUE_API
#undef TEST_BLOCKQUEUE
#undef TEST_NONBLOCK_SOCKET
#define TEST_NETNOTIFY

#ifdef TEST_RAIN_LOG
#define TEST_SHOW_SCREEN

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
#include <stdlib.h>
#include <pthread.h>
#include <sys/syscall.h>  

#ifdef ANDROID
#include <unwind.h>
#else
#include <execinfo.h>

#ifdef __cplusplus
#include <execinfo.h>
#include <cxxabi.h>
#endif

#endif

#define RAINLILOG_FILENAME    "rainli.log"

#define RAIN_DBGLOG(fmt,arg...)         RainLog("[%s:%d] " fmt,__func__,__LINE__,##arg)
void RainLog(const char *fmt,...) __attribute__ ((format (gnu_printf, 1, 2)));

void RainLog(const char *fmt,...)
{
    int fd = openat(AT_FDCWD, RAINLILOG_FILENAME, O_CREAT|O_APPEND| O_WRONLY,0666);
    if( fd != -1 ){
        va_list arg_ptr;

        struct timespec    now;
        struct tm day;

        va_start(arg_ptr, fmt);

        clock_gettime(CLOCK_REALTIME,&now);
        day   = *localtime(&now.tv_sec);
#ifdef TEST_SHOW_SCREEN
        printf("%02d:%02d:%02d.%03ld %ld ",day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000,syscall(__NR_gettid));
        vprintf(fmt,arg_ptr);
#else
        dprintf(fd,"%02d:%02d:%02d.%03ld %ld ",day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000,syscall(__NR_gettid));
        vdprintf(fd,fmt,arg_ptr);
#endif
        va_end(arg_ptr);
        close(fd);
    }
}


#define RAIN_DBGTRACE(fmt,arg...)       RainTrace("[%s:%d] "fmt,__func__,__LINE__,##arg)
void RainTrace(const char *fmt,...) __attribute__ ((format (gnu_printf, 1, 2)));
#ifdef ANDROID
struct BacktraceState
{
    void** current;
    void** end;
};

static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg)
{
    BacktraceState* state = static_cast<BacktraceState*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = reinterpret_cast<void*>(pc);
        }
    }
    return _URC_NO_REASON;
}



size_t captureBacktrace(void** buffer, size_t max)
{
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(unwindCallback, &state);

    return state.current - buffer;
}

#endif

void RainTrace(const char *fmt,...)
{
    void *callstack[128];
    const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
    va_list arg_ptr;
    int i = 1;          //  up one call level
    int nFrames;
    char **symbols  = NULL;
    struct timespec    now;
    struct tm day;

    int fd = openat(AT_FDCWD, RAINLILOG_FILENAME, O_CREAT|O_APPEND| O_WRONLY,0666);
    if( fd == -1 ){
        return ;
    }

    va_start(arg_ptr, fmt);

    clock_gettime(CLOCK_REALTIME,&now);
    day   = *localtime(&now.tv_sec);
#ifdef TEST_SHOW_SCREEN
        printf("%02d:%02d:%02d.%03d ",day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000);
        vprintf(fmt,arg_ptr);
#else
        dprintf(fd,"%02d:%02d:%02d.%03d ",day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000);
        vdprintf(fd,fmt,arg_ptr);
#endif
    va_end(arg_ptr);


    dprintf(fd,"backtrace:\n");
#ifdef ANDROID
    nFrames         = captureBacktrace(callstack, nMaxFrames);
#else
    nFrames         = backtrace(callstack, nMaxFrames);
    symbols         = backtrace_symbols(callstack, nFrames);
#endif

    for (; i < nFrames; i++) {
        /*  simple mode,only print address
            dprintf(fd,"\t<%s>\t", symbols[i]);
            */
        //  complex mode, try to resolve address name,need -ldl link option
        Dl_info info;
        char *demangled     = NULL;
        const char* symbol  = symbols ? symbols[i] : NULL;

        if (!dladdr(callstack[i], &info) || !info.dli_sname)
        {
            dprintf(fd, "%-3d %*p %s\n",
                    i, (int)(2 + sizeof(void*) * 2), callstack[i], symbol);
            continue;
        }

        symbol      = info.dli_sname;
#if ( defined __cplusplus)&& (!defined ANDROID)
        if (info.dli_sname[0] == '_'){
            int status          = -1;
            demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
            //  android version, __cxxabiv1::__cxa_demangle(info.dli_sname, NULL, 0, &status);
            if( status == 0 ){
                symbol  = demangled;
            }
        }
#endif

        dprintf(fd, "%-3d %*p %s + %zd\n",
                i, (int)(2 + sizeof(void*) * 2), callstack[i],
                symbol,
                (char *)callstack[i] - (char *)info.dli_saddr);
        if( demangled )
            free(demangled);
    }

#ifndef ANDROID
    free(symbols);
#endif
    if (nFrames == nMaxFrames)
        dprintf(fd, "[truncated]\n");
    else
        write(fd,"\n",1);
    close(fd);
}
















#ifdef __JAVA__
import java.io.FileWriter;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.text.SimpleDateFormat;
import java.util.Date;


void log(String val){
    try{

        SimpleDateFormat dateFormat = new SimpleDateFormat("HH:mm:ss.SSS");
        String when = dateFormat.format(new Date());
        FileWriter fw = new FileWriter("rainli.log",true);
        fw.write( when + " " + val + "\n");
        fw.close();
    }
    catch( Exception e){
    }
}
void logbt(String val){
    try{
        StringWriter errors = new StringWriter();
        new Throwable().printStackTrace(new PrintWriter(errors));
        log( val + " BACKTRACE:\n" + errors);
    }
    catch( Exception e){
    }
}

#endif

#endif












#ifdef TEST_OSAPI
int task_create(void *(*start_routine) (void *), void *arg)
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
#endif

#ifdef TEST_INOTIFY
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

#define ERROR   RAIN_DBGLOG
#define INFO    RAIN_DBGLOG

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

typedef int (*INOTIFY_CALLBACK)(const struct inotify_event *ev,void *param,int *stop);

struct inotify_handle{
    int                     fd;
    int                     wd;
    unsigned int            evmask;
    struct inotify_event    *ev;
    char                    cache[1024];
    int                     cachelen;
    int                     offset;
};

static int inotifyapi_init(struct inotify_handle *handle,const char *target,int evmask)
{
    memset(handle,0,sizeof(*handle));
    handle->wd  = -1;
    handle->fd  = inotify_init();

    if( handle->fd == -1 ){
        RAIN_DBGLOG("monitor init fail, errno:%d\n",errno);
        return INOTIFYAPI_INITFAIL;
    }

    handle->wd = inotify_add_watch(handle->fd, target, evmask);
    if( handle->wd == -1 ){
        RAIN_DBGLOG("inotify_add_watch %s fail, errno:%d\n",target, errno);
        close(handle->fd);
        handle->fd  = -1;
        return INOTIFYAPI_WATCHFAIL;
    }

    handle->evmask      = evmask;
    return 0;
}

static void inotifyapi_release(struct inotify_handle *handle)
{
    inotify_rm_watch(handle->fd,handle->wd);
    close(handle->fd);
    handle->fd  = -1;
    handle->wd  = -1;
}

int inotifyapi_wait(struct inotify_handle *handle,int timeout_ms,INOTIFY_CALLBACK callback,void *param)
{
    int isstop          = 0;
    int ret             = 0;
    struct timespec     ts_start, ts_end;
    int     diff;
    handle->ev          = NULL;
    while( isstop == 0 ){
        if( handle->offset + sizeof(struct inotify_event) > handle->cachelen ){
            //  don't recaculate wait time
            struct pollfd pfd = { handle->fd, POLLIN, 0 };
            if( timeout_ms > 0 ){
                clock_gettime(CLOCK_MONOTONIC,&ts_start);
            }

            ret = poll(&pfd, 1, timeout_ms);
            if( ret < 0 ){
                ERROR("***** ERROR! inotifyapi_wait() poll fail:%d!",errno);
                return INOTIFYAPI_WAITFAIL;
            }
            else if( ret == 0 ){
                return INOTIFYAPI_TIMEOUT;
            }

            if( timeout_ms > 0 ){
                clock_gettime(CLOCK_MONOTONIC,&ts_end);
                diff    = (ts_end.tv_sec - ts_start.tv_sec) * 1000 + (ts_end.tv_nsec - ts_start.tv_nsec)/1000000;
                if( diff > 0 ){
                    timeout_ms  -= diff;
                }
                else if( diff < 0 ){
                    INFO("***** WARNING! inotifyapi_wait() diff time err(end: %d,%d, start:%d,%d)!",
                            ts_end.tv_sec,ts_end.tv_nsec,ts_start.tv_sec,ts_start.tv_nsec);
                }
            }

            handle->offset          = 0;
            handle->cachelen        = read(handle->fd,handle->cache,sizeof(handle->cache));
            if (handle->cachelen < (int)sizeof(struct inotify_event)) {
                handle->cachelen    = 0;

                if (errno == EINTR)
                    continue;

                ERROR("***** ERROR! inotifyapi_wait() got a short event!");
                return INOTIFYAPI_SHORTREC;
            }
        }

        handle->ev      = (struct inotify_event *)(handle->cache + handle->offset);
        handle->offset  += sizeof(struct inotify_event) + handle->ev->len;

        if( handle->ev->mask & handle->evmask ){
            ret     = 0;
            isstop  = 1;
            if( callback ){
                ret = callback(handle->ev,param,&isstop);
            }
        }
    }

    return ret;
}

int main(int argc, char *argv[])
{
    const char              *target  = (argc ==1) ? "." : argv[1];
    struct inotify_handle   handle;

    int ret = inotifyapi_init(&handle, target,  IN_CREATE /*IN_ALL_EVENTS */);
    if( ret < 0 ){
        RAIN_DBGLOG("monitor init fail, errno %d:%d\n",ret,errno);
        return 1;
    }

    RAIN_DBGTRACE("start\n");

    /* event:inotify_event -> name:char[event.len] */
    while (1) {
        ret = inotifyapi_wait(&handle,10,NULL,NULL);
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

        RAIN_DBGLOG("<%s:%d/0x%08x>:\n",event->name,event->len, event->mask);

        for (int i=0; i<sizeof(event_masks)/sizeof(struct EventMask); ++i) {
            if (event->mask & event_masks[i].flag) {
                printf("\t%s\n", event_masks[i].name);
            }
        }
    }

    inotifyapi_release(&handle);
    return 0;
}
#endif




#ifdef TEST_STLQUEUE_API
/*------------------------------------------------------------------------------------------------------------------
 *
 *  LIST EARASE SAMPLE:   use remove_if
 *
 * ---------------------------------------------------------------------------------------------------------------*/
#ifdef __cplusplus
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <list>
#include <functional>
#include <algorithm>

struct SockManagerInfo{
    int sd;
};

typedef void (*DELAY_ACK_CALLBACK)(SockManagerInfo *info,void *param);

class NetworkAckCache{
    struct DelayAckItem{
	SockManagerInfo         *info;
	DELAY_ACK_CALLBACK      callback;
	uint8_t*                data;
	int                     datalen;
    };

    struct remove_a_match : public std::unary_function<DelayAckItem &, bool>
    {
	remove_a_match(const SockManagerInfo *val) : val_(val) {};
	bool operator()(DelayAckItem &victim) const {
            if( victim.info == val_ ){
                printf("will erease item:%p %s[%d]\n",victim.info,(char *)victim.data,victim.datalen);
                delete[] victim.data;
                victim.data    = NULL;
                return true;
            }
            else{
                printf("skip item:%p %s[%d]\n",victim.info,(char *)victim.data,victim.datalen);
                return false;
            }
        }
	private:
	const SockManagerInfo *val_;
    };


    std::list<DelayAckItem>      items_list;
    public:
    ~NetworkAckCache(){
	for( std::list<DelayAckItem>::iterator iter = items_list.begin();
		iter != items_list.end(); iter ++ ){
	    delete[] iter->data;
	    iter->data = NULL;
	}
    }

    bool push(SockManagerInfo *info,DELAY_ACK_CALLBACK callback,const uint8_t*data, int datalen){
        DelayAckItem    item    = {info,callback,NULL,datalen};
        if( datalen > 0 ){
            item.data              = new uint8_t[datalen];
            if( !item.data )
                return false;

            memcpy(item.data,data,datalen);
        }

        items_list.push_back(item);
        return true;
    }

    bool clear(SockManagerInfo *info){
	items_list.erase(std::remove_if(items_list.begin(), items_list.end(), remove_a_match(info) ),items_list.end());
    }

    void dump(){
        int i   = 0;
        for( std::list<DelayAckItem>::iterator iter = items_list.begin();
                iter != items_list.end(); iter ++,i++ ){
            printf("%d:\t%p %s[%d]\n",i,iter->info,iter->data,iter->datalen);
        }
    }
};

int main()
{
    SockManagerInfo a[10];
    NetworkAckCache cache;
    for( int i = 0; i < 10; i ++ ){
        char    buf[20];
        sprintf(buf,"it_is %d",i);
        cache.push(a+i,NULL,(uint8_t *)buf,20);
    }

    cache.clear(a+1);
    printf("=================   after clear 1,ret:\n");
    cache.dump();

    cache.clear(a+0);
    printf("=================   after clear 0,ret:\n");
    cache.dump();

    cache.clear(a+5);
    printf("=================   after clear 5,ret:\n");
    cache.dump();

    cache.clear(a+9);
    printf("=================   after clear 9,ret:\n");
    cache.dump();

    return 0;
}
#endif


#endif



/*------------------------------------------------------------------------------------------------------------------
 *
 *  BLOCK QUEUE :   RemotedBlockQueue
 *
 * ---------------------------------------------------------------------------------------------------------------*/
#ifdef TEST_BLOCKQUEUE

#include <pthread.h>
#include <list>

#define BUFFER_SIZE 8

typedef void (*BLOCKQUEUE_CLEAR_FUNC)(void *);

class RemotedBlockQueue{
    pthread_mutex_t         m_locker;
    pthread_cond_t          m_notEmpty;
    pthread_cond_t          m_notFull;

    volatile bool           m_stop;
    const unsigned int      m_maxsz;
    std::list<void *>       m_items;
    BLOCKQUEUE_CLEAR_FUNC   m_clearfunc;

    bool push(void* data,bool wait,bool isfront){
	if( m_stop )
	    return false;

	pthread_mutex_lock(&m_locker);
	while (isfull()){
	    if( !wait || m_stop ){
		pthread_mutex_unlock(&m_locker);
		return false;
	    }
	    pthread_cond_wait(&m_notFull, &m_locker);
	} 

	if( isfront ){
	    m_items.push_front(data);
	}
	else{
	    m_items.push_back(data);
	}

	pthread_cond_signal(&m_notEmpty);
	pthread_mutex_unlock(&m_locker);
	return true;
    }

    bool pop(void** out,bool wait,bool isfront){
	if( m_stop || out == NULL )
	    return false;

	*out        = NULL;
	pthread_mutex_lock(&m_locker);
	while (isempty()) {
	    if( !wait || m_stop ){
		pthread_mutex_unlock(&m_locker);
		return false;
	    }

	    pthread_cond_wait(&m_notEmpty, &m_locker);
	}

	if( isfront ) {
	    *out    = m_items.front();
	    m_items.pop_front();
	}
	else {
	    *out    = m_items.back();
	    m_items.pop_back();
	}

	pthread_cond_signal(&m_notFull); 
	pthread_mutex_unlock(&m_locker);
	return true;
    }

    public:
    RemotedBlockQueue(int maxsz,BLOCKQUEUE_CLEAR_FUNC clearfunc):m_maxsz(maxsz) {
	m_stop          = false;
	m_locker	= PTHREAD_MUTEX_INITIALIZER;
	m_notEmpty 	= PTHREAD_COND_INITIALIZER;
	m_notFull 	= PTHREAD_COND_INITIALIZER;
	m_clearfunc 	= clearfunc;
    }
    ~RemotedBlockQueue(){
	stop();
    }

    bool isfull(){
	return m_items.size() >= m_maxsz;
    }

    bool isempty(){
	return m_items.empty();
    }

    bool isstop(){
	return m_stop;
    }


    bool push_front(void* data,bool wait = true){
	return push(data,wait,true);
    }
    bool pop_front(void** data,bool wait = true){
	return pop(data,wait,true);
    }

    bool push_back(void* data,bool wait = true){
	return push(data,wait,false);
    }

    bool pop_back(void** data,bool wait = true){
	return pop(data,wait,false);
    }

    void stop(){
	pthread_mutex_lock(&m_locker);
	m_stop  = true;
	if( m_clearfunc ){
	    for( std::list<void *>::iterator iter = m_items.begin(); iter != m_items.end(); iter ++ ){
		m_clearfunc(*iter);
	    }
	}
	m_items.clear();

	pthread_cond_signal(&m_notEmpty); 
	pthread_cond_signal(&m_notFull); 
	pthread_mutex_unlock(&m_locker);
    }
    void enable(){
	pthread_mutex_lock(&m_locker);
	m_stop  = false;
	pthread_mutex_unlock(&m_locker);
    }
};

#if 1   //  TEST FUNC
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BLKQUEUE_SZ         8
#define END_FLAG (-1)

RemotedBlockQueue	s_BlkQueue(BLKQUEUE_SZ,NULL);

void* ProducerThread(void* data)
{
    long i;
    usleep(1000);
    RAIN_DBGLOG("producer begin\n");
    for (i = 0; i < 16; ++i)
    {
	RAIN_DBGLOG("producer: %d\n", i);
	if( false == s_BlkQueue.push_back((void *)i,false) ){
	    RAIN_DBGLOG("producer fast push %d fail, will try again\n",i);
	    if( false == s_BlkQueue.push_back((void *)i) ){
		RAIN_DBGLOG(" error: producer slow push %d fail,stop:%d\n",i,s_BlkQueue.isstop());
                return NULL;
	    }
	}
    }
    s_BlkQueue.push_back((void *)END_FLAG);
    return NULL;
}

void* ConsumerThread(void* data)
{
    void *item = NULL;
    RAIN_DBGLOG("consumer begin\n");
    if( s_BlkQueue.pop_front(&item,false) == false ){
	RAIN_DBGLOG("consumer first pop no data\n");
    }
    else{
	RAIN_DBGLOG("consumer first: %p\n", item);
    }

    while (1)
    {
	usleep(1000000);
	if( !s_BlkQueue.pop_front(&item) ){
	    RAIN_DBGLOG("consumer pop fail,stop:%d\n",s_BlkQueue.isstop());
            break;
	}

	if ((void *)END_FLAG == item)
	    break;
	RAIN_DBGLOG("consumer: %p\n", item);
    }
    return (NULL);
}

int main(int argc, char* argv[])
{
    pthread_t producer;
    pthread_t consumer;
    void * result;

    RAIN_DBGLOG(" test producer/consumer\n");
    pthread_create(&producer, NULL, &ProducerThread, NULL);
    pthread_create(&consumer, NULL, &ConsumerThread, NULL);

    pthread_join(producer, &result);
    pthread_join(consumer, &result);

    RAIN_DBGLOG("test stop producer\n");
    for( int i = 0; i < BLKQUEUE_SZ ; i ++ ){
        s_BlkQueue.push_back(result);
    }
    pthread_create(&producer, NULL, &ProducerThread, NULL);
    usleep(2000);
    s_BlkQueue.stop();
    pthread_join(producer, &result);

    s_BlkQueue.enable();
    s_BlkQueue.push_back((void *)0x1001);

    RAIN_DBGLOG("test stop consumer\n");
    pthread_create(&consumer, NULL, &ConsumerThread, NULL);
    usleep(2000);
    s_BlkQueue.stop();
    pthread_join(consumer, &result);

    RAIN_DBGLOG("finish all test\n");
    exit(EXIT_SUCCESS);
}
#endif

#endif


#ifdef TEST_NONBLOCK_SOCKET
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <arpa/inet.h>

#define ERROR_STEP_SOCK_CREAT       0x010000
#define ERROR_STEP_CONN_FAIL        0x020000
#define ERROR_STEP_POLL_FAIL        0x030000
#define ERROR_STEP_POLL_TIMEOUT     0x040000
#define ERROR_STEP_OPT_ERR          0x050000
#define ERROR_STEP_CONN_ERR         0x060000
#define ERROR_STEP_GETFLAG_ERR      0x070000
#define ERROR_STEP_SETFLAG_ERR      0x080000
static const char *step2str(int step){
    switch( step ){
        case ERROR_STEP_SOCK_CREAT:     return "creat";
        case ERROR_STEP_CONN_FAIL:      return "conn";
        case ERROR_STEP_POLL_FAIL:      return "poll";
        case ERROR_STEP_POLL_TIMEOUT:   return "poll_timeout";
        case ERROR_STEP_OPT_ERR:        return "getsockopt";
        case ERROR_STEP_CONN_ERR:       return "conn2";
        case ERROR_STEP_GETFLAG_ERR:    return "fcntl";
        case ERROR_STEP_SETFLAG_ERR:    return "fcntl_set";
        default:                        return "unknown";
    }
}

int connectwithtimeout(int sd,const char *addr,int port,int timeout_ms)
{
    int                 ret         = 0;
    struct sockaddr_in  servaddr;
    struct pollfd       pfd         = { sd, POLLIN, 0 };
    int                 err         = 0;
    socklen_t           len         = sizeof (int);
    int                 flags       = 0;

    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family             = AF_INET;
    servaddr.sin_port               = htons(port);
    servaddr.sin_addr.s_addr        = inet_addr(addr);

    if( sd == -1 ){
        return ERROR_STEP_SOCK_CREAT | errno;
    }

    ret  = connect(sd,(const struct sockaddr*)&servaddr,sizeof(servaddr));
    if( ret == 0 ){
        goto setblock;
    }

    if( errno != EINPROGRESS ){
        return ERROR_STEP_CONN_FAIL | errno;
    }

    ret = poll(&pfd, 1, timeout_ms);
    if( ret == 0 ){
        return ERROR_STEP_POLL_TIMEOUT | errno;
    }
    else if( ret == -1 ){
        return ERROR_STEP_POLL_FAIL | errno;
    }

    ret = getsockopt (sd, SOL_SOCKET, SO_ERROR, &err,&len);
    if( ret == -1 ){
        return ERROR_STEP_OPT_ERR | errno;
    }

    if( err != 0 ){
        return ERROR_STEP_CONN_ERR | err;
    }


setblock:
    flags = fcntl(sd, F_GETFL, 0);
    if( flags < 0 ){
        return ERROR_STEP_SETFLAG_ERR | errno;
    }

    if( fcntl(sd, F_SETFL, (flags&~O_NONBLOCK)) !=0 ){
        return ERROR_STEP_SETFLAG_ERR | errno;
    }

    return 0;
}

int testconnect(const char *addr,int port,int timeout_ms)
{
    int ret = -1;
    RAIN_DBGLOG("begin test connect:%s:%d with timeout %d ms\n",addr,port,timeout_ms);
    while(ret != 0 ){
        int sd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if( sd == -1 ){
            RAIN_DBGLOG("create socket fail: %d\n",errno);
        }
        else{
            int ret = connectwithtimeout(sd,addr,port,timeout_ms);
            close(sd);

            if( ret == 0 ){
                RAIN_DBGLOG("connect succ\n");
            }
            else{
                RAIN_DBGLOG("connect fail, error step :0x%x/%s errno:%d/%s\n",ret & 0xffff0000,step2str(ret & 0xffff0000), ret & 0xffff,strerror(ret & 0xffff));
            }
        }
    }
    RAIN_DBGLOG("finish test\n");
}

int main(){
    testconnect("10.206.139.237",22,300);
}
#endif








#ifdef TEST_NETNOTIFY
#include    <stdio.h>
#include    <string.h>
#include    <unistd.h>
#include    <sys/types.h>
#include    <sys/socket.h>
#include    <netinet/in.h>
#include    <linux/rtnetlink.h>
#include    <linux/if_arp.h>
#include    <linux/if_bridge.h>

static int init_monisd()
{
    struct  sockaddr_nl sa;
    int     soc =socket(AF_NETLINK,SOCK_DGRAM,NETLINK_ROUTE);

    sa.nl_family=AF_NETLINK;
    sa.nl_groups=RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;
    bind(soc,(struct sockaddr *)&sa,sizeof(sa));

    return soc;
}

static struct rtattr *dec_msghdr(int *family,struct nlmsghdr    *nlhdr){
    struct ifinfomsg    *ifimsg = NULL;
    struct ifaddrmsg    *ifamsg = NULL;

    int ifi_family;
    int ifi_index;
    int ifi_flags;

    printf("len: %d,",nlhdr->nlmsg_len);
    printf("type: %x: ",nlhdr->nlmsg_type);

    switch( nlhdr->nlmsg_type ){
        case RTM_NEWLINK:
            ifimsg  = NLMSG_DATA(nlhdr);
            printf("NEW LINK\n");
            break;
        case RTM_DELLINK:
            ifimsg  = NLMSG_DATA(nlhdr);
            printf("DEL LINK\n");
            break;

        case RTM_DELADDR:
            ifamsg  = NLMSG_DATA(nlhdr);
            printf("DEL ADDR\n");
            break;

        case RTM_GETADDR:
            ifamsg  = NLMSG_DATA(nlhdr);
            printf("GET ADDR\n");
            break;

        default:
            printf("<Error: uncheck %d continue>\n",nlhdr->nlmsg_type);
            return NULL;
    }

    if(ifimsg){
        ifi_family  = ifimsg->ifi_family;
        ifi_index   = ifimsg->ifi_index;
        ifi_flags   = ifimsg->ifi_flags;
    }
    else{
        ifi_family  = ifamsg->ifa_family;
        ifi_index   = ifamsg->ifa_index;
        ifi_flags   = ifamsg->ifa_flags;
    }

    *family            =ifi_family; 
    printf("family:%d/",ifi_family);
    switch( ifi_family ){
        case AF_UNSPEC:         printf("UniSpec\n");    break;
        case AF_INET6:          printf("Inet6\n");      break;
        case AF_INET:           printf("Inet\n");       break;
        case AF_BRIDGE:         printf("bridge\n");     break;
        default:                
                                printf("Error family:%x continue\n",ifi_family);
                                return NULL;
    }
    if( ifimsg ){
        printf(" type: %x,",ifimsg->ifi_type);
        printf("change: %x\n", ifimsg->ifi_change);
    }

    printf(" index: %d\n",ifi_index);
    printf(" flags: %x =   ",ifi_flags);

    if(ifi_flags&IFF_UP)            printf("UP,");
    if(ifi_flags&IFF_BROADCAST)     printf("BROADCAST,");
    if(ifi_flags&IFF_DEBUG)         printf("DEBUG,");
    if(ifi_flags&IFF_LOOPBACK)      printf("LOOPBACK,");
    if(ifi_flags&IFF_POINTOPOINT)   printf("POINTOPOINT,");
    if(ifi_flags&IFF_NOTRAILERS)    printf("NOTRAILERS,");
    if(ifi_flags&IFF_RUNNING)       printf("RUNNING,");
    if(ifi_flags&IFF_NOARP)         printf("NOARP,");
    if(ifi_flags&IFF_PROMISC)       printf("PROMISC,");
    if(ifi_flags&IFF_MASTER)        printf("MASTER,");
    if(ifi_flags&IFF_SLAVE)         printf("SLAVE,");
    if(ifi_flags&IFF_MULTICAST)     printf("MULTICAST,");
    if(ifi_flags&IFF_PORTSEL)       printf("PORTSEL,");
    if(ifi_flags&IFF_AUTOMEDIA)     printf("AUTOMEDIA,");
    if(ifi_flags&IFF_DYNAMIC)       printf("DYNAMIC,");
    if(ifi_flags&IFF_LOWER_UP)      printf("LOWER_UP,");
    if(ifi_flags&IFF_DORMANT)       printf("DORMANT,");
    if(ifi_flags&IFF_ECHO)          printf("ECHO,");
    printf("\n");
    return ifimsg ? IFLA_RTA(ifimsg) : IFLA_RTA(ifamsg);
}

static const char *operstate2str(int stat)
{
    switch (stat){
        case IF_OPER_UNKNOWN:       return "Unknown";
        case IF_OPER_NOTPRESENT:    return "NetPresent";
        case IF_OPER_DOWN:          return "Down";
        case IF_OPER_LOWERLAYERDOWN: return "LowerDown";
        case IF_OPER_TESTING:       return "Testing";
        case IF_OPER_DORMANT:       return "L1Up";
        case IF_OPER_UP:            return "Up";
        default:                    return "UNKNOWN";
    }
}


int main()
{
    int soc = init_monisd();

    while(1){
        struct timespec     now;
        struct tm           day;

        char                buf[4096];
        struct nlmsghdr    *nlhdr;

        int n=recv(soc,buf,sizeof(buf),0);
        if(n<0){
            perror("recv");
            return(1);
        }

        clock_gettime(CLOCK_REALTIME,&now);
        day   = *localtime(&now.tv_sec);
        printf("%02d:%02d:%02d.%03ld %ld ",day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000,syscall(__NR_gettid));

        for(nlhdr=(struct nlmsghdr *)buf;NLMSG_OK(nlhdr,n);nlhdr=NLMSG_NEXT(nlhdr,n)){
            int                 rtalist_len;
            int ifi_family;
            
            struct rtattr       *rta = dec_msghdr(&ifi_family,nlhdr);

            if( !rta )
                continue;

            rtalist_len=nlhdr->nlmsg_len-NLMSG_LENGTH(sizeof(struct ifinfomsg));
            for(;RTA_OK(rta,rtalist_len);rta=RTA_NEXT(rta,rtalist_len)){
                unsigned char *p = (unsigned char *)RTA_DATA(rta);
                printf(" type:%x =",rta->rta_type);
                /*
                if( ifi_family == AF_BRIDGE ){
                    switch(rta->rta_type){
                        case IFLA_BRIDGE_FLAGS:
                            printf(" bridge flags: %d\n",*(unsigned short *)p); break;

                        case IFLA_BRIDGE_MODE:
                            printf(" bridge mode: %d\n",*(unsigned short *)p); break;
                        default:
                            printf(" unkown\n"); break;
                    }
                    continue;
                }*/

                switch(rta->rta_type){
                    case IFLA_MTU:
                        printf(" MTU: %d\n",*(int *)p);
                        break;
                    case IFLA_ADDRESS:
                        printf(" HWADDR: %02X%02X%02X%02X%02X%02X\n",p[0],p[1],p[2],p[3],p[4],p[5]);
                        break;
                    case IFLA_BROADCAST:
                        printf(" BROADCASTADDR: %d.%d.%d.%d\n",p[0],p[1],p[2],p[3]);
                        break;
                    case IFLA_IFNAME:
                        printf(" IFName: %s\n",(char *)p);
                        break;
                    case IFLA_LINK:
                        printf(" Link Type: %d\n",*(int *)p);
                        break;
                    case IFLA_QDISC:
                        printf(" QDisc:%s\n",(char *)p);
                        break;
                    case IFLA_STATS:
                        printf(" STAT\n");
                        break;
                    case IFLA_COST:
                        printf(" cost: %d\n",*(int *)p);
                        break;
                    case IFLA_PRIORITY:
                        printf(" prio: %d\n",*(int *)p);
                        break;
                    case IFLA_MASTER:
                        printf(" master: %d\n",*(int *)p);
                        break;
                    case IFLA_WIRELESS:
                        printf(" wireless: %d\n",*(int *)p);
                        break;
                    case IFLA_PROTINFO:
                        printf(" ProtInfo: %d\n",p[0]);

                        break;
                    case IFLA_TXQLEN:
                        printf("txqlen: %d\n",*(int *)p);
                        break;
                    case IFLA_MAP:
                        printf("map: %d\n",*(int *)p);
                        break;
                    case IFLA_WEIGHT:
                        printf("weight: %d\n",*(int *)p);
                        break;
                    case IFLA_OPERSTATE:
                        printf(" operstate: %d/%s\n",*(int *)p,operstate2str(*(int *)p));
                        break;
                    case IFLA_LINKMODE:
                        printf(" linkmode: %d\n",*(int *)p);
                        break;
                    case IFLA_LINKINFO:
                        printf(" linkinfo: %d\n",*(int *)p);
                        break;

                    case IFLA_NET_NS_PID:
                        printf(" net ns pid: %d\n",*(int *)p);
                        break;
                    case IFLA_IFALIAS:
                        printf(" QDisc:%s\n",(char *)p);
                        break;
                    case IFLA_NUM_VF:
                        printf(" num vf: %d\n",*(int *)p);
                        break;
                    case IFLA_VFINFO_LIST:
                        printf(" vf info list: %d\n",*(int *)p);
                        break;
                    case IFLA_STATS64:
                        printf(" stat64: %d\n",*(int *)p);
                        break;
                    case IFLA_VF_PORTS:
                        printf(" vf ports: %d\n",*(int *)p);
                        break;
                    case IFLA_PORT_SELF:
                        printf(" port self: %d\n",*(int *)p);
                        break;
                    case IFLA_AF_SPEC:
                        {
                            struct nlattr *spec = (struct nlattr *)p;
                            printf(" af spec: %d\n",*(int *)p);
                        }
                        break;
                    case IFLA_GROUP:
                        printf(" group: %d\n",*(int *)p);
                        break;
                    case IFLA_NET_NS_FD:
                        printf(" net ns fd: %d\n",*(int *)p);
                        break;
                    case IFLA_EXT_MASK:
                        printf(" ext mask: %d\n",*(int *)p);
                        break;
                    case IFLA_PROMISCUITY:
                        printf(" promisc: %d\n",*(int *)p);
                        break;
                    case IFLA_NUM_TX_QUEUES:
                        printf(" tx queue: %d\n",*(int *)p);
                        break;

                    case IFLA_NUM_RX_QUEUES:
                        printf(" rx queue: %d\n",*(int *)p);
                        break;
                    case IFLA_CARRIER:
                        printf(" carrier: %d\n",*(int *)p);
                        break;

                    case IFLA_PHYS_PORT_ID:
                        printf(" phy port id: %d\n",*(int *)p);
                        break;

                    default:
                        printf(" UNKNOWN\n",rta->rta_type);
                        break;
                }
            }
        }
        printf("\n");
    }

    close(soc);
    return(0);
}
#endif


/////////////////////////////////////////////////////
#ifdef GOOGLEPROTOBUF_API_

    bool Rain_GetEthInfo(std::string& name,std::string& ipaddr,std::string& macaddr)
    {
        char    addrBuf[128]    = "";
        bool    ret             = false;
        struct ifaddrs *ifa_list= 0, *ifa;

        if (getifaddrs(&ifa_list)== -1) {
            return false;
        }

        int32_t sd = socket( PF_INET, SOCK_DGRAM, 0 );
        if( sd < 0 )
        {
            /// free memory allocated by getifaddrs
            freeifaddrs( ifa_list );
            return false;
        }

        for (ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || !ifa->ifa_data)
                continue;

            /// print MAC address
            struct ifreq req;
            strcpy( req.ifr_name, ifa->ifa_name );
            if( ioctl( sd, SIOCGIFHWADDR, &req ) != -1 ){
                uint8_t* mac = (uint8_t*)req.ifr_ifru.ifru_hwaddr.sa_data;
                char    macbuf[32]      = "";
                sprintf(macbuf, "%02X:%02X:%02X:%02X:%02X:%02X",
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
                macaddr = macbuf;
            }

            if(ifa->ifa_addr->sa_family == AF_INET) {
                const struct sockaddr_in *addr = (const struct sockaddr_in*)ifa->ifa_addr;
                if(inet_ntop(AF_INET, &addr->sin_addr, addrBuf, 127)) {
                    ret = true;
                }
            } 
            else {
                const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6*)ifa->ifa_addr;
                if(inet_ntop(AF_INET6, &addr6->sin6_addr, addrBuf, 127)) {
                    ret = true;
                }
            }
            
            if( ret ){
                if( ifa->ifa_name)
                    name = ifa->ifa_name;
                addrBuf[127]	= 0;
                ipaddr	= addrBuf;
                if( strcmp(ifa->ifa_name,"lo") )
                    break;
            }
        }

        freeifaddrs(ifa_list);
        Rain_CloseSocket(sd);
        return ret;
    }

    int Rain_InitUDP(IPEndPoint *server_address,IPEndPoint* local_address,const char *hostname,uint16_t port,bool isserv){
        char	service[32];
        struct addrinfo hints, *res, *ressave;
        int n;

        int sd      = -1;
        int retval  = -1;
        IPEndPoint convaddr;

        //  server input local name and port, expect output local_address
        //  client input server's name and port, expect output server_address and local_address
        if( !local_address )
            return -1;

        if( isserv ){
            if( port == 0 )
                return -1;

            if( !hostname )
                hostname    = "0.0.0.0";
        }
        else{
            if( !hostname || !hostname[0] || port == 0 || !server_address )
                return -1;
        }

        sprintf(service,"%d",port);

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags  = AI_ADDRCONFIG;


        n = getaddrinfo(hostname, service, &hints, &res);

        if (n <0 || !res) {
            fprintf(stderr,
                    "getaddrinfo error:: [%s]\n",
                    gai_strerror(n));
            return -1;
        }

        ressave = res;

        do{
            // check address
            retval = convaddr.FromSockAddr(res->ai_addr,res->ai_addrlen) ? 0 : -1;
            if( retval != 0){
                continue;
            }

            int family  = convaddr.GetFamily();
            
            if( convaddr.address() == IPAddress::IPv4Localhost()
                    || convaddr.address() == IPAddress::IPv6Localhost() ){
                if( isserv || family == ADDRESS_FAMILY_IPV6 )
                    continue;
            }
            else if( convaddr.address().IsZero() ){
                if( !isserv )
                    continue;
            }

            //  check if socket can success create
            sd  = socket( family == ADDRESS_FAMILY_IPV6 ? AF_INET6 : AF_INET,SOCK_DGRAM,0);
            if( sd == -1 )
                continue;

            struct sockaddr_storage	addr;
            socklen_t               addrlen = sizeof(addr);
            if( isserv ){
                retval = convaddr.ToSockAddr((struct sockaddr*)&addr,&addrlen) ? 0 : -1;
                if( retval == 0 )
                    retval = bind(sd,(sockaddr *)&addr,addrlen);
            }
            else{
                char        tmpbuf[1];
                IPEndPoint  tmpaddr(convaddr.address(),22);

                retval = tmpaddr.ToSockAddr((struct sockaddr*)&addr,&addrlen)? 0 : -1;
                if( retval == 0 )
                    retval = sendto(sd,tmpbuf,1,0,(struct sockaddr *)&addr,addrlen) == 1 ? 0 : -1;

                if( retval >= 0 ){
                    struct sockaddr_storage laddr;
                    socklen_t address_length   = sizeof(laddr);

                    retval = getsockname(sd,(sockaddr *)&laddr,&address_length);

                    if( retval == 0  ){
                        if( !local_address->FromSockAddr((sockaddr *)&laddr,address_length) ){
                            retval = -1;
                        }
                    }
                }
            }

            if( retval == 0 ){
                break;
            }

            Rain_CloseSocket(sd);
            sd  = -1;
        }while((res = res->ai_next) != NULL);

        freeaddrinfo(ressave);

        if( retval == 0 ){
            if( isserv ){
                *local_address  = convaddr;
            }
            else{
                *server_address  = convaddr;
            }

            return sd;
        }
        else{
            return -1;
        }
    }




    int Rain_UDPSend(int sd,const char *buf,int len,const IPEndPoint *peer_address)
    {
        if( peer_address ){
            sockaddr_storage   dst;
            socklen_t          dstlen  = sizeof(dst);
            if(! peer_address->ToSockAddr((sockaddr *)&dst, &dstlen) ){
                RAIN_TRACE_ERROR("serv send fail, addr invalid\n");
                return -1;
            }
            int ret;
            WRAP_SYSAPI(ret,sendto(sd,buf,len,0,(struct sockaddr *)&dst,dstlen));

            TRACE_UDP(buf,len,&dst,1,ret);
            return ret;
        }
        else{
            int ret;
            WRAP_SYSAPI(ret, send(sd,buf,len,0));
            TRACE_UDP(buf,len,NULL,1,ret);
            return ret;
        }
    }


    bool Rain_Poll(int sd,int timeout_ms)
    {
        int ret = -1;
        struct pollfd pfds	 = {sd,POLLIN,0};
        WRAP_SYSAPI( ret , poll(&pfds,1,timeout_ms));
        if( ret == 1 && ((pfds.revents & POLLIN)) ){
            return false;
        }
        return true;
    }

    int Rain_UDPRecv(int sd,char* buf,size_t maxsz,IPEndPoint *peer_address)
    {
        if( peer_address ){
            sockaddr_storage   dst;
            socklen_t          dstlen  = sizeof(dst);
            int ret;
            WRAP_SYSAPI(ret , recvfrom(sd,buf,maxsz,0,(struct sockaddr *)&dst,&dstlen));
            if(! peer_address->FromSockAddr((sockaddr *)&dst, dstlen) ){
                RAIN_TRACE_ERROR("serv recv %d fail, addr invalid\n",ret);
                return -1;
            }

            TRACE_UDP(buf,ret,&dst,0,ret);
            return ret;
        }
        else{
            int ret;
            WRAP_SYSAPI(ret , recv(sd,buf,maxsz,0));
            TRACE_UDP(buf,ret,NULL,0,ret);
            return ret;
        }
    }




    int Rain_TCPCreate(const char *serv,uint16_t port,bool isbind)
    {
        int 	sockfd	= -1;
        char	portstr[32];
        struct addrinfo hints, *res, *ressave;

        bzero(&hints, sizeof(struct addrinfo));
        hints.ai_family 		= PF_UNSPEC;
        hints.ai_socktype		= SOCK_STREAM;
        if( isbind ){
            hints.ai_flags		= AI_PASSIVE;
        }
        else{
            if( !serv || !serv[0] ){
                RAIN_TRACE_WARNING( "SockAPI_Create	param error for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
                return -1;
            }
        }

        sprintf(portstr,"%d",port);

        int ret =  getaddrinfo(serv, portstr, &hints, &res);
        if( ret != 0) {
            RAIN_TRACE_WARNING( "SockAPI_Create getaddr fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
            return -1;
        }

        ressave = res;

        do {
            struct sockaddr_in* paddr = (struct sockaddr_in*)res->ai_addr;
            if( isbind ){
                if( paddr->sin_addr.s_addr == htonl(0x7f000001))
                    continue;
            }
            else{
                if( paddr->sin_addr.s_addr == 0)
                    continue;
            }

            sockfd = socket(res->ai_family, res->ai_socktype,
                    res->ai_protocol);
            if(sockfd < 0) {
                RAIN_TRACE_WARNING( "SockAPI_Create getsock fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);

                continue;
            }

            int on	= 1;
            if( isbind ){
                if( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0 ){
                    RAIN_TRACE_WARNING( "SockAPI_Create reuse port fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
                }
                if(bind(sockfd, res->ai_addr, res->ai_addrlen) == 0
                        && listen(sockfd, TCP_DEFAULTBACK_LOG) == 0 ) {
                    break;
                }
            }
            else{
                if(connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
                    break;
                }
                setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on)) ;
            }
            Rain_CloseSocket(sockfd);
            sockfd = -1;
            RAIN_TRACE_WARNING( "SockAPI_Create init sock fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
        }while((res = res->ai_next) != NULL);

        freeaddrinfo(ressave);
        if( sockfd >= 0 )
            return sockfd;

        RAIN_TRACE_WARNING( "SockAPI_Create fatal create fail for %s:%d bind:%d\n",serv ? serv: "NULL",port,isbind);
        return -1;
    }





    bool Rain_RecvN(int sd,char* p, size_t size,int timeout_ms,uint16_t interval_wait_ms){
        if( sd < 0 )
            return false;

        while( size > 0 ){
            int ret;
            if( timeout_ms != -1 ){
                struct pollfd pfds	 = {sd,POLLIN,0};
                WRAP_SYSAPI( ret , poll(&pfds,1,timeout_ms));
                if( ret != 1 || (!(pfds.revents & POLLIN)) ){
                    return false;
                }

                timeout_ms = interval_wait_ms;
            }

            WRAP_SYSAPI( ret , recv(sd,p,size,0) );

            if( ret <= 0 ){
                RAIN_TRACE_WARNING( " %d recv data error, ret: %d,errno:%d\n" ,sd,ret , errno);

                return false;
            }

            size	-= ret;
            p	+= ret;
        }

        return true;
    }

    bool Rain_SendN(int sd,const char* p, size_t size){
        if( sd < 0 )
            return false;
        while( size > 0 ){
            int ret;
            WRAP_SYSAPI( ret , send(sd,p,size,0) );

            if( ret <= 0 ){
                RAIN_TRACE_WARNING( " %d send data error, ret: %d,errno:%d\n" ,sd,ret , errno);
                return false;
            }

            size	-= ret;
            p	+= ret;
        }

        return true;
    }


    bool Rain_StartTask(pthread_t *thrid,void* (*start_routine)(void*),void *arg)
    {
        pthread_t slave_tid;
        int ret = pthread_create(thrid ? thrid : &slave_tid, NULL, start_routine, arg);
	if( ret == 0 && !thrid )
	    pthread_detach(slave_tid);
        return ret == 0;
    }




    static int Rsa2String(RSA *r,std::string& out,bool isPublic)
    {
        //bp_private = BIO_new_file(RSA_PRIV_FILE, "w+");
        BIO*	bp	= BIO_new(BIO_s_mem());
        int 	ret = isPublic ? PEM_write_bio_RSAPublicKey(bp, r) :
            PEM_write_bio_RSAPrivateKey(bp, r, NULL, NULL, 0, NULL, NULL);;
        if(ret != 1){
            out.clear();
            BIO_free_all(bp);
            return 0;
        }

        int keylen = BIO_pending(bp);	 
        char	*buf	= new char[keylen+1];

        BIO_read(bp,buf , keylen);
        buf[keylen] = 0;
        out.assign(buf,keylen+1);
        delete[] buf;

        BIO_free_all(bp);
        return 1;
    }

    bool Rain_RSAGenKey(std::string& priv,std::string& pub){
        priv.clear();
        pub.clear();

        RSA 			*r = NULL;

        // 1. generate rsa key
        BIGNUM* bne = BN_new();
        int ret = BN_set_word(bne,RSA_F4);
        if(ret != 1){
            goto free_all;
        }

        r = RSA_new();
        ret = RSA_generate_key_ex(r, 2048, bne, NULL);
        if(ret != 1){
            goto free_all;
        }

        // 2. save public key
        // 3. save private key
        ret = Rsa2String(r,priv,false);
        if( ret == 1 ){
            ret = Rsa2String(r,pub,true);
            if( ret != 1 ){
                priv.clear();
            }
        }

        // 4. free
free_all:
        RSA_free(r);
        BN_free(bne);

	/* ////	ONLY CHECK RSA SIGN/VERIFY FUNCTION
	std::string sign;
	const char* data	= "hellpafafasfasfadsasfsdafafasdfsdagfgfafgadsfasfasdfadf";
	int ret1 = Rain_RSASign(sign,data,strlen(data),priv.data(),priv.size());
	int ret2 = Rain_RSAVerify(sign,data,strlen(data),pub.data(),pub.size());
	*/
        return (ret == 1);
    }

    int Rain_RSA256Hash(unsigned char hash[32],const char *string, int len)
    {
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, string, len);
        SHA256_Final(hash, &sha256);
        return 0;
    }


    int Rain_RSASign(std::string &sign,const char *data, int dataLen,const char *privkey, int privkeylen)
    {   
        if( !data || dataLen <= 0 || !privkey || privkeylen <= 0 )
            return -1;

	unsigned char hash[SHA256_DIGEST_LENGTH]; 
	unsigned char signature[2048/8]; 
	unsigned int signatureLength; 
	int sigret = 0; 

	SHA256( (unsigned char *)data,dataLen, hash );

#ifdef __APPLE__
    BIO *mem = BIO_new_mem_buf((void *)privkey, privkeylen);
#else
    BIO *mem = BIO_new_mem_buf(privkey, privkeylen);
#endif
	if (mem != NULL){
	    RSA *rsa_private = PEM_read_bio_RSAPrivateKey(mem, NULL, NULL, NULL);
	    if (rsa_private != NULL
		    && RSA_check_key(rsa_private) != 0 ){
		sigret = RSA_sign( NID_sha256, hash, SHA256_DIGEST_LENGTH, signature, &signatureLength, rsa_private );
		if( sigret == 1 )
		    sign.assign((char *)signature,signatureLength);
	    }

	    if( rsa_private )
		RSA_free(rsa_private);
	    BIO_free (mem);
	}

	return sigret == 1 ? 0 : -1;
    }

    int Rain_RSAVerify(const std::string &sign,const char *data, int dataLen,const char *pubkey, int pubkeylen)
    {
	if( !data || dataLen <= 0 || !pubkey || pubkeylen <= 0 )
	    return -1;

	unsigned char hash[SHA256_DIGEST_LENGTH]; 
	int sigret = 0; 

	SHA256( (unsigned char *)data,dataLen, hash );

#ifdef __APPLE__
	BIO *mem = BIO_new_mem_buf((void*)pubkey, pubkeylen);
#else
    BIO *mem = BIO_new_mem_buf(pubkey, pubkeylen);
#endif
	if (mem != NULL){
	    RSA *rsa_public = PEM_read_bio_RSAPublicKey(mem, NULL, NULL, NULL);
	    if (rsa_public != NULL ){
		sigret = RSA_verify( NID_sha256, hash, SHA256_DIGEST_LENGTH, (const unsigned char *)sign.data(), sign.size(), rsa_public );
	    }

	    if( rsa_public )
		RSA_free(rsa_public);
	    BIO_free (mem);
	}

	return sigret == 1 ? 0 : -1;
    }

#define OPENSSL_PRN_INFO(fmt,arg...)    

static    int gcmdeccnt = 0;
static    int gcmenccnt = 0;
#define OPENSSL_PRN_ERROR(fmt,arg...)   {   \
    char    buf[256]    = "";   unsigned long err = ERR_get_error();    \
    RAIN_TRACE_WARNING(  fmt " err:%lu dec/enc:%d/%d <%s>\n",##arg,err,gcmdeccnt,gcmenccnt,ERR_error_string(err,buf)); \
}

#define OPENSSL_PUT_ERROR(x,y)      OPENSSL_PRN_ERROR( #x " " #y "dec/enc:%d/%d \n",gcmdeccnt,gcmenccnt)



    int RMXQuic_GCMEncrypt(const unsigned char *plaintext, int plaintext_len, const unsigned char *aad,
            int aad_len, const unsigned char *key,const unsigned char *iv,int iv_len,
            unsigned char *ciphertext,int taglen)
    {
        EVP_CIPHER_CTX *ctx;

        int len;

        int ciphertext_len;


        /* Create and initialise the context */
        if(!(ctx = EVP_CIPHER_CTX_new())){
            OPENSSL_PRN_ERROR("create ctx fail\n");
            return -1;
        }

        /* Initialise the encryption operation. */
        if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("init ctx fail\n");
            return -1;
        }

        /* Set IV length if default 12 bytes (96 bits) is not appropriate */
        if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_SET_IVLEN fail\n");
            return -1;
        }

        /* Initialise key and IV */
        if(1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_SET_IV + key fail\n");
            return -1;
        }

        /* Provide any AAD data. This can be called zero or more times as
         * required
         */
        if( aad && aad_len > 0 ){
            if(1 != EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len)){
                EVP_CIPHER_CTX_free(ctx);
                OPENSSL_PRN_ERROR("EVP_EncryptUpdate NULL fail\n");
                return -1;
            }
        }


        OPENSSL_PRN_INFO("enc: %d out: txtlen %d + %d aad:%d \n", plaintext_len,ciphertext_len,len,aad_len);

        /* Provide the message to be encrypted, and obtain the encrypted output.
         * EVP_EncryptUpdate can be called multiple times if necessary
         */
        if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_EncryptUpdate txt fail\n");
            return -1;
        }
        ciphertext_len = len;

        /* Finalise the encryption. Normally ciphertext bytes may be written at
         * this stage, but this does not occur in GCM mode
         */
        if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_EncryptFinal_ex fail\n");
            return -1;
        }

        OPENSSL_PRN_INFO("enc:%s %d out: txtlen %d + %d\n",plaintext, plaintext_len,ciphertext_len,len);
        ciphertext_len += len;

        /* Get the tag */
        if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, taglen, ciphertext + ciphertext_len )){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_GET_TAG fail\n");
            return -1;
        }

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        gcmenccnt ++;
        return ciphertext_len + taglen;
    }

    int RMXQuic_GCMDecrypt(const unsigned char *ciphertext, int ciphertext_len, const unsigned char *aad,
            int aad_len,const unsigned char *key, const unsigned char *iv,int iv_len,
            unsigned char *plaintext,int taglen)
    {
        EVP_CIPHER_CTX *ctx;
        int len;
        int plaintext_len;
        int ret;

        /* Create and initialise the context */
        if(!(ctx = EVP_CIPHER_CTX_new())){
            OPENSSL_PRN_ERROR("EVP_CIPHER_CTX_new fail\n");
            return -1;
        }

        /* Initialise the decryption operation. */
        if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_DecryptInit_ex fail\n");
            return -1;
        }

        /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
        if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_SET_IVLEN fail\n");
            return -1;
        }

        /* Initialise key and IV */
        if(!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_CTRL_GCM_GET_TAG fail\n");
            return -1;
        }

        /* Provide any AAD data. This can be called zero or more times as
         * required
         */
        if( aad && aad_len > 0 ){
            if(!EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len)){
                EVP_CIPHER_CTX_free(ctx);
                OPENSSL_PRN_ERROR("EVP_DecryptUpdate aad fail\n");
                return -1;
            }
        }

        /* Provide the message to be decrypted, and obtain the plaintext output.
         * EVP_DecryptUpdate can be called multiple times if necessary
         */

        if(!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len - taglen)){
            EVP_CIPHER_CTX_free(ctx);
            OPENSSL_PRN_ERROR("EVP_DecryptUpdate txt fail\n");
            return -1;
        }
        plaintext_len = len;

        /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
        if( taglen > 0 ){
            unsigned char tag[taglen];
            memcpy(tag,ciphertext + plaintext_len,taglen);
            if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, taglen,tag)){
                EVP_CIPHER_CTX_free(ctx);
                OPENSSL_PRN_ERROR("EVP_CTRL_GCM_SET_TAG fail\n");
                return -1;
            }
        }

        /* Finalise the decryption. A positive return value indicates success,
         * anything else is a failure - the plaintext is not trustworthy.
         */
        ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
        OPENSSL_PRN_INFO("dec:%s %d in: ciphlen %d + %d ret:%d\n",plaintext, plaintext_len,ciphertext_len- taglen,len,ret);

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        if(ret > 0)
        {
            /* Success */
            plaintext_len += len;
            gcmdeccnt ++;
            return plaintext_len;
        }
        else
        {
            /* Verify failed */
            OPENSSL_PRN_ERROR("dec:%s %d in: ciphlen %d + %d ret:%d\n",plaintext, plaintext_len,ciphertext_len- taglen,len,ret);
            return -1;
        }
    }
#endif
}


#endif


#if 0
    bool EPollAdd(int sd,RMXQuicWriter::EPollMode mode ,RMXQUIC_POLL_CALLBACK parser,void *data)
    {
        if( sd < 0 || epollfd_ < 0 )
            return false;

        __uint32_t event    = 0;// EPOLLERR|EPOLLHUP;
        if( mode == RMXQuicWriter::EPollRead || mode == RMXQuicWriter::EPollRW )
            event   |= EPOLLIN |EPOLLPRI;
        if( mode == RMXQuicWriter::EPollWrite || mode == RMXQuicWriter::EPollRW )
            event   |= EPOLLOUT;

        struct epoll_event ev;
        memset(&ev,0,sizeof(ev));
        ev.events = event;
        ev.data.fd = sd;
        if( epoll_ctl(epollfd_,EPOLL_CTL_ADD,sd,&ev) != 0 ){
            RAIN_TRACE_ERROR("add %d:%p,%p ev:%x fail errcode:%d\n",sd,parser,data,event,errno);
            return false;
        }

        pollfds_[sd]    = HeapItem(parser,data,mode);
        RMXQUIC_TRACE_DEBUG("add %d:%p,%p ev:0x%x \n",sd,parser,data,event);

        return true;
    }

    void EPollDel(int sd)
    {
        if( epoll_ctl(epollfd_,EPOLL_CTL_DEL,sd,NULL) != 0 
                && errno != ENOENT )
            RAIN_TRACE_WARNING("del %d fail errcode:%d\n",sd,errno);
        pollfds_.erase(sd);
        RAIN_TRACE_DEBUG("del %d\n",sd);
    }

    bool EPollMod(int sd, RMXQuicWriter::EPollMode mode){
        if( sd < 0 || epollfd_ < 0 || (pollfds_.find(sd) == pollfds_.end()) )
            return false;

        __uint32_t event    = 0;//EPOLLERR|EPOLLHUP;
        if( mode == RMXQuicWriter::EPollRead || mode == RMXQuicWriter::EPollRW )
            event   |= EPOLLIN |EPOLLPRI;
        if( mode == RMXQuicWriter::EPollWrite || mode == RMXQuicWriter::EPollRW )
            event   |= EPOLLOUT;

        struct epoll_event ev;
        memset(&ev,0,sizeof(ev));
        ev.events = event;
        ev.data.fd = sd;
        if( epoll_ctl(epollfd_,EPOLL_CTL_MOD,sd,&ev) != 0 ){
            RAIN_TRACE_ERROR("mod %d: to ev:%x fail errcode:%d\n",sd,event,errno);
            return false;
        }

        pollfds_[sd].mode_ = mode;
        RAIN_TRACE_DEBUG("mod %d ev:0x%x \n",sd,event);

        return true;
    }
