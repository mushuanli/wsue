#define TEST_RAIN_LOG
#undef TEST_OSAPI
#undef TEST_INOTIFY
#undef TEST_STLQUEUE_API
#define TEST_BLOCKQUEUE

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
#include <execinfo.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef ANDROID
#include <unwind.h>
#else
#include <execinfo.h>

#ifdef __cplusplus
#include <cxxabi.h>
#endif

#endif

#define RAINLILOG_FILENAME    "rainli.log"

#define RAIN_DBGLOG(fmt,arg...)         RainLog("[%s:%d] "fmt,__func__,__LINE__,##arg)
#define RAIN_DBGTRACE(fmt,arg...)       RainTrace("[%s:%d] "fmt,__func__,__LINE__,##arg)
void RainLog(const char *fmt,...) __attribute__ ((format (gnu_printf, 1, 2)));
void RainTrace(const char *fmt,...) __attribute__ ((format (gnu_printf, 1, 2)));

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
        printf("%02d:%02d:%02d.%03d ",day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000);
        vprintf(fmt,arg_ptr);
#else
        dprintf(fd,"%02d:%02d:%02d.%03d ",day.tm_hour,day.tm_min,day.tm_sec,now.tv_nsec/1000000);
        vdprintf(fd,fmt,arg_ptr);
#endif
        va_end(arg_ptr);
        close(fd);
    }
}


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
