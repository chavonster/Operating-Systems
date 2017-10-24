#include "whatsappClient.h"
#include <regex>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include <libltdl/lt_system.h>
#include <zconf.h>
#include <poll.h>

using namespace std;

/**
 * Checks the validity of the name of the client and the group's nams- that it
 * contains onlu letters and digits.
 * @param to_check
 * @return
 */
bool Client::checkValidityStr(string to_check)
{
    return (regex_match(to_check, regex(LETTERS_DIGITS)));
}

Client::Client(char* name, char* host, int port)
{
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    portno = port;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        handleSysError("socket");
    }
    socket_client = sockfd;
    server = gethostbyname(host);
    if (server == NULL) {
        handleSysError("host");
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(socket_client,(struct sockaddr *)&serv_addr,sizeof(serv_addr))
        < 0)
    {
        handleSysError("connect");
    }
    strncpy(client_name, name, NAME_LEN);
    string name_str(client_name);
    if(!checkValidityStr(name_str))
    {
        cerr << "ERROR: Invalid name." << endl;
        exit(EXIT_FAILURE);
    }
    string msg = "new_connection " + string(client_name);
    send(sockfd, (void*)msg.c_str(), msg.length(), 0);
    char buff[MAX_MSG];
    int bytes = recv(sockfd, buff, MAX_MSG, 0);
    if(bytes <= 0)
    {
        cout << "Failed to connect the server" << endl;
        exit(EXIT_FAILURE);
    }
    string buff_str(buff);
    buff_str = buff_str.substr(0, bytes);
    char* alignedBuff = const_cast<char*>(buff_str.c_str());
    cout << alignedBuff << endl;
    if(buff_str.compare(NAME_IN_USE) == 0)
    {
        exit(EXIT_FAILURE);
    }
}

void Client::handleSysError(string syscall)
{
    cerr << "ERROR: " << syscall << " " << errno << endl;
    exit(EXIT_FAILURE);
}

void Client::establishClient()
{
    struct pollfd poll_list[2];
    int retval;
    poll_list[0].fd = STDIN_FILENO;
    poll_list[1].fd = this->socket_client;
    poll_list[0].events = POLLIN|POLLPRI;
    poll_list[1].events = POLLIN|POLLPRI;
    char buf[MAX_MSG];
    while(true)
    {
        bzero(buf, sizeof(buf));
        retval = poll(poll_list,(unsigned long)2,-1);
        if(retval < 0)
        {
            handleSysError("poll");
        }
        if((poll_list[0].revents&POLLIN) == POLLIN)
        {
            read(poll_list[0].fd, buf, MAX_MSG);
            createRequest(buf);
        }
        if((poll_list[1].revents&POLLIN) == POLLIN)
        {
            read(poll_list[1].fd, buf, MAX_MSG);
            string buff_str(buf);
            if(!buff_str.compare("exit server"))
            {
                exit(EXIT_FAILURE);
            }
            cout << buf << endl;
        }
    }
}

int Client::createRequest(char* request)
{
    char msg[MAX_MSG];
    strcpy(msg, request);
    string msg_str(msg);
    string delimiter = " ";
    int pos = msg_str.find(delimiter);
    if(pos == NOT_FOUND)
    {
        delimiter = "\n";
        pos = msg_str.find(delimiter);
    }
    string command = msg_str.substr(0, pos);
    int result = 0;
    if(!command.compare(CREATE_GROUP))
    {
        result = handleGroupCom(msg_str);
    }
    else if (!command.compare(SEND))
    {
        result = handleSendCom(msg_str);
    }
    else if(!command.compare(WHO))
    {
        result = handleWhoCom(msg_str);
    }
    else if (!command.compare(EXIT))
    {
        result = handleExitCom(msg_str);
    }
    else
    {
        cerr << ERR_INPUT << endl;
    }
    if(result == SUCCESS)
    {
        return SUCCESS;
    }
    return FAILURE;
}

int Client::handleGroupCom(string msg_str)
{
    string group_name = "";
    bool res = regex_match(msg_str,
                           regex("^create_group \\w+ \\w+(,\\w+)*\\s*"));
    string delimiter = " ";
    string cpy_msg(msg_str);
    int pos1 = cpy_msg.find(delimiter);
    int pos2 = cpy_msg.find(delimiter, pos1+1);
    if(pos1 != NOT_FOUND)
    {
        if(pos2 == NOT_FOUND)
        {
            string delimiter = "\n";
            pos2 = cpy_msg.find(delimiter, pos1+1);
        }
        group_name = cpy_msg.substr(pos1+1, pos2-pos1 -1);
    }
    if(!res)
    {
        cerr << "Error: failed to create group \"" << group_name << "\"." <<
                                                                          endl;
        return FAILURE;
    }

    if (!checkValidityStr(group_name))
    {
        cerr << "ERROR: Invalid group name." << endl;
        return FAILURE;
    }
    if (send(this->socket_client, (void*)msg_str.c_str(), msg_str.length(), 0)
        < 0) {
        handleSysError("write");
    }
    char buf[MAX_MSG];
    int bytes;
    if ((bytes = recv(this->socket_client, buf, MAX_MSG, 0)) < 0)
    {
        handleSysError("read");
    }
    string buff_str(buf);
    buff_str = buff_str.substr(0, bytes);
    char* alignedBuff = const_cast<char*>(buff_str.c_str());
    cout << alignedBuff << endl;
    return(SUCCESS);
}

int Client::handleSendCom(string msg_str)
{
    bool res = regex_match(msg_str, regex("^send \\w+ \\w.*\\s*"));
    if(!res)
    {
        cerr << ERR_SEND << endl;
        return FAILURE;
    }
    string delimiter = " ";
    int pos1 = msg_str.find(delimiter);
    int pos2 = msg_str.find(delimiter, pos1+1);
    string receiver = msg_str.substr(pos1+1, pos2-pos1 -1);
    string name_str(client_name);
    //the use can't send a message to himself
    if(name_str.compare(receiver) == 0)
    {
        cerr << ERR_SEND << endl;
        return FAILURE;
    }
    if (send(this->socket_client, (void*)msg_str.c_str(), msg_str.length(), 0)
        < 0)
    {
        handleSysError("write");
    }
    char buf[MAX_MSG];
    int bytes;
    if ((bytes = recv(this->socket_client, buf, MAX_MSG, 0)) < 0)
    {
        handleSysError("read");
    }
    string buff_str(buf);
    buff_str = buff_str.substr(0, bytes);
    char* alignedBuff = const_cast<char*>(buff_str.c_str());
    cout << alignedBuff << endl;
    return(SUCCESS);
}

int Client::handleWhoCom(string msg_str)
{
    if(!(msg_str.compare("who\n") == 0 || msg_str.compare("who") == 0))
    {
        cerr << "ERROR: failed to receive list of connected clients." << endl;
        return 0;
    }
    msg_str = msg_str.substr(0,3);
    if (send(this->socket_client, (void*)msg_str.c_str(), msg_str.length(), 0)
        < 0)
    {
        handleSysError("write");
    }
    char buf[MAX_MSG];
    int bytes;
    if ((bytes = recv(this->socket_client, buf, MAX_MSG, 0)) < 0)
    {
        cerr << "ERROR: failed to receive list of connected clients" << endl;
        handleSysError("read");
    }
    string buff_str(buf);
    buff_str = buff_str.substr(0, bytes);
    char* alignedBuff = const_cast<char*>(buff_str.c_str());
    cout << alignedBuff << endl;
    return(SUCCESS);
}

int Client::handleExitCom(string msg_str)
{
    if(!(msg_str.compare("who\n") == 0 || msg_str.compare("who") == 0))
    {
        cerr << ERR_INPUT << endl;
        return 0;
    }
    if (send(this->socket_client,(void*)msg_str.c_str(), msg_str.length(), 0)
        < 0) {
        handleSysError("write");
    }
    char buf[MAX_MSG];
    int bytes;
    if ((bytes = recv(this->socket_client, buf, MAX_MSG, 0)) < 0)
    {
        handleSysError("read");
    }
    string buff_str(buf);
    buff_str = buff_str.substr(0, bytes);
    char* alignedBuff = const_cast<char*>(buff_str.c_str());
    cout << alignedBuff << endl;
    exit(EXIT_SUCCESS);
}

/**
 * The main of the client, geta an arguments from the user, creates the client,
 * and runs the programm.
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[])
{
    if (argc != NUM_OF_ARGS)
    {
        cout << "Usage: whatsappClient clientName ServerAddress serverPort" <<
             endl;
        exit(EXIT_FAILURE);
    }
    char* name = argv[1];
    char* host = argv[2];
    int port = atoi(argv[3]);
    Client client(name, host, port);
    client.establishClient();
    return SUCCESS;
}