#include "whatsappServer.h"

#define MAX_CLIENTS 30
#define CLOSE_REQUEST 2

using namespace std;

int Server::establishServer(unsigned short port_num) {
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        fprintf(stderr, "ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = port_num;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "ERROR on binding %s", strerror(errno));
    }
    listen(sockfd, MAX_CONNECTIONS);
    return sockfd;

    }

int Server::runServer(int socket_id) {

    socklen_t addlen;
    string buf;
    //char buffer[SIZE_OF_MSG];
    int master_socket = socket_id, new_socket, client_socket[MAX_CLIENTS],
            activity, i, sd, max_sd;

    struct sockaddr_in address;
    fd_set clientsfds;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        client_socket[i] = 0;
    }

    while (true)
    {
        //clear the socket set
        FD_ZERO(&clientsfds);
        //add master socket to set
        FD_SET(master_socket, &clientsfds);
        //add input socket
        FD_SET(STDIN_FILENO, &clientsfds);
        max_sd = max(master_socket, STDIN_FILENO);

        //add child sockets to set
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            //socket descriptor
            sd = client_socket[i];
            //if valid socket descriptor then add to read list
            if (sd > 0)
            {
                FD_SET(sd, &clientsfds);
            }
            //highest file descriptor number, need it for the select function
            if (sd > max_sd)
            {
                max_sd = sd;
            }
        }
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select(max_sd + 1, &clientsfds, NULL, NULL, NULL);


        if ((activity < 0) && (errno != EINTR))
        {
            printf("Error with select");
        }
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &clientsfds))
        {
            if ((new_socket = accept(master_socket, (struct sockaddr*)&address, &addlen)) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            if (readRequest(new_socket))
            {
                perror("send");
            }

            //add new socket to array of sockets
            for (i = 0; i < MAX_CLIENTS; i++)
            {
                //if position is empty
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }

        if (FD_ISSET(STDIN_FILENO, &clientsfds))
        {
            char buff[MAX_SIZE_REQUEST];
            int bytes_read = read(STDIN_FILENO, buff, MAX_SIZE_REQUEST);
            if (bytes_read  <= FAILURE)
            {
                cout << "ERROR: read " << strerror(errno) << "." << endl;
                return FAILURE;
            }
            string buff_str(buff);
            buff_str = buff_str.substr(0, bytes_read);

            if (!buff_str.compare("EXIT\n"))
            {
                for (CLIENT_PAIR client_pair : clients_vec)
                {
                    string msg_str("exit server");
                    int socket_id = getSocketId(client_pair.second);
                    send(socket_id, (void*)msg_str.c_str(),
                                            msg_str.length(), 0);
                }
                cout << "EXIT command is typed: server is shutting down" <<endl;
                exit(SUCCESS);
            }
        }
        //else its some IO operation on some other socket :)
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];
            if (FD_ISSET(sd, &clientsfds))
            {

                //Check if it was for closing , and also read the incoming message
                int res = readRequest(sd);

                if (res == CLOSE_REQUEST)
                {
                    close(sd);
                    client_socket[i] = 0;
                }
            }
        }
    }
}

Server::Server()
{
    clients_vec = vector<CLIENT_PAIR>();
}

int Server::readRequest(int client_socket_fd) {
    char buff[MAX_SIZE_REQUEST];
    bzero(buff, sizeof(buff));

    int bytes_read = recv(client_socket_fd, buff, MAX_SIZE_REQUEST, 0);
    if (bytes_read  <= FAILURE)
    {
        return FAILURE;
    }
    if(bytes_read > 0)
    {
        string buff_str(buff);
        buff_str = buff_str.substr(0, bytes_read);
        char* alignedBuff = const_cast<char*>(buff_str.c_str());
        int res = parseRequest(client_socket_fd, alignedBuff);
        return res;
    }
    return FAILURE;
}

int Server::parseRequest(int client_socket_fd, char* buff)
{
    string msg_str(buff);
    string delimiter = " ";
    int pos = msg_str.find(delimiter);

    string command = msg_str.substr(0, pos);
    msg_str.erase(0, pos + delimiter.length());

    if (!command.compare(NEW_CONNECTION))
    {
        handleNewConnect(client_socket_fd, msg_str);
    }
    else if(!command.compare(CREATE_GROUP))
    {
        handleGroupReq(client_socket_fd, msg_str);
    }
    else if (!command.compare(SEND))
    {
        handleSendReq(client_socket_fd, msg_str);
    }
    else if(!command.compare(WHO))
    {
        handleWhoReq(client_socket_fd, msg_str);
    }
    else if (!command.compare(EXIT))
    {
        handleExitReq(client_socket_fd, msg_str);
        return CLOSE_REQUEST;
    }
    return 0;
}


int Server::handleGroupReq(int client_socket_fd, string msg_str) {
    string delimiter = " ";
    int pos = msg_str.find(delimiter);
    string group_name = msg_str.substr(0, pos);
    msg_str.erase(0, pos + delimiter.length());
    delimiter = "\n";
    pos = msg_str.rfind(delimiter);
    msg_str.erase(pos, 1);
    string sender_name = getUsername(client_socket_fd);
    string send_msg_failure = "ERROR: failed to create group \"" + group_name + "\".";
    string send_msg_ok = "Group \"" + group_name +
                                      "\" was created successfully.";

    if((groups_map.find(group_name) != groups_map.end()) ||
            (doesUsernameExist(group_name)))
    {
        cout << sender_name << ": " << send_msg_failure << endl;
        send(client_socket_fd, (void*)send_msg_failure.c_str(),
             send_msg_failure.length(), 0);
        return FAILURE;
    }
    groups_map[group_name] = GROUP_SET();
    delimiter = ",";
    if (msg_str.find(delimiter) == std::string::npos)
    {
        if (doesUsernameExist(msg_str))
        {
            groups_map[group_name].insert(msg_str);
        }
    }
    else
    {
        while ((pos = msg_str.find(delimiter))!= (int)std::string::npos) {
            string group_member = msg_str.substr(0, pos);
            if (doesUsernameExist(group_member))
            {
                groups_map[group_name].insert(group_member);
            }
            msg_str.erase(0, pos + delimiter.length());
        }
        if (doesUsernameExist(msg_str))
        {
            groups_map[group_name].insert(msg_str);
        }
    }
    bool in_the_group = false;

    for (string username : groups_map[group_name])
    {
        if (!username.compare(sender_name))
        {
            in_the_group = true;
            break;
        }
    }

    if(!in_the_group)
    {
        groups_map[group_name].insert(sender_name);
    }
    if(groups_map[group_name].size() < 2)
    {
        groups_map[group_name].clear();
        groups_map.erase(groups_map.find(group_name));
        cout << sender_name << ": " << send_msg_failure << endl;
        send(client_socket_fd, (void*)send_msg_failure.c_str(),
             send_msg_failure.length(), 0);
        return FAILURE;
    }

    int bytes_sent = (int) send(client_socket_fd, (void*)send_msg_ok.c_str(),
                                send_msg_ok.length(), 0);
    if(bytes_sent == (int)send_msg_ok.length())
    {
        cout << sender_name << ": " << send_msg_ok << endl;
        return SUCCESS;
    }
    return FAILURE;

}

int Server::handleSendReq(int client_socket_fd, string msg_str)
{
    string delimiter = " ";
    int pos = msg_str.find(delimiter);
    string receiver_name = msg_str.substr(0, pos);
    msg_str.erase(0, pos + delimiter.length());
    delimiter = "\n";
    pos = msg_str.rfind(delimiter);
    msg_str.erase(pos, 1);
    string sender_name = getUsername(client_socket_fd);
    string send_msg_failure = "ERROR: failed to send.";
    string send_msg_ok = "Sent successfully.";
    int bytes_sent;
    //send to himself
    if(!receiver_name.compare(sender_name))
    {
        cout << sender_name << ": ERROR: failed to send \"" << msg_str << "\" to "
             << receiver_name << endl;
        send(client_socket_fd, (void*)send_msg_failure.c_str(),
             send_msg_failure.length(), 0);
        return FAILURE;
    }
    //another client
    if(doesUsernameExist(receiver_name))
    {
        int other_socket_fd = getSocketId(receiver_name);

        bytes_sent = (int) send(other_socket_fd, (void*)msg_str.c_str(),
                                msg_str.length(), 0);
        if(bytes_sent == (int)msg_str.length())
        {
            send(client_socket_fd, (void*)send_msg_ok.c_str(),
                 send_msg_ok.length(), 0);
            cout << sender_name << ": \"" << msg_str <<
                 "\" was sent successfully to " << receiver_name << "." << endl;
            return SUCCESS;
        }
        return FAILURE;
    }
    //send to group
    if(groups_map.find(receiver_name) != groups_map.end())
    {
                GROUP_SET cur_group = groups_map[receiver_name];
        bool is_sender_in_group = false;
        for (string name : cur_group)
        {
            if (name.compare(sender_name) == 0)
            {
                is_sender_in_group = true;
            }
        }
        if (!is_sender_in_group)
        {
            cout << sender_name << ": ERROR: failed to send \"" << msg_str << "\" to "
                 << receiver_name << endl;
            send(client_socket_fd, (void*)send_msg_failure.c_str(),
                 send_msg_failure.length(), 0);
            return FAILURE;
        }
        bool is_all_msg_delivered = true;
        for (string username : cur_group)
        {
            if (username.compare(sender_name))
            {
                int socket_id = getSocketId(username);
                bytes_sent = (int) send(socket_id, (void*)msg_str.c_str(),
                                        msg_str.length(), 0);
                if(bytes_sent != (int)msg_str.length())
                {
                    is_all_msg_delivered = false;
                }
            }
        }
        if (is_all_msg_delivered)
        {
            cout << sender_name << ": \"" << msg_str <<
                 "\" was sent successfully to " << receiver_name << "." << endl;
            send(client_socket_fd, (void*)send_msg_ok.c_str(),
                 send_msg_ok.length(), 0);
            return SUCCESS;
        }
        return FAILURE;
    }
    cout << sender_name << ": ERROR: failed to send \"" << msg_str << "\" to "
         << receiver_name << endl;
    send(client_socket_fd, (void*)send_msg_failure.c_str(),
         send_msg_failure.length(), 0);
    return FAILURE;
}

int Server::handleWhoReq(int client_socket_fd, string msg_str)
{
    string sender_name = getUsername(client_socket_fd);
    cout << sender_name << ": Requests the currently connected client names.\n";
    string msg;
    string delimiter(",");

    sort(clients_vec.begin(), clients_vec.end(), clientsComp());
    for (CLIENT_PAIR client_pair : clients_vec)
    {
        msg += delimiter + client_pair.second;
    }
    int pos = msg.find(delimiter);
    if (pos == 0)
    {
        msg.erase(0, 1);
    }
    //msg += ".\n";
    int bytes_sent = (int) send(client_socket_fd, (void*)msg.c_str(),
                            msg.length(), 0);
    if(bytes_sent == (int) msg.length())
    {
        return SUCCESS;
    }
    return FAILURE;
}

int Server::handleExitReq(int client_socket_fd, string msg_str)
{
    string sender_name = getUsername(client_socket_fd);
    removeUserFromAllGroup(sender_name);
    removeUserFromClients(sender_name);
    cout << sender_name << ": Unregistered successfully." << endl;
    string send_msg = "Unregistered successfully.";
    int bytes_sent = (int) send(client_socket_fd, send_msg.c_str(), send_msg.length(), 0);
    if(bytes_sent == (int)send_msg.length())
    {
        return SUCCESS;
    }
    return FAILURE;
}

int Server::handleNewConnect(int client_socket_fd, string msg_str)
{
    string name = msg_str;

    bool isGroupName = false;
    for (pair<string, GROUP_SET> group : groups_map)
    {
        if (name.compare(group.first) == 0)
        {
            isGroupName = true;
            break;
        }
    }

    for (unsigned int i = 0; i < clients_vec.size(); i++)
    {
        if ((!clients_vec.at(i).second.compare(name)) || isGroupName)
        {
            string msg = "Client name is already in use.\n";
            send(client_socket_fd, (void*)msg.c_str(),
                 msg.length(), 0);
            cout << msg_str << " failed to connect." <<endl;
            return FAILURE;
        }
    }
    clients_vec.push_back(make_pair(client_socket_fd, name));
    string msg = "Connected Successfully.";
    send(client_socket_fd, (void*)msg.c_str(),
         msg.length(), 0);
    cout << msg_str << " connected." <<endl;//name
    return SUCCESS;
}

string Server::getUsername(int socket)
{
    for (CLIENT_PAIR pair: clients_vec)
    {
        if (pair.first == socket)
        {
            return pair.second;
        }
    }
    return nullptr;
}

int Server::getSocketId(string name)
{
    for (CLIENT_PAIR pair: clients_vec)
    {
        if (!pair.second.compare(name))
        {
            return pair.first;
        }
    }
    return -1;
}

bool Server::doesUsernameExist(string name)
{
    for (CLIENT_PAIR pair: clients_vec)
    {
        if (!pair.second.compare(name))
        {
            return true;
        }
    }
    return false;
}

void Server::removeUserFromAllGroup(string username)
{
    GROUP_SET::iterator inside_it;
    for (map<string, GROUP_SET>::iterator it = groups_map.begin(); it != groups_map.end(); ++it)
    {
        inside_it = it->second.find(username);
        if (inside_it != it->second.end())
        {
            it->second.erase(inside_it);
        }
    }
}

void Server::removeUserFromClients(string username)
{
    for (unsigned int i = 0; i < clients_vec.size(); i++)
    {
        if (clients_vec.at(i).second.compare(username) == 0)
        {
            clients_vec.erase(clients_vec.begin()+ i);
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2)
    {
        cout << "Usage: whatsappServer portNum" << endl;
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    Server* server = new Server();
    int socket = server->establishServer((unsigned short) port);
    if (socket <= 0)
    {
        cout << "Usage: whatsappServer portNum" << endl;
        delete (server);
        exit(EXIT_FAILURE);
    }
    if(server->runServer(socket) == FAILURE)
    {
        delete(server);
        exit(EXIT_FAILURE);
    };

    delete(server);
    return 0;
}
