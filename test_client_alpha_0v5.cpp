#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <iostream>
#include <cstring>
#include <ctime>
using namespace std;

#define MAX_BUFFER_SIZE 1000

#define ADR_IPv4		0xf0
#define ADR_IPv6		0xf1
#define ADR_HOSTNAME	0xf2

#define MSG_PING                  0x01
#define MSG_PONG                  0x02
#define MSGPONG_REQ_REG           0x03
#define MSG_STREAM_ADVERTISEMENT  0x04
#define MSG_STREAM_REGISTERED     0x05
#define MSG_IDENTIFIER_NOT_USABLE 0x06
#define MSG_FIND_STREAM_SOURCE    0x07
#define MSG_STREAM_SOURCE_DATA    0x08
#define MSG_STREAM_REMOVE         0x09
#define MSG_MULTIMEDIA            0x0a
#define MSG_REQUEST_STREAMING     0x0b
#define MSG_FORWARD_PLAYER_READY  0x0c
#define MSG_PLAYER_READY          0x0d
#define MSG_SOURCE_READY          0x0e
#define MSG_REQ_RELAY_LIST        0x0f
#define MSG_RELAY_LIST            0x10
#define MSG_SHUTTING_DOWN         0x11
#define MSG_PLEASE_FORWARD        0x12
#define MSG_REGISTER_FORWARDING   0x13

unsigned char msg_sa[] = {
	MSG_STREAM_ADVERTISEMENT,
	0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71, 0x72,
	ADR_IPv4, 127, 168, 5, 23, 0x05, 0x20,
	0x00
};
unsigned char msg_sa2[] = {
	MSG_STREAM_ADVERTISEMENT,
	0x65, 0x66, 0x60, 0x68, 0x69, 0x70, 0x71, 0x10,
	ADR_IPv4, 127, 168, 5, 23, 0x05, 0x20,
	0x00
};

unsigned char msg_sr[] = {
	MSG_STREAM_REMOVE,
	0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71, 0x72
};
unsigned char msg_sr2[] = {
	MSG_STREAM_REMOVE,
	0x65, 0x66, 0x60, 0x68, 0x69, 0x70, 0x71, 0x10
};

int main() {
	srand(time(NULL) );
	rand();
	
	int er = rand()%42142;
	cout << er << endl;
	
	int sock_id = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock_id<0) { cout << "Error (1)!" << endl; exit(0); }
	
	sockaddr_in server_addr;
	socklen_t server_addr_len;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(1234);
	//server_addr.sin_addr.s_addr = inet_addr("192.168.1.44");
	//server_addr.sin_addr.s_addr = inet_addr("127.0.0.0");
	server_addr.sin_addr.s_addr = inet_addr("127.0.20.12");
	
	server_addr_len = sizeof(server_addr);
	
	unsigned char buffer[MAX_BUFFER_SIZE];
	string str1;
	
	// lokalno spajanje ili globalno ??
	while(1) {
		
		
		string x;
		cout << "dalje.. (q-izlaz)" << endl;
		cin >> x;
		if(x=="q") break;
		
		int sz;
		
		for(int i=0; i<1; i++) {
			strcpy((char*)buffer, (char*)msg_sa);
			
			sz = sendto(sock_id, (void*)&buffer, strlen((char*)buffer), 0, (sockaddr*)&server_addr, server_addr_len);
			cout << "saljem: [";
			for(int i=0; i<strlen((char*)buffer); i++)
				cout << (int)buffer[i] << " ";
			cout << "] ..." << endl;
			
			strcpy((char*)buffer, (char*)msg_sa2);
			
			sz = sendto(sock_id, (void*)&buffer, strlen((char*)buffer), 0, (sockaddr*)&server_addr, server_addr_len);
			cout << "saljem: [";
			for(int i=0; i<strlen((char*)buffer); i++)
				cout << (int)buffer[i] << " ";
			cout << "] ..." << endl;
			
			strcpy((char*)buffer, (char*)msg_sa);
			
			sz = sendto(sock_id, (void*)&buffer, strlen((char*)buffer), 0, (sockaddr*)&server_addr, server_addr_len);
			cout << "saljem: [";
			for(int i=0; i<strlen((char*)buffer); i++)
				cout << (int)buffer[i] << " ";
			cout << "] ..." << endl;
			
			usleep(100);
		}
		
		char r;
		cout << "prihvatiti poruku? " << endl;
		cin >> r;
		
		if(r=='d') {
			sz = recvfrom(sock_id, (void*)&buffer, MAX_BUFFER_SIZE, 0, (sockaddr*)&server_addr, &server_addr_len);
			if(sz>0) {
				cout << "primljeno: [";
				for(int i=0; i<strlen((char*)buffer); i++)
					cout << (int)buffer[i] << " ";
				cout << "] ..." << endl;
			}
		}
	}
	close(sock_id);
	return 0;
}
