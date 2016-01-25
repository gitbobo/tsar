#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tsar.h"

struct stats_nginx_sys {
    unsigned long long crash;
};

struct hostinfo {
    char *host;
    int   port;
    char *server_name;
    char *uri;
};

static char *nginx_sys_mport_usage = "    --nginx_sys_mport   nginx sys of multi-port";

static struct mod_info nginx_sys_mport_info[] = {
    {"crash", DETAIL_BIT,  0,  STATS_SUB},
};

static void
set_nginx_sys_mport_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < 1; i++) {
        if (cur_array[i] >= pre_array[i]) {
            st_array[i] = cur_array[i] - pre_array[i];
        } else {
            st_array[i] = 0;
        }
    }
}

static void
init_nginx_host_info(struct hostinfo *p)
{
    char *port;

    p->host = getenv("NGX_TSAR_HOST");
    p->host = p->host ? p->host : "127.0.0.1";

    port = getenv("NGX_TSAR_PORT");
    p->port = port ? atoi(port) : 80;

    p->uri = getenv("NGX_TSAR_URI");
    p->uri = p->uri ? p->uri : "/nginx_status";

    p->server_name = getenv("NGX_TSAR_SERVER_NAME");
    p->server_name = p->server_name ? p->server_name : "status.taobao.com";
}

/*
 *read data from nginx and store the result in buf
 * */
static int
read_one_nginx_sys_stats(char *parameter, char * buf, int pos)
{
    int                 write_flag = 0, addr_len, domain;
    int                 m, sockfd, send;
    void               *addr;
    char                request[LEN_4096], line[LEN_4096];
    FILE               *stream = NULL;

    struct sockaddr_in  servaddr;
    struct sockaddr_un  servaddr_un;
    struct hostinfo     hinfo;

    init_nginx_host_info(&hinfo);
    if (atoi(parameter) != 0) {
       hinfo.port = atoi(parameter);
    }
    struct stats_nginx_sys st_nginx;
    memset(&st_nginx, 0, sizeof(struct stats_nginx_sys));

    if (*hinfo.host == '/') {
        addr = &servaddr_un;
        addr_len = sizeof(servaddr_un);
        bzero(addr, addr_len);
        domain = AF_LOCAL;
        servaddr_un.sun_family = AF_LOCAL;
        strncpy(servaddr_un.sun_path, hinfo.host, sizeof(servaddr_un.sun_path) - 1);

    } else {
        addr = &servaddr;
        addr_len = sizeof(servaddr);
        bzero(addr, addr_len);
        domain = AF_INET;
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(hinfo.port);
        inet_pton(AF_INET, hinfo.host, &servaddr.sin_addr);
    }


    if ((sockfd = socket(domain, SOCK_STREAM, 0)) == -1) {
        goto writebuf;
    }

    sprintf(request,
            "GET %s HTTP/1.0\r\n"
            "User-Agent: taobot\r\n"
            "Host: %s\r\n"
            "Accept:*/*\r\n"
            "Connection: Close\r\n\r\n",
            hinfo.uri, hinfo.server_name);

    if ((m = connect(sockfd, (struct sockaddr *) addr, addr_len)) == -1 ) {
        goto writebuf;
    }

    if ((send = write(sockfd, request, strlen(request))) == -1) {
        goto writebuf;
    }

    if ((stream = fdopen(sockfd, "r")) == NULL) {
        goto writebuf;
    }

    while (fgets(line, LEN_4096, stream) != NULL) {
        if (!strncmp(line, "Crash:", sizeof("Crash:") - 1)) {
            sscanf(line, "Crash: %llu", &st_nginx.crash);
            write_flag = 1;
        } else {
            ;
        }
    }

writebuf:
    if (stream) {
        fclose(stream);
    }

    if (sockfd != -1) {
        close(sockfd);
    }

    if (write_flag) {
         pos += snprintf(buf + pos, LEN_1M - pos, "%d=%lld" ITEM_SPLIT,
                hinfo.port,
                st_nginx.crash
                );
        if (strlen(buf) == LEN_1M - 1) {
            return -1;
        }
        return pos;
    } else {
        return pos;
    }
}

void
read_nginx_sys_mport_stats(struct module *mod, char *parameter)
{
    int     pos = 0;
    int     new_pos = 0;
    char    buf[LEN_1M];
    char    *token;
    char    mod_parameter[LEN_256];

    buf[0] = '\0';
    strcpy(mod_parameter, parameter);
    if ((token = strtok(mod_parameter, W_SPACE)) == NULL) {
        pos = read_one_nginx_sys_stats(token,buf,pos);
    } else {
        do {
            pos = read_one_nginx_sys_stats(token,buf,pos);
            if(pos == -1){
                break;
            } 
        }
        while ((token = strtok(NULL, W_SPACE)) != NULL);
    }
    if(new_pos != -1) {
        set_mod_record(mod,buf);
    }
}

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--nginx_sys_mport", nginx_sys_mport_usage, nginx_sys_mport_info, 1, read_nginx_sys_mport_stats, set_nginx_sys_mport_record);
}
