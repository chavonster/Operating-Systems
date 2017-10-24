//
// Created by marina92 on 6/12/17.
//

#ifndef EX5_WHATSAPPSERVER_H
#define EX5_WHATSAPPSERVER_H

#include <netinet/in.h>
#include <zconf.h>
#include <cstring>
#include <iostream>
#include <stdlib.h>
#include <libltdl/lt_system.h>
#include <arpa/inet.h>
#include <algorithm>
#include <vector>
#include <set>
#include <string>
#include <map>

#define FAILURE -1
#define SUCCESS 0
#define MAX_CONNECTIONS 10
#define MAX_SIZE_REQUEST 1024
#define CREATE_GROUP "create_group"
#define SEND "send"
#define WHO "who"
#define EXIT "exit"
#define NEW_CONNECTION "new_connection"


using namespace std;
template< class T, bool (*comp)( T const &, T const & ) >
/**
 * a generic comperator object for set
 */
class set_funcomp {
    struct ftor {
        bool operator()( T const &l, T const &r )
        { return comp( l, r ); }
    };
public:
    typedef std::set< T, ftor > t;
};

/**
 * defines a comperator for two string objects
 * @param l first var to be compared
 * @param r sedond var
 * @return lex ordering
 */
bool nameComparator( string const &l, string const &r )
{
    return (bool) (l.compare(r));
}

typedef set_funcomp< string, nameComparator>::t GROUP_SET;
typedef pair<int, string> CLIENT_PAIR;


struct clientsComp
{
    inline bool operator() (const CLIENT_PAIR& pair1, const CLIENT_PAIR& pair2)
    {
        return (pair1.second < pair2.second);
    }
};

/**
 * Defines a server object
 */
class Server
{
private:
    vector<pair<int, string>> clients_vec;
    map<string, GROUP_SET> groups_map;

public:
    /**
     * constructor
     * @return
     */
    Server();

    /**
     * Established the server bind address to socket and listen
     * @param port_num
     * @return
     */
    int establishServer(unsigned short port_num);

    /**
     * run the server waiting for requests
     * @param socket_id
     * @return
     */
    int runServer(int socket_id);

    /**
     * reads a new request if it is OK calls parser
     * @param client_socket_fd
     * @return
     */
    int readRequest(int client_socket_fd);

    /**
     * Parses requests chooses the compatible handler
     * @param client_socket_fd
     * @param buff
     * @return
     */
    int parseRequest(int client_socket_fd, char* buff);

    /**
     * handles create a new group request
     * @param client_socket_fd
     * @param msg_str
     * @return
     */
    int handleGroupReq(int client_socket_fd, string msg_str);

    /**
     * handles a send request
     * @param client_socket_fd
     * @param msg_str
     * @return
     */
    int handleSendReq(int client_socket_fd, string msg_str);

    /**
     * handles who request
     * @param client_socket_fd
     * @param msg_str
     * @return
     */
    int handleWhoReq(int client_socket_fd, string msg_str);

    /**
     * handles exit request
     * @param client_socket_fd
     * @param msg_str
     * @return
     */
    int handleExitReq(int client_socket_fd, string msg_str);

    /**
     * handles new connection from a new client
     * @param client_socket_fd
     * @param msg_str
     * @return
     */
    int handleNewConnect(int client_socket_fd,string msg_str);

    /**
     * get the username by socket
     * @param socket
     * @return
     */
    string getUsername(int socket);

    /**
     * get the socket id by username
     * @param name
     * @return
     */
    int getSocketId(string name);

    /**
     * checks if a certain username exists
     * @param name
     * @return
     */
    bool doesUsernameExist(string name);

    /**
     * removes username from all the groups he is a member of
     * @param username
     */
    void removeUserFromAllGroup(string username);

    /**
     * Removes user from the clients list on the server
     * @param username
     */
    void removeUserFromClients(string username);
};

#endif //EX5_WHATSAPPSERVER_H
