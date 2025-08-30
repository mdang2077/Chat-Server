#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

static const int USERNAME_SIZE = 15;
static const int REACTION_SIZE = 15;
static const int MESSAGE_SIZE = 255;
static const int MAX_REACTIONS = 100;
static const int TIMESTAMP_SIZE = 19;

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
	char time_buffer[17];
	time_t now = time(NULL);
	struct tm *local_time = localtime(&now);
	snprintf(time_buffer, sizeof(time_buffer),
		"%04d-%02d-%02d %02d:%02d",
		local_time->tm_year + 1900,
		local_time->tm_mon + 1,
		local_time->tm_mday,
		local_time->tm_hour,
		local_time->tm_min
		);
	char* new_timestamp = malloc(17);
	strcpy(new_timestamp, time_buffer);
	Reaction* reactions = malloc(sizeof(Reaction) * REACTION_SIZE);
	struct Chat new_chat = { id, thisusername, thismessage, new_timestamp, 0, reactions };
	return new_chat;
}
uint8_t add_chat(char* username, char* message)
{
	global_id++;
	Chat add = new_chat(username, message);
	// Realloc for chats here
	chats = realloc(chats, sizeof(Chat) * (global_id + 1));
	chats[global_id] = add;
	return 1;
}

uint8_t add_reaction(char* username, char* message, char* id)
{
	uint64_t usernameSize = strlen(username);
	uint64_t messageSize = strlen(message);
	char* thisusername = malloc(usernameSize + 1);
	char* thismessage = malloc(messageSize + 1);
	strcpy(thisusername, username);
	strcpy(thismessage, message);
	int thisID = atoi(id);
	struct Reaction new_reaction = { thisusername, thismessage };

	chats[thisID].reactions[chats[thisID].num_reactions] = new_reaction;
	chats[thisID].num_reactions += 1;
	return 1;
}

void respond_with_chats(int client)
{
	for (int i = 1; i <= global_id; i++)
	{
		printf("#%d %s	%s: %s\n", chats[i].id, chats[i].timestamp, chats[i].username, chats[i].message);
	}
}

///post?user=MDANG&message=URMOM
void handle_post(char* path, int client_socket)
{
	char username[USERNAME_SIZE + 1];
	char message[MESSAGE_SIZE + 1]; /*
	int result = sscanf(path, "/post?user=%15[^&]&message=%255s", username, message);
	printf("%s %s\n", username, message); */

	int token_count = 0;
	char* token;
	char delimeters[] = "=&";
	int id = 0;
	token = strtok(path, delimeters);

	while (token != NULL) {
		token_count++;
		if (token_count == 2)
		{
			strcpy(username, token);
		}
		if (token_count == 4)
		{
			strcpy(message, token);
		}
		token = strtok(NULL, delimeters);
	}
	printf("%s, %s\n", username, message);
}
//[#N 20XX-MM-DD HH:MM]   <username>: <message>
void test_add_chat() {
	add_chat("MDANG", "Testing");
	printf("#%d %s	%s: %s\n", chats[1].id, chats[1].timestamp, chats[1].username, chats[1].message);
}

void test_add_reaction() {
	add_reaction("Bob", "Reaction", "1");
	printf("Reaction: %s %s\n", chats[1].reactions[0].user, chats[1].reactions[0].message);
}

void test_handle_post() {
	char path[] = "/post?user=MDANG&message=URMOM";
	handle_post(path, 2);
}

const int testing = 1;
int main(int argc, char** argv)
{
	if (testing) {
		test_add_chat();
		test_add_reaction();
		respond_with_chats(3);
		test_handle_post();
	}
}

