#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "ikcp.h"
#include "test.h"
#include <fcntl.h>

#define SERVER_IP "127.0.0.1"
#define CLIENT_IP "127.0.0.1"

#define SERVER_PORT 10000
#define CLIENT_PORT 20000
#define SEND_BUFF_SIZE 128

static int _g_client_sock = 0;
static struct sockaddr_in _g_client_addr = {0};
static struct sockaddr_in _g_server_addr = {0};


static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    int ret = -1;
	ret=sendto(_g_client_sock,buf,len,0,(struct sockaddr*)&(_g_server_addr), sizeof(_g_server_addr));
    printf("clietn sendto ret:%d\n",ret);
	return 0;
}

//用于设置recvfrom为非阻塞
static void setnonblocking(int sockfd) {
    int flag = fcntl(sockfd, F_GETFL, 0);
    if (flag < 0) {
        printf("fcntl F_GETFL fail");
        return;
    }
    if (fcntl(sockfd, F_SETFL, flag | O_NONBLOCK) < 0) {
        printf("fcntl F_SETFL fail");
    }
}

int main(void)
{
    /*创建UDP套接字*/
    _g_client_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    /*复用端口*/
    int _on = 1;
    setsockopt(_g_client_sock,SOL_SOCKET,SO_REUSEADDR,&_on,sizeof(_on));

    //设置为非阻塞模式
    setnonblocking(_g_client_sock);
    
    /*本地IP和端口*/
    _g_client_addr.sin_family = AF_INET;  //使用IPv4地址
    _g_client_addr.sin_addr.s_addr = inet_addr(CLIENT_IP);//本地IP
    _g_client_addr.sin_port = htons(CLIENT_PORT);  //本地端口

    bind(_g_client_sock, (struct sockaddr*)&_g_client_addr, sizeof(_g_client_addr));

    /*服务器端的IP和端口*/
    _g_server_addr.sin_family = AF_INET; //使用IPv4地址
    _g_server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); //目标IP
    _g_server_addr.sin_port = htons(SERVER_PORT);  //目标端口
    int _g_server_addr_len = sizeof(_g_server_addr);

    /*空的addr*/
    struct sockaddr_in temp_addr = {0};
    int temp_addr_len = sizeof(temp_addr);
    
    /*初始化KCP*/
    ikcpcb *kcp2 = ikcp_create(1, (void *)1);
    kcp2->output = udp_output;
	ikcp_nodelay(kcp2, 1, 10, 2, 1);
    kcp2->rx_minrto = 10;
    kcp2->fastresend = 1;
    if (ikcp_setmtu(kcp2,1500)<0)
    {
        printf("SET MTU faild!\n");
    }

    long current = iclock();
    long slap = current + 20;
    int index = 1;

    char *buf="hello KCP";

    char cli_send_buff[SEND_BUFF_SIZE]={0};
    char cli_recv_buff[SEND_BUFF_SIZE]={0};

    char *temp = (char *)malloc(1424);
    int next = 0;

    while(1)
    {
        int ret = -1;

        current = iclock();
        usleep(10);
        ikcp_update(kcp2, clock());

        
        //每隔 20ms，kcp2发送数据
        for (; current >= slap; slap += 20) {
            sprintf(cli_send_buff,"%s_%d",buf,index);
            //printf("client ikcp send: %d\n", index);
            ikcp_send(kcp2, cli_send_buff, strlen(cli_send_buff));
            index++;
        }

        while (1)
        {
            ret = recvfrom(_g_client_sock,cli_recv_buff,SEND_BUFF_SIZE,0,(struct sockaddr*)&_g_server_addr,&_g_server_addr_len);
            if (ret < 0)
            {
                break;
            }
            printf("recvfrom ret:%d\n",ret);
            ikcp_input(kcp2,cli_recv_buff,ret);
        }
        memset(cli_recv_buff,0,SEND_BUFF_SIZE);
        while (1)
        {
            ret = ikcp_recv(kcp2,cli_recv_buff,SEND_BUFF_SIZE);
            if (ret < 0)
            {
                break;
            }
            printf("recv from server:%s\n",cli_recv_buff);
            next++;
        }
        if(next==1000){
            break;
        }
    }
    free(temp);
    temp = NULL;
    return 0;
}