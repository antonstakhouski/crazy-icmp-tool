#include "client_lib.h"

struct protoent *proto = NULL;
pthread_mutex_t lock;

unsigned short checksum(void *b, int len)
{
    unsigned short *buf = (unsigned short*)b;
    unsigned int sum = 0;
    unsigned short result;

    for(sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

void tv_sub(struct timeval *out, const struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0)
    {
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
    return;
}

int display(void* buf, int bytes, uint16_t thread_id)
{
    struct iphdr *ip = (struct iphdr*)buf;
    struct icmphdr *icmp = (struct icmphdr*)((int*)buf + ip->ihl);
    if (icmp->un.echo.id != thread_id)
        return -1;

    struct timeval finish;
    gettimeofday(&finish, NULL);
    pthread_mutex_lock(&lock);
    printf("----------\n");
    for (int i = 0; i < bytes; i++)
    {
        if (!(i & 15))
            printf("\nX:%2d", i);
        printf("%4x", ((unsigned char*)buf)[i]);
    }
    printf("\n");
    struct in_addr ntoa_addr;
    ntoa_addr.s_addr = ip->saddr;
    printf("IPv%d: hdr-size=%d pkt-size=%d protocol=%d  TTL=%d src=%s ",
        ip->version, ip->ihl * 4, htons(ip->tot_len), ip->protocol,
        ip->ttl, inet_ntoa(ntoa_addr));

    ntoa_addr.s_addr = ip->daddr;
    printf("dst=%s\n", inet_ntoa(ntoa_addr));
    printf("thread_id: %u thread:%u\n", icmp->un.echo.id, thread_id);
    if (icmp->un.echo.id == thread_id)
    {
        printf("ICMP: type[%d/%d] checksum[%d] id[%d] seq[%d] ",
            icmp->type, icmp->code, ntohs(icmp->checksum),
            icmp->un.echo.id, icmp->un.echo.sequence);
        struct timeval start ;
        start = *((struct timeval*)((char*)icmp + sizeof(struct icmphdr)));
        gettimeofday(&finish, NULL);
        tv_sub(&finish, &start);
        printf("time= %.3fms\n", finish.tv_sec * 1000.0 + finish.tv_usec / 1000.0);
        pthread_mutex_unlock(&lock);
        return 0;
    }
    pthread_mutex_unlock(&lock);
    return 1;
}

void listener(uint16_t thread_id)
{
    int sd;
    struct sockaddr_in addr;
    unsigned char buf[1024];

    sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
    if (sd < 0) {
        perror("socket");
        exit(0);
    }

    for(;;) {
        int bytes, len = sizeof(addr);

        memset(buf, 0, sizeof(buf));
        bytes = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, (socklen_t*)&len);
        if (bytes > 0) {
            int res = display(buf, bytes, thread_id);
            if (!res) {
                sleep(1);
                break;
            }
        }
    }
}

void* ping(void* arg)
{
    struct thread_info *tinfo = (struct thread_info*)arg;
    struct sockaddr_in* addr = &(tinfo->addr);
    const int val = 255;
    int sd, cnt = 1;
    struct packet pckt;

    sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
    if (sd < 0) {
        perror("socket");
        return NULL;
    }
    if (setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
        perror("Set TTL option");
    if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0)
        perror("Request Nonblocking I/O");

    for (;;) {
        memset(&pckt, 0, sizeof(pckt));
        pckt.hdr.type = ICMP_ECHO;
        pckt.hdr.un.echo.id = tinfo->thread_id;
        gettimeofday(&(pckt.tm), NULL);
        pckt.hdr.un.echo.sequence = cnt++;
        pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));
        if (sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0)
            perror("sendto");
        listener(tinfo->thread_id);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    struct hostent *hname;
    struct sockaddr_in addr;
    struct thread_info* tinfo;

    if (argc < 2) {
        printf("usage: %s <addr>\n", argv[0]);
        exit(0);
    }

    tinfo = (struct thread_info*)calloc(argc - 1, sizeof(struct thread_info));
    assert(!pthread_mutex_init(&lock, NULL));

    proto = getprotobyname("ICMP");
    for (int i = 1; i < argc; i++) {
        hname = gethostbyname(argv[i]);
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = hname->h_addrtype;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = *(long*)hname->h_addr_list[0];

        tinfo[i].addr = addr;
        pthread_create(&tinfo[i].thread_id, NULL, &ping, &tinfo[i]);

        /*if ((pid = fork()) == 0)*/
            /*ping(&addr);*/
        /*else*/
            /*listener();*/
    }
    while(1);
    pthread_mutex_destroy(&lock);
    return 0;
}
