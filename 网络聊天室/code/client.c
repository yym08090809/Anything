#include "chat.h"
int socketfd;  //套接字描述符
//菜单提示的数据结构   
typedef struct {
	char cmd[40];			//format
	char explain[100];		//description
	int admin;			//authority
}usage;
int global_admin_flag=NORMAL_USER;   	//用户的权限标记，默认为0
int globel_is_shutup;  			//用户是否被禁言的标记 ,默认为0
  
//0为普通用户具有的执行权限 ，1为管理员具有的执行权限 。
usage help_menu[] = {
	{"\tFormat to communicate",		"\tDescription",0},
	{"\t:your message", 			"\tChat with all users online",0},
	{"\t@username:your message",		"Chat with someone",0},
	{"\t--CheckOnline",            		"\tShow the users are online",0},
	{"\t--help" ,              		"\t\tDisplay the help information",0},
	{"\texit",		            	"\t\tExit",0},
	{"\t$Reducto:user",         		"\tKick the user out of this group",1},
	{"\t$Stupefy:user",        		"\tUser cannot speak for 5 mins", 1},
	{"\t$Lumos:user",	    		"\tPromote a user to be manager", 1},
	{"\t$Imperio:user", 	      		"\tReduce a manager", 1},
	{0,0,0}
}; 

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc ,char *argv[])
{
    	signal(SIGINT, signHandler); 
	inits();//初始化socket，挂起消息提醒的线程pthread_fun，运行处理用户输入字符串的write_data()函数
	close(socketfd);
	return 0;
}
//初始化链接
void inits()
{
	struct sockaddr_in server;
	if((socketfd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
        	perror("creat socket error\n");
        	exit(1);
    	}
	bzero(&server, sizeof(server));
	//memset(&server,0,sizeof(server));
	//bzero(&(server.sin_zero),8);  
	server.sin_family=AF_INET;
	server.sin_port=htons(PORT);
	inet_pton(AF_INET,IP,&server.sin_addr);
	if(( connect(socketfd,(struct sockaddr *)&server,sizeof(struct sockaddr)))==-1)
	{
        	perror("connect error\n");
         	exit(1);
    	} 
	reg_log(); //    login or register
	pthread_t tid;//线程标识符 pthread_t 
	if((pthread_create(&tid,NULL,pthread_fun,&socketfd))==-1)
	/*int pthread_create (pthread_t * thread_id, __const pthread_attr_t * __attr, void *(*__start_routine) (void *), void *__restrict __arg)
	线程标识符 pthread_t 
	第一个参数为指向线程标识符的指针；
	第二个参数用来设置线程属性，若取NULL，则生成默认属性的子线程；
	第三个参数是线程运行函数的起始地址，该函数是线程体函数，即子线程将完成的工作；
	第四个参数用来设定线程体函数的参数 ；若函数体不需要参数，则最后一个参数设为NULL。
	*/
		print_err("client pthread_create() error");		
	writedata();
}
//显示菜单项
void show_menu()
{
	int i = 0;
	printf("******************************************************************************\n");
	for(; help_menu[i].cmd[0] != 0; i++)
	{
		  if(global_admin_flag==ADMIN_USER) //管理员
		  	printf("*\t%s\t\t%s\n", help_menu[i].cmd, help_menu[i].explain);
		  else if(help_menu[i].admin==NORMAL_USER)//普通用户
		   	printf("*\t%s\t\t%s\n", help_menu[i].cmd, help_menu[i].explain);
	}
	printf("*******************************************************************************\n");
}
//进行选择注册或登陆 。
void reg_log()
{
	char ch;
	printf("1：Sign In ;\t2：Log In\n");
	printf("Please enter the number to sign in or log in:");
	while(1)
	{
		ch=getchar();
		if(ch=='2') //2代表登录
		{
			int get_ret=0;
  			while(1)
			{ 
				login();//输入用户密码，检测是否符合输入格式要求，接着将用户密码写入一个send_info结构体，发送给服务器
				read(socketfd,&get_ret,sizeof(get_ret));//读服务器的响应
				if(get_ret==NORMAL_USER_LOGIN_SUCCESS)//1
				{
					printf("\nYou have logged in successfully. Welcome！\n\n");
					global_admin_flag=NORMAL_USER;//0普通用户
				    	show_menu();//显示菜单
					break;
				}
				else if(get_ret==ADMIN_LOGIN_SUCCESS)//3
				{ 
					printf("\nYou have logged in successfully. Welcome, manager！\n\n");
					global_admin_flag=ADMIN_USER;//1管理员
				    	show_menu();
					break;
				}
				else if(get_ret==NORMAL_USER_LOGIN_FAILED_ONLINE)//2
                		{
					printf("\nLogin failed. The user is already online!\n");
                			exit(EXIT_SUCCESS);
					break;
                		}
                		else if(get_ret==NORMAL_USER_LOGIN_PASSWD_ERROR) //4
          	        		printf("\nIncorrect password. Please log in again！\n");
                		else//0
					printf("\nUsername error. Please log in again！\n");
			}
			break;
		}
		else if(ch=='1')
		{
			 int get_ret=0;
			 while(1)
			{
				register_client();
				read(socketfd,&get_ret,sizeof(int));
			  	if(get_ret==REGIST_EXITED)//账号已经存在 1
           				printf("The user already exists. Please re-enter！\n ");
 				else if(get_ret==REGIST_FALIED)//注册失败 0
					printf("Registration fsiled. Please re-enter!\n ");
				else 
				{
					printf("Registration success, username ：%d , please log in.\n ",get_ret);
					exit(EXIT_SUCCESS);//第一次注册成功后，客户端程序自动离开，我们需要重新登录	
					break;
				}
			}
			break;
		}
		else
		{
			printf("Input error. Please reselect.\n");
		}
		//清空输入流.
		for(  ;   (ch=getchar())!='\n'  && ch !=EOF; )
			continue;
	}      
}

//判断用户名是否输入了非法字符eg:空格，标点或特殊符号
void isvalid (char *str)
{
	while(1)
	{ 
		scanf("%s",str);
         	int  i=0 ,flag=-1;
		for(i=0;str[i]!=0;i++)
		{ 
			if(ispunct(str[i]))
			{
				flag=i;
				break;		   
  		        }
		}
		if (flag!=-1)
		{
			printf("Sorry, you entered an illegal character！Please re-enter.\n");
			bzero(str,sizeof(str));
		}
		else 
			break;
 	}
}
//注册新用户
void register_client()
{
	pthread_mutex_lock(&mutex);
	send_info send;
	printf("Username：\n");
	isvalid (send.name);//输入用户名，并且检测是否符合输入要求的格式
 	printf("Password：\n");
	isvalid (send.passwd);//输入密码，并且检测是否符合输入要求的格式
	send.type=CLIENT_REGISTER;
	write(socketfd,&send,sizeof(send));
	pthread_mutex_unlock(&mutex);
} 
//登陆
void login()
{
	send_info send;
	printf("Username："); 
  	isvalid (send.name);//输入用户名，并且检测是否符合输入要求的格式
 	printf("Password：");
  	isvalid (send.passwd);//输入密码，并且检测是否符合输入要求的格式
  	send.type=CLIENT_LOGIN;
  	write(socketfd,&send,sizeof(send));
}
// 接收数据
void *pthread_fun(void *arg)
{
	char buf[BUF_SIZE]={0};
	int length=0;
	while(1)
	{
		length=read(socketfd,buf,BUF_SIZE);
		if(length<=0)
		{
			printf("The server has been closed!\n");
			exit(EXIT_SUCCESS);	
			break;
		}
		if(strcmp("exit",buf)==0)
		{
		    	printf("You have been moved out from the chatting room, please check the rules and keep them in your mind.\n");
		    	exit(EXIT_SUCCESS);	
		    	break;
	    	}
	  	else if(strcmp("Stupefy",buf)==0)
       		{
           		printf("You have been banned for breaking the rules for 5 mins.\n");
           		globel_is_shutup =1;
           		continue;
       		}
     		else if(strcmp("Lumos",buf)==0)
       		{
           		printf("You are promoted to manager.\n");
           		global_admin_flag =ADMIN_USER;//1管理员
           		show_menu();
           		continue;
    		}
    		else if(strcmp("Imperio",buf)==0)
    		{
           		printf("You are reduced to an ordinary user by the manager.\n");
           		global_admin_flag =NORMAL_USER;//0普通用户  
           		show_menu();
           		continue;
    		}
    		else if(strcmp("self",buf)==0)
    		{
           		printf("You cannot mute yourself or promote yourself to manager or kick yourself out.\n");  
           		continue;
    		} 
  		printf("%s\n",buf);//输出聊天信息
		memset(buf,0,sizeof(buf)); // clear  buf
	}//while循环结尾
	close(socketfd);
	pthread_exit(NULL);
	exit(EXIT_SUCCESS);	
}

//打印错误信息
void print_err(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

//解析字符串，包装填充数据结构
void parse_input_buf(char *src, send_info *send)
{
	switch(src[0]){
		case ':' : 
		 	send->type=PUBLIC_CHAT;
		 	strcpy(send->buf,src+1);
			break;
		case '@' :
	    		strcpy(send->id,strtok(src+1,":"));//id 为用户名
			send->type=PRIVATE_CHAT; 
			strcpy(send->buf,strtok(NULL,":"));//代表聊天内容的字符串
		 	break;
  		case '-' :
       			if(strcmp(src,"--CheckOnline")==0)
          			send->type=CLIENT_ONLINE;  
       			else if(strcmp(src,"--help")==0)
          			show_menu();
  		 	break;
  		case '$':
		   	if(global_admin_flag==ADMIN_USER)//必须是管理员才可得到执行
		    	{
		    		char *tok_buf[2];
		     	  	tok_buf[0]=strtok(src,":");
		        	tok_buf[1]=strtok(NULL,":");
		       		//DEBUG("tok_buf[0] %s\n",tok_buf[0]);
		        	//DEBUG("tok_buf[1] %s\n",tok_buf[1]);
   		      		if(strcmp(tok_buf[0],"$Reducto")==0)
		              		send->type=ADMIN_KICK_CLIENT;
		        	else if(strcmp(tok_buf[0],"$Stupefy")==0)
                  			send->type=ADMIN_SHUTUP_CLIENT;  
            			else if(strcmp(tok_buf[0],"$Lumos")==0)
                  			send->type=ADVANCED_CLIENT_TO_ADMIN;  
            			else if(strcmp(tok_buf[0],"$Imperio")==0)
                 			send->type=DROP_CLIENT_TO_NORMAL;  
            			strcpy(send->id,tok_buf[1]);   
 		    	}
		   	break;		 
		default :
	   		send->type=0;
		 	strcpy(send->buf,src); 
		 	break;
	}
} 
//禁言后需等待的时间 处理函数,5分钟
void wait_minutes()
{
	sleep(5*60);
	globel_is_shutup =0;
}
//发送数据
void writedata()
{
	char buf[BUF_SIZE]={'\0'};
  	send_info send; 
 	while(1)
	{
		gets(buf); 
		parse_input_buf(buf, &send); //解析client输入的字符串，放到send_info send结构体中
		if(strcmp("--help",buf)==0) 
			continue;	
		if(!globel_is_shutup)//如果该用户没有被禁言？
    			write(socketfd, &send, sizeof(send));
    		else 
			wait_minutes ();//等待5分钟
	  	if(strcmp("exit",buf)==0) 
	  	{   
	  		close(socketfd);
	  	    	exit(EXIT_SUCCESS);
	  	}
		memset(buf,0,sizeof(buf));//每一回合都把buf清掉
	}
}

//忽略ctrl+c 键的处理函数
void signHandler(int signNo)
{
	DEBUG("singal:%d \n",signNo);
    	printf("Please enter 'exit' if you wanna leave group chat, thx.\n");
}

