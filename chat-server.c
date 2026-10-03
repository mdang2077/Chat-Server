#include "http-server.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

char const* HTTP_200_OK = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n";
char const* HTTP_400 = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\n";
char const* HTTP_404 = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n";
char const* HTTP_500 = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\n";

enum {
	USERNAME_SIZE = 15,
	REACTION_SIZE = 15,
	MESSAGE_SIZE = 255,
	MAX_REACTIONS = 100,
	CHAT_LIMIT = 100000,
	TIMESTAMP_SIZE = 20,  // "YYYY-MM-DD HH:MM:SS" plus terminator
	ID_SIZE = 12,
	// Raw query values may be percent-encoded, so allow room for "%XX" escapes.
	RAW_FACTOR = 4,
};

typedef struct {
	char* user;
	char* message;
} Reaction;

typedef struct {
	uint32_t id;
	char* username;
	char* message;
	char timestamp[TIMESTAMP_SIZE];
	uint32_t num_reactions;
	Reaction* reactions;
} Chat;

// Chat IDs start at 1, so chats[0] is unused and chats[id] is the chat with that ID.
static Chat* chats = NULL;
static int num_chats = 0;

static void respond(int client, char const* response)
{
	write(client, response, strlen(response));
}

static char* copy_string(char const* src)
{
	size_t len = strlen(src);
	char* dest = malloc(len + 1);
	if (dest != NULL) {
		memcpy(dest, src, len + 1);
	}
	return dest;
}

static int hex_to_value(char c)
{
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	return -1;
}

// Decodes %XX escapes. A '%' not followed by two hex digits is kept as-is.
// dest must be at least as large as src.
static void url_decode(char const* src, char* dest)
{
	while (*src != '\0') {
		if (src[0] == '%') {
			int hi = hex_to_value(src[1]);
			int lo = hi < 0 ? -1 : hex_to_value(src[2]);
			if (hi >= 0 && lo >= 0) {
				*dest++ = (char)((hi << 4) | lo);
				src += 3;
				continue;
			}
		}
		*dest++ = *src++;
	}
	*dest = '\0';
}

static int valid_length(char const* s, size_t max)
{
	size_t len = strlen(s);
	return len > 0 && len <= max;
}

// Parses a chat ID, returning 0 if it is not a number or not an existing chat.
static int parse_chat_id(char const* s)
{
	char* end;
	long id = strtol(s, &end, 10);
	if (end == s || *end != '\0' || id < 1 || id > num_chats) {
		return 0;
	}
	return (int)id;
}

static int add_chat(char const* username, char const* message)
{
	Chat* grown = realloc(chats, sizeof(Chat) * (num_chats + 2));
	if (grown == NULL) {
		return 0;
	}
	chats = grown;

	Chat* chat = &chats[num_chats + 1];
	chat->username = copy_string(username);
	chat->message = copy_string(message);
	if (chat->username == NULL || chat->message == NULL) {
		free(chat->username);
		free(chat->message);
		return 0;
	}
	time_t now = time(NULL);
	strftime(chat->timestamp, sizeof(chat->timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
	chat->num_reactions = 0;
	chat->reactions = NULL;

	num_chats++;
	chat->id = num_chats;
	return 1;
}

static int add_reaction(int id, char const* username, char const* message)
{
	Chat* chat = &chats[id];
	Reaction* grown = realloc(chat->reactions, sizeof(Reaction) * (chat->num_reactions + 1));
	if (grown == NULL) {
		return 0;
	}
	chat->reactions = grown;

	Reaction* reaction = &chat->reactions[chat->num_reactions];
	reaction->user = copy_string(username);
	reaction->message = copy_string(message);
	if (reaction->user == NULL || reaction->message == NULL) {
		free(reaction->user);
		free(reaction->message);
		return 0;
	}
	chat->num_reactions++;
	return 1;
}

static void respond_with_chats(int client)
{
	char line[USERNAME_SIZE + MESSAGE_SIZE + TIMESTAMP_SIZE + 32];

	respond(client, HTTP_200_OK);
	for (int i = 1; i <= num_chats; i++) {
		Chat* chat = &chats[i];
		snprintf(line, sizeof(line), "[#%u %s]\t%s: %s\n",
			chat->id, chat->timestamp, chat->username, chat->message);
		respond(client, line);
		for (uint32_t j = 0; j < chat->num_reactions; j++) {
			snprintf(line, sizeof(line), "\t\t\t(%s)  %s\n",
				chat->reactions[j].user, chat->reactions[j].message);
			respond(client, line);
		}
	}
}

// /post?user=<username>&message=<message>
static void handle_post(char const* path, int client)
{
	char username[USERNAME_SIZE * RAW_FACTOR];
	char message[MESSAGE_SIZE * RAW_FACTOR];

	if (num_chats >= CHAT_LIMIT) {
		respond(client, HTTP_400);
		return;
	}
	if (sscanf(path, "/post?user=%59[^&]&message=%1019s", username, message) != 2) {
		respond(client, HTTP_400);
		return;
	}

	char decoded_username[sizeof(username)];
	char decoded_message[sizeof(message)];
	url_decode(username, decoded_username);
	url_decode(message, decoded_message);

	if (!valid_length(decoded_username, USERNAME_SIZE) || !valid_length(decoded_message, MESSAGE_SIZE)) {
		respond(client, HTTP_400);
		return;
	}
	if (!add_chat(decoded_username, decoded_message)) {
		respond(client, HTTP_500);
		return;
	}
	respond_with_chats(client);
}

// /react?user=<username>&message=<reaction>&id=<id>
static void handle_reaction(char const* path, int client)
{
	char username[USERNAME_SIZE * RAW_FACTOR];
	char reaction[REACTION_SIZE * RAW_FACTOR];
	char id_str[ID_SIZE];

	if (sscanf(path, "/react?user=%59[^&]&message=%59[^&]&id=%11s", username, reaction, id_str) != 3) {
		respond(client, HTTP_400);
		return;
	}
	int id = parse_chat_id(id_str);
	if (id == 0 || chats[id].num_reactions >= MAX_REACTIONS) {
		respond(client, HTTP_400);
		return;
	}

	char decoded_username[sizeof(username)];
	char decoded_reaction[sizeof(reaction)];
	url_decode(username, decoded_username);
	url_decode(reaction, decoded_reaction);

	if (!valid_length(decoded_username, USERNAME_SIZE) || !valid_length(decoded_reaction, REACTION_SIZE)) {
		respond(client, HTTP_400);
		return;
	}
	if (!add_reaction(id, decoded_username, decoded_reaction)) {
		respond(client, HTTP_500);
		return;
	}
	respond_with_chats(client);
}

// /edit?id=<id>&message=<message>
static void handle_edit(char const* path, int client)
{
	char id_str[ID_SIZE];
	char message[MESSAGE_SIZE * RAW_FACTOR];

	if (sscanf(path, "/edit?id=%11[^&]&message=%1019s", id_str, message) != 2) {
		respond(client, HTTP_400);
		return;
	}
	int id = parse_chat_id(id_str);
	if (id == 0) {
		respond(client, HTTP_400);
		return;
	}

	char decoded_message[sizeof(message)];
	url_decode(message, decoded_message);
	if (!valid_length(decoded_message, MESSAGE_SIZE)) {
		respond(client, HTTP_400);
		return;
	}

	char* updated = copy_string(decoded_message);
	if (updated == NULL) {
		respond(client, HTTP_500);
		return;
	}
	free(chats[id].message);
	chats[id].message = updated;
	respond_with_chats(client);
}

static void handle_reset(int client)
{
	for (int i = 1; i <= num_chats; i++) {
		free(chats[i].username);
		free(chats[i].message);
		for (uint32_t j = 0; j < chats[i].num_reactions; j++) {
			free(chats[i].reactions[j].user);
			free(chats[i].reactions[j].message);
		}
		free(chats[i].reactions);
	}
	free(chats);
	chats = NULL;
	num_chats = 0;
	respond(client, HTTP_200_OK);
}

static void handle_request(char* request, int client)
{
	char path[1001];

	printf("Got request \"%s\"\n", request);
	if (sscanf(request, "GET %1000s", path) != 1) {
		respond(client, HTTP_400);
		return;
	}

	if (strncmp(path, "/chats", 6) == 0) {
		respond_with_chats(client);
	} else if (strncmp(path, "/post", 5) == 0) {
		handle_post(path, client);
	} else if (strncmp(path, "/react", 6) == 0) {
		handle_reaction(path, client);
	} else if (strncmp(path, "/edit", 5) == 0) {
		handle_edit(path, client);
	} else if (strncmp(path, "/reset", 6) == 0) {
		handle_reset(client);
	} else {
		respond(client, HTTP_404);
		respond(client, "Error");
	}
}

int main(int argc, char** argv)
{
	int port = 0;
	if (argc >= 2) {
		port = atoi(argv[1]);
	}
	start_server(&handle_request, port);
}
