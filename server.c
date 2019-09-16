#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "ikcp.h"
#include <fcntl.h>

#define TARGET_IP "127.0.0.1"
#define LOCAL_IP "127.0.0.1"

#define SERVER_PORT 10000
#define CLIENT_PORT 20000
#define RECV_BUFF_SIZE 128
#define SEND_BUFF_SIZE 128

static int server_sock = 0;

static struct sockaddr_in _g_client_addr = {0};

static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	int ret=-1;
    ret=sendto(server_sock,buf,len,0,(struct sockaddr*)&(_g_client_addr), sizeof(_g_client_addr));
    //printf("sendto client OK,ret=%d\n",ret);
	return 0;
}

//用于设置recvfrom为非阻塞
static void setnonblocking(int sockfd) {
    int flag = fcntl(sockfd, F_GETFL, 0);
    if (flag < 0) {
        printf("fcntl F_GETFL fail\n");
        return;
    }
    if (fcntl(sockfd, F_SETFL, flag | O_NONBLOCK) < 0) {
        printf("fcntl F_SETFL fail\n");
    }
}

int main(void)
{
     /*创建UDP套接字*/
    server_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    /*复用端口*/
    int _on = 1;
    setsockopt(server_sock,SOL_SOCKET,SO_REUSEADDR,&_on,sizeof(_on));

    //设置为非阻塞模式
    setnonblocking(server_sock);

    /*本地IP和端口*/
    struct sockaddr_in server_addr = {0};
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;  //使用IPv4地址
    server_addr.sin_addr.s_addr = inet_addr(LOCAL_IP);//本地IP
    server_addr.sin_port = htons(SERVER_PORT);  //本地端口10000

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

    int _g_client_addr_len = sizeof(_g_client_addr);
    // 初始化KCP
    ikcpcb *kcp1 = ikcp_create(1, (void *)0);
    kcp1->output = udp_output;
    ikcp_nodelay(kcp1, 1, 10, 2, 1);
    kcp1->rx_minrto = 10;
	kcp1->fastresend = 1;
    if (ikcp_setmtu(kcp1,1500)<0)
    {
        printf("SET MTU faild!\n");
    }


    char ser_recv_buf[RECV_BUFF_SIZE]={0};
    char ser_send_buf[SEND_BUFF_SIZE]={0};

    printf(" server started..\n");

    char *temp = (char *)malloc(2000);

    while (1)
    {
        int ret = -1;
        
        usleep(10);
        ikcp_update(kcp1, clock());

        while (1)
        {
            ret = recvfrom(server_sock,ser_recv_buf,RECV_BUFF_SIZE,0,(struct sockaddr*)&_g_client_addr,&_g_client_addr_len);
            if (ret<0)
            {
                break;
            }
            printf("recvfrom ret:%d\n",ret);
            ikcp_input(kcp1,ser_recv_buf,ret);
            
        }
        memset(ser_recv_buf,0,RECV_BUFF_SIZE);
        while (1)
        {
            ret = ikcp_recv(kcp1,ser_recv_buf,RECV_BUFF_SIZE);
            if (ret < 0)
            {
                break;
            }
            printf("ikcp recv:%s  ret=%d\n",ser_recv_buf,ret);
            ikcp_send(kcp1,ser_recv_buf,strlen(ser_recv_buf)+1);
        }
    }
    free(temp);
    temp = NULL;
    return 0;
}