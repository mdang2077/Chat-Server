#include "http-server.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

char const* HTTP_200_OK = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n";
char const* HTTP_404 = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n";
char const* HTTP_500 = "HTTP/1.1 500 Internal Server\r\nContent-Type: text/plain\r\n\r\n";
char const* HTTP_400 = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\n";

static const int USERNAME_SIZE = 15;
static const int REACTION_SIZE = 15;
static const int MESSAGE_SIZE = 255;
static const int MAX_REACTIONS = 100;
static const int TIMESTAMP_SIZE = 20;
static const int CHAT_LIMIT = 100000;

int global_id = 0;

struct Reaction {
    char* user;
    char* message;
};
typedef struct Reaction Reaction;

struct Chat {
    uint32_t id;
    char* username;
    char* message;
    char* timestamp;
    uint32_t num_reactions;
    Reaction* reactions;
};
typedef struct Chat Chat;

uint8_t hex_to_byte(char c) {
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	return -1;
}

void url_decode(char *src, char *dest){
	int i = 0;
	char *dest_ptr = dest;
	while (*(src+i) != 0){
		if(*(src+i)=='%'){
			uint8_t a = hex_to_byte(*(src+i+1));
		    uint8_t b = hex_to_byte(*(src+i+2));
			*dest_ptr=(unsigned char)((a<< 4)| b);
			dest_ptr++;
			i +=2; 
		} else {
			*dest_ptr = *(src+i);
			dest_ptr++;

		}	
		i++;
	}
	*dest_ptr = 0;
}

Chat* chats = NULL;

Chat new_chat(char* username, char* message)
{
	int id = global_id;
	uint64_t usernameSize = strlen(username);
	uint64_t messageSize = strlen(message);
	char* thisusername = malloc(usernameSize + 1);
	char* thismessage = malloc(messageSize + 1);
	strcpy(thisusername, username);
	strcpy(thismessage, message);
	char time_buffer[20];
	time_t now = time(NULL);
	struct tm *local_time = localtime(&now);
	snprintf(time_buffer, sizeof(time_buffer),
		"%04d-%02d-%02d %02d:%02d:%02d",
		local_time->tm_year + 1900,
		local_time->tm_mon + 1,
		local_time->tm_mday,
		local_time->tm_hour,
		local_time->tm_min,
		local_time->tm_sec
		);
	char* new_timestamp = malloc(20);
	strcpy(new_timestamp, time_buffer);
	Reaction* reactions = NULL;
	struct Chat new_chat = { id, thisusername, thismessage, new_timestamp, 0, reactions };
	return new_chat;
}
uint8_t add_chat(char* username, char* message)
{
	global_id += 1;
	Chat add = new_chat(username, message);
	// Realloc for chats here
	chats = realloc(chats, sizeof(Chat) * (global_id + 1));
	chats[global_id] = add;
	return 1;
}

void add_reaction(char* username, char* message, char* id, int client_socket)
{
	int thisID = atoi(id);
	uint64_t usernameSize = strlen(username);
	uint64_t messageSize = strlen(message);
	char* thisusername = malloc(usernameSize + 1);
	char* thismessage = malloc(messageSize + 1);
	strcpy(thisusername, username);
	strcpy(thismessage, message);
	struct Reaction new_reaction = { thisusername, thismessage };

	chats[thisID].num_reactions += 1;
	chats[thisID].reactions = realloc(chats[thisID].reactions, sizeof(Reaction) * (chats[thisID].num_reactions + 1));
	chats[thisID].reactions[chats[thisID].num_reactions] = new_reaction;
}

void respond_with_chats(int client)
{
	char string[350];
	//char response[256];
	//strcpy(response, HTTP_200_OK);
	write(client, HTTP_200_OK, strlen(HTTP_200_OK));
	for (int i = 1; i <= global_id; i++)
	{
		sprintf(string, "[#%d %s]\t%s: %s\n", chats[i].id, chats[i].timestamp, chats[i].username, chats[i].message);
		//strcat(buffer, string);
		write(client, string, strlen(string));
		for (int j = 1; j <= chats[i].num_reactions; j++)
		{
			sprintf(string, "\t\t\t(%s)  %s\n", chats[i].reactions[j].user, chats[i].reactions[j].message);
			write(client, string, strlen(string));
		}

	}
//	write(client, buffer, strlen(buffer));
}

void handle_chats(int client_socket)
{
	respond_with_chats(client_socket);
}
// /post?user=<username>&message=<message>
void handle_post(char* path, int client_socket)
{
	if (global_id  >= 100000)
	{
		write(client_socket, HTTP_400, strlen(HTTP_400));
		return;
	}	
	char username[USERNAME_SIZE * 4];
	char message[MESSAGE_SIZE * 4];

	int result = sscanf(path, "/post?user=%59[^&]&message=%1019s", username, message);

	if (strlen(message) == 0 || strlen(username) == 0 || result < 2)
	{
		write(client_socket, HTTP_400, strlen(HTTP_400));
		return;
	} 

	char decoded_username[USERNAME_SIZE * 4];
	char decoded_message[MESSAGE_SIZE * 4];
	url_decode(username, decoded_username);
	url_decode(message, decoded_message);

	if (strlen(decoded_username) == 0 || strlen(decoded_message) == 0 || strlen(decoded_username) >= 16 || strlen(decoded_message) >= 256)
	{
		write(client_socket, HTTP_400, strlen(HTTP_400));
		return;
	} 

	add_chat(decoded_username, decoded_message);
	respond_with_chats(client_socket);
	memset(username, 0, sizeof(username));
	memset(message, 0, sizeof(message));
	memset(decoded_username, 0, sizeof(decoded_username));
	memset(decoded_message, 0, sizeof(decoded_message));
}

void handle_edit(char* path, int client_socket)
{
	char message[MESSAGE_SIZE * 4];
	char id[12];
	int result = sscanf(path, "/edit?id=%11[^&]&message=%1019s", id, message);
	int IDnum = atoi(id);

	if (IDnum > global_id || IDnum <= 0 || strlen(id) == 0 || result < 2)
	{
		write(client_socket, HTTP_400, strlen(HTTP_400));
		return;
	}
	char decoded_message[MESSAGE_SIZE * 4];
	url_decode(message, decoded_message);

	if (strlen(decoded_message) == 0 || strlen(decoded_message) > 255)
	{
		write(client_socket, HTTP_400, strlen(HTTP_400));
		return;
	}
	free(chats[IDnum].message);
	chats[IDnum].message = malloc(strlen(decoded_message) + 1);
	strncpy(chats[IDnum].message, decoded_message, strlen(decoded_message));
	chats[IDnum].message[strlen(decoded_message)] = 0;
	
	respond_with_chats(client_socket);

}

// /react?user=<username>&message=<reaction>&id=<id>
void handle_reaction(char* path, int client_socket)
{
	char username[USERNAME_SIZE * 4];
	char reaction[REACTION_SIZE * 4];
	char id[12];
	int result = sscanf(path, "/react?user=%59[^&]&message=%59[^&]&id=%11s", username, reaction, id);
	if (result < 3) {
		write(client_socket, HTTP_400, strlen(HTTP_400));
		return;
	}
	int IDnum = atoi(id);
	if (IDnum > global_id || IDnum <= 0)
	{
		write(client_socket, HTTP_400, strlen(HTTP_400));
		return;
	}
	if (chats[IDnum].num_reactions >= 100)
	{
		write(client_socket, HTTP_400, strlen(HTTP_400));
		return;
	}
	char decoded_username[USERNAME_SIZE * 4];
	char decoded_reaction[REACTION_SIZE * 4];
	url_decode(username, decoded_username);
	url_decode(reaction, decoded_reaction);

	if (strlen(decoded_username) == 0 || strlen(decoded_reaction) == 0 || strlen(decoded_username) >= 16 || strlen(decoded_reaction) >= 16)	
	{		
		write(client_socket, HTTP_400, strlen(HTTP_400));
		return;
	}


	add_reaction(decoded_username, decoded_reaction, id, client_socket);
	respond_with_chats(client_socket);

	memset(username, 0, sizeof(username));
	memset(reaction, 0, sizeof(reaction));
	memset(id, 0, sizeof(id));
	memset(decoded_username, 0, sizeof(decoded_username));
	memset(decoded_reaction, 0, sizeof(decoded_reaction));
}

void handle_reset(char* path, int client_socket)
{
	for (int i = 1; i <= global_id; i++)                                                                                                                                
	{                                                                                                                                                                   
		free(chats[i].username);                                                                                                                                          
		free(chats[i].message);                                                                                                                                           
		free(chats[i].timestamp);                                                                                                                                         
		for (int j = 1; j <= (int)chats[i].num_reactions; j++)                                                                                                            
		{                                                                                                                                                                 
			free(chats[i].reactions[j].user);                                                                                                                               
			free(chats[i].reactions[j].message);                                                                                                                            
		}                                                                                                                                                                 
		free(chats[i].reactions);                                                                                                                                         
	}
	free(chats);
	chats = NULL;
	global_id = 0;
	write(client_socket, HTTP_200_OK, strlen(HTTP_200_OK));
}

void handle_response(char *request, int client_socket) {
	char path[1001];

	printf("Got request \"%s\"\n", request);
	if (sscanf(request, "GET %999s", path) != 1)
	{
		printf("Invalid request line\n");
		return;
	}
	if (strncmp(path, "/chats", 6) == 0)
	{
		handle_chats(client_socket);
	}
    else if (strncmp(path, "/post", 5) == 0)
	{
		handle_post(path, client_socket);
	}
    else if (strncmp(path, "/react", 6) == 0)
	{
		handle_reaction(path, client_socket);
	}
    else if (strncmp(path, "/reset", 6) == 0)
	{
		handle_reset(path, client_socket);
	}
	else if (strncmp(path, "/edit", 5) == 0)
	{
		handle_edit(path, client_socket);
	}
	else
	{
		write(client_socket, HTTP_404, strlen(HTTP_404));
		write(client_socket, "Error", 5);
	}
}


int main(int argc, char** argv)
{
	int port = 0;
	if (argc >= 2)
	{
		port = atoi(argv[1]);
	}
	start_server(&handle_response, port);
}
