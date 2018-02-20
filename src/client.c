#include "cross_header.h"
#include "client_lib.h"

int pid = -1;
struct protoent *proto = NULL;

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

void display(void* buf, int bytes)
{
    struct iphdr *ip = (struct iphdr*)buf;
    struct icmphdr *icmp = (struct icmphdr*)((int*)buf + ip->ihl);

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
    printf("pid: %u process:%d\n", icmp->un.echo.id, pid);
    if (icmp->un.echo.id == pid)
    {
        printf("ICMP: type[%d/%d] checksum[%d] id[%d] seq[%d] ",
            icmp->type, icmp->code, ntohs(icmp->checksum),
            icmp->un.echo.id, icmp->un.echo.sequence);
        struct timeval start, finish;
        start = *((struct timeval*)((char*)icmp + sizeof(struct icmphdr)));
        gettimeofday(&finish, NULL);
        tv_sub(&finish, &start);
        printf("time= %.3fms\n", finish.tv_sec * 1000.0 + finish.tv_usec / 1000.0);
    }
    return;
}

void listener(void)
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
        if (bytes > 0)
            display(buf, bytes);
        else
            perror("recvfrom");
    }
    exit(0);
}

void ping(struct sockaddr_in *addr)
{
    uint16_t mypid = getpid();
    const int val = 255;
    int sd, cnt = 1;
    struct packet pckt;
    struct sockaddr_in r_addr;

    sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
    if (sd < 0) {
        perror("socket");
        return;
    }
    if (setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
        perror("Set TTL option");
    if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0)
        perror("Request Nonblocking I/O");

    for (;;) {
        int len = sizeof(r_addr);
        printf("Msg #%d\n", cnt);
        if (recvfrom(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)&r_addr, (socklen_t*)&len) > 0)
            printf("***Got Message!***\n");
        memset(&pckt, 0, sizeof(pckt));
        pckt.hdr.type = ICMP_ECHO;
        pckt.hdr.un.echo.id = mypid;
        gettimeofday(&(pckt.tm), NULL);
        pckt.hdr.un.echo.sequence = cnt++;
        pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));
        if (sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0)
            perror("sendto");
        sleep(1);
    }
    return;
}

int main(int argc, char *argv[])
{
    struct hostent *hname;
    struct sockaddr_in addr;

    if (argc != 2) {
        printf("usage: %s <addr>\n", argv[0]);
        exit(0);
    }

    if (argc > 1) {
        proto = getprotobyname("ICMP");
        hname = gethostbyname(argv[1]);
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = hname->h_addrtype;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = *(long*)hname->h_addr;

        if ((pid = fork()) == 0)
            ping(&addr);
        else
            listener();
        wait(0);
    }
    else
        printf("usage: %s <addr>\n", argv[0]);
    return 0;
}
