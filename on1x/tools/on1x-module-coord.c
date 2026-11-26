#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const char *roles[] = {"grammar","modules","imports","visibility",
    "signatures","asdl","ppx","stdlib","integration"};
enum { lanes = 9, revision = 1, capacity = 1024 };
struct message {
    uint32_t schema, from, to;
    uint64_t id, correlation;
    char kind[24], text[640];
};
struct record {
    uint32_t schema, size, offset;
    char data[capacity];
};
struct state {
    uint32_t schema;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    uint64_t sequence;
    struct record contracts, status[lanes];
};
static const char *prefix;
static struct state *state;
static unsigned created_queues;
static int created_state;
static int audit_fd = -1;
static void die(const char *reason) { perror(reason); exit(1); }
static void check(int rc, const char *reason) { if (rc) { errno = rc; die(reason); } }
static void name(char out[128], const char *suffix) {
    if (snprintf(out, 128, "/on1x_%s_%s", prefix, suffix) >= 128) exit(2);
}
static int unlink_queue(unsigned index) {
    char buf[128];
    name(buf, roles[index]);
    if (mq_unlink(buf) == 0 || errno == ENOENT) return 0;
    perror(buf);
    return 1;
}
static int unlink_state(void) {
    char buf[128];
    name(buf, "state");
    if (shm_unlink(buf) == 0 || errno == ENOENT) return 0;
    perror(buf);
    return 1;
}
static void rollback_init(void) {
    for (unsigned i = 0; i < created_queues; ++i) (void)unlink_queue(i);
    if (created_state) (void)unlink_state();
}
static int cleanup(void) {
    int failed = 0;
    for (unsigned i = 0; i < lanes; ++i) failed |= unlink_queue(i);
    return failed | unlink_state();
}
static void lock(void) {
    int rc = pthread_mutex_lock(&state->mutex);
    if (rc == EOWNERDEAD) check(pthread_mutex_consistent(&state->mutex), "recover mutex");
    else check(rc, "lock mutex");
}
static void open_audit(void) {
    const char *path = getenv("ON1X_MOD_AUDIT");
    if (!path || !*path) {
        fputs("ON1X_MOD_AUDIT must name an audit file\n", stderr);
        exit(2);
    }
    audit_fd = open(path,O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC|O_NONBLOCK,0600);
    if (audit_fd < 0) die("open audit");
    struct stat st;
    if (fstat(audit_fd, &st)) die("stat audit");
    if (!S_ISREG(st.st_mode)) {
        fputs("audit path must name a regular file\n", stderr);
        exit(2);
    }
}
static void audit(const char *action, const struct message *m) {
    char line[1024];
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME,&ts)) die("audit clock");
    int length = snprintf(line, sizeof line, "%lld.%09ld %s %llu %llu %s %s %s %s\n",
        (long long)ts.tv_sec,ts.tv_nsec,action,(unsigned long long)m->id,
        (unsigned long long)m->correlation,roles[m->from],roles[m->to],m->kind,m->text);
    if (length < 0 || (size_t)length >= sizeof line) exit(2);
    ssize_t written;
    do { written = write(audit_fd, line, (size_t)length); } while (written < 0 && errno == EINTR);
    if (written < 0) die("append audit");
    if (written != length) { errno = EIO; die("short audit write"); }
}
static void unlock(void) { check(pthread_mutex_unlock(&state->mutex), "unlock mutex"); }
static int role(const char *value) {
    for (int i = 0; i < lanes; ++i) if (!strcmp(roles[i],value)) return i;
    fprintf(stderr,"unknown role: %s\n",value); exit(2);
}
static void put(struct record *r, const char *text) {
    size_t size = strlen(text);
    if (size >= sizeof r->data) exit(2);
    r->schema = 0;
    memcpy(r->data,text,size+1);
    r->size = (uint32_t)size;
    r->offset = (uint32_t)((char *)r->data-(char *)state);
    r->schema = revision;
    check(pthread_cond_broadcast(&state->changed),"signal change");
}
static int print_record(const char *key, const struct record *r) {
    if (!r->schema) return 0;
    if (r->schema != revision || r->size >= sizeof r->data ||
        r->offset != (uint32_t)((const char *)r->data-(const char *)state) ||
        r->data[r->size] != '\0') {
        fprintf(stderr, "invalid shared record: %s\n", key);
        return 1;
    }
    printf("%s %.*s\n", key, (int)r->size, r->data);
    return 0;
}
static int valid_line(const char *text, size_t capacity) {
    return strlen(text) < capacity && !strpbrk(text, "\r\n");
}
static int valid_kind(const char *kind, size_t capacity) {
    return *kind && valid_line(kind, capacity) &&
        strspn(kind, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == strlen(kind);
}
static int parse_number(const char *input, uint64_t maximum, uint64_t *value) {
    if (!*input || strspn(input, "0123456789") != strlen(input)) return 0;
    errno = 0;
    unsigned long long parsed = strtoull(input, NULL, 10);
    if (errno == ERANGE || parsed > maximum) return 0;
    *value = (uint64_t)parsed;
    return 1;
}
static int usage(void) {
    fputs("usage: on1x-module-coord SESSION init|cleanup|snapshot\n"
          "       on1x-module-coord SESSION contract TEXT | state ROLE TEXT\n"
          "       on1x-module-coord SESSION send FROM TO KIND TEXT [CORRELATION]\n"
          "       on1x-module-coord SESSION recv ROLE [TIMEOUT_MS]\n", stderr);
    return 2;
}
static struct timespec deadline(long milliseconds) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME,&ts)) die("deadline clock");
    ts.tv_sec += milliseconds/1000;
    ts.tv_nsec += milliseconds%1000*1000000;
    if (ts.tv_nsec >= 1000000000) { ++ts.tv_sec; ts.tv_nsec -= 1000000000; }
    return ts;
}
int main(int argc,char **argv) {
    if (argc < 3) return usage();
    prefix=argv[1];
    if (!*prefix || strlen(prefix)>64 || strspn(prefix,"abcdefghijklmnopqrstuvwxyz0123456789_")!=strlen(prefix)) return usage();
    const char *cmd=argv[2];
    if (!strcmp(cmd,"cleanup")) return argc == 3 ? cleanup() : usage();
    int creating = !strcmp(cmd,"init");
    if ((creating || !strcmp(cmd, "snapshot")) && argc != 3) return usage();
    char buf[128];
    name(buf,"state");
    int fd=shm_open(buf,O_RDWR|(creating ? O_CREAT|O_EXCL : 0),0600);
    if (fd<0) die("shm_open");
    if (creating) {
        created_state = 1;
        if (atexit(rollback_init)) { rollback_init(); return 1; }
        if (ftruncate(fd,(off_t)sizeof *state)) die("ftruncate");
    }
    struct stat st;
    if (fstat(fd,&st)) die("stat shared state");
    if (st.st_size!=(off_t)sizeof *state) {
        fputs("invalid shared state size (or initialization in progress)\n", stderr);
        return 2;
    }
    state=mmap(NULL,sizeof *state,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if (state==MAP_FAILED) die("mmap");
    close(fd);
    if (creating) {
        pthread_mutexattr_t mutex;
        pthread_condattr_t cond;
        check(pthread_mutexattr_init(&mutex),"mutex attr");
        check(pthread_mutexattr_setpshared(&mutex,PTHREAD_PROCESS_SHARED),"shared mutex");
        check(pthread_mutexattr_setrobust(&mutex,PTHREAD_MUTEX_ROBUST),"robust mutex");
        check(pthread_mutex_init(&state->mutex,&mutex),"init mutex");
        check(pthread_condattr_init(&cond),"cond attr");
        check(pthread_condattr_setpshared(&cond,PTHREAD_PROCESS_SHARED),"shared cond");
        check(pthread_cond_init(&state->changed,&cond),"init cond");
        struct mq_attr attr={.mq_maxmsg=10,.mq_msgsize=sizeof(struct message)};
        for (int i=0;i<lanes;++i) {
            name(buf,roles[i]);
            mqd_t q=mq_open(buf,O_RDWR|O_CREAT|O_EXCL,0600,&attr);
            if (q==(mqd_t)-1) die("mq_open create");
            ++created_queues;
            if (mq_close(q)) die("mq_close create");
        }
        state->schema=revision;
        /* Successful creation leaves resources in place until explicit cleanup. */
        _Exit(0);
    }
    if (state->schema!=revision) return 2;
    if (!strcmp(cmd,"snapshot")) {
        lock();
        int invalid = print_record("contract", &state->contracts);
        for (int i=0;i<lanes;++i) invalid |= print_record(roles[i], &state->status[i]);
        unlock();
        return invalid ? 2 : 0;
    }
    if ((!strcmp(cmd,"contract") && argc==4) || (!strcmp(cmd,"state") && argc==5)) {
        int owner=argc==4 ? lanes-1 : role(argv[3]);
        const char *text=argv[argc-1];
        struct message m={.schema=revision,.from=(uint32_t)owner,.to=(uint32_t)(lanes-1)};
        strcpy(m.kind,cmd);
        if (!valid_line(text, sizeof m.text)) return usage();
        strcpy(m.text,text);
        open_audit();
        lock();
        put(argc==4 ? &state->contracts : &state->status[owner],text);
        m.id=++state->sequence;
        audit("publish",&m);
        unlock();
        return 0;
    }
    if (!strcmp(cmd,"send") && (argc==7 || argc==8)) {
        struct message m={.schema=revision,.from=(uint32_t)role(argv[3]),.to=(uint32_t)role(argv[4])};
        if (!valid_kind(argv[5], sizeof m.kind) || !valid_line(argv[6], sizeof m.text)) return usage();
        strcpy(m.kind,argv[5]); strcpy(m.text,argv[6]);
        if (argc==8 && !parse_number(argv[7], UINT64_MAX, &m.correlation)) return usage();
        open_audit();
        lock(); m.id=++state->sequence; unlock();
        name(buf,roles[m.to]);
        mqd_t q=mq_open(buf,O_WRONLY);
        if (q==(mqd_t)-1) die("mq_open send");
        for (int attempt=0;attempt<3;++attempt) {
            struct timespec until=deadline(250);
            if (!mq_timedsend(q,(const char *)&m,sizeof m,0,&until)) {
                lock(); audit("send",&m); unlock();
                if (mq_close(q)) die("mq_close send");
                printf("%llu\n",(unsigned long long)m.id);
                return 0;
            }
            if (errno!=EINTR && errno!=ETIMEDOUT) die("mq_timedsend");
        }
        lock(); audit("timeout",&m); unlock();
        if (mq_close(q)) die("mq_close timeout");
        return 3;
    }
    if (!strcmp(cmd,"recv") && (argc==4 || argc==5)) {
        int owner=role(argv[3]);
        uint64_t delay = 50;
        if (argc==5 && !parse_number(argv[4], 60000, &delay)) return usage();
        open_audit();
        name(buf,roles[owner]);
        mqd_t q=mq_open(buf,O_RDONLY);
        if (q==(mqd_t)-1) die("mq_open receive");
        struct timespec until=deadline((long)delay);
        for (;;) {
            struct message m;
            ssize_t size=mq_timedreceive(q,(char *)&m,sizeof m,NULL,&until);
            if (size<0 && errno==ETIMEDOUT) break;
            if (size<0 && errno==EINTR) continue;
            if (size<0) die("mq_timedreceive");
            if (size!=sizeof m || m.schema!=revision || m.from>=lanes || m.to!=(uint32_t)owner ||
                !memchr(m.text,0,sizeof m.text) || !memchr(m.kind,0,sizeof m.kind) ||
                !valid_line(m.text, sizeof m.text) || !valid_kind(m.kind, sizeof m.kind)) return 2;
            lock(); audit("recv",&m); unlock();
            printf("%llu %llu %s %s %s\n",(unsigned long long)m.id,
                (unsigned long long)m.correlation,roles[m.from],m.kind,m.text);
            until=deadline(0);
        }
        if (mq_close(q)) die("mq_close receive");
        return 0;
    }
    return usage();
}
