#ifndef EX5_WHATSAPPCLIENT_H
#define EX5_WHATSAPPCLIENT_H
#include <string>
#define FAILURE -1
#define SUCCESS 0
#define MAX_MSG 1024
#define NAME_LEN 30
#define NOT_FOUND -1
#define NUM_OF_ARGS 4
#define LETTERS_DIGITS "^[A-Za-z0-9]+$"
#define CREATE_GROUP "create_group"
#define SEND "send"
#define WHO "who"
#define EXIT "exit"
#define NAME_IN_USE "Client name is already in use.\n"
#define ERR_SEND "Error: failed to send."
#define ERR_INPUT "ERROR: Invalid input."

using namespace std;

class Client
{
private:
    char client_name[30];
    int socket_client;
    bool checkValidityStr(string to_check);
public:
    /**
     * Constructor of the client class, gets the parameters:
     * @param name
     * @param host
     * @param port
     * @return
     */
    Client(char* name, char* host, int port);
    /**
     * Prints to cerr the system call, the arror umber, and exits.
     * @param syscall
     */
    void handleSysError(string syscall);
    /**
     * Waits for input from the server or from the user and handeles it.
     */
    void establishClient();
    /**
     * Creates request to the srver- supports the commands who, create_group,
     * send and exit.
     * @param request
     */
    int createRequest(char* request);
    /**
     * Sends to the server the request to create a group- with the group name
     * and the requested members.
     * @param msg_str
     * @return 0 if was a success, else -1.
     */
    int handleGroupCom(string msg_str);
    /**
     * Sends to the server the request to send amassege to existed group or to
     * another client.
     * @param msg_str
     * @return 0 if was a success, else -1.
     */
    int handleSendCom(string msg_str);
    /**
     * Sends to the server the request to get a list of the connected clients.
     * @param msg_str
     * @return 0 if was a success, else -1.
     */
    int handleWhoCom(string msg_str);
    /**
     * Sends to the server the request to exit from the whatsapp.
     * @param msg_str
     * @return
     */
    int handleExitCom(string msg_str);
};

#endif //EX5_WHATSAPPCLIENT_H
