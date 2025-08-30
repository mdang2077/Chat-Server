# Chat-Server
A basic HTTP server that handles chat messages and reactions.

# Data Structures
1. Post: Contains message ID, username, message content, timestamp, and reactions
2. Reaction: Stores user and message for reactions to chats

# Routes
http://localhost:<port>...
1. /chats
 A request which responds with the plain text rendering of all the current chats.

2. /post?user=<username>&message=<message>
This creates a new chat with a username and a message

3. /react?user=<username>&message=<reaction>&id=<id>
This creates a new reaction for an existing chat with a given message

4. /edit?id=<id>&message=<message>
Allows the user to edit an existing message in the chat server

5. /reset
Resets the chat server to have no chats or reactions

# How to use
1. Clone or download the repository
2. Open the folder
3. Run "make clean" and then "make" 
4. Run "./chat-server <port>" or "./chat-server"
5. Open a browser and enter "http://localhost:<port>/<request>