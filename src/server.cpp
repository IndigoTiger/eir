#include "server.h"
#include "exceptions.h"
#include "event_internal.h"

#include "logger.h"

#include <paludis/util/private_implementation_pattern-impl.hh>

#include <queue>
#include <cstdlib>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

using namespace eir;
using paludis::Implementation;

namespace paludis
{
    template<>
    struct Implementation<eir::Server> {

        int socketfd;
        int port;
        std::string servername;
        hostent *host;
        sockaddr_in server;

        std::queue<std::string> _send_queue;

        Server::Handler _handler;

        int _burst;

        void maybe_send_stuff();
        void io_event();
        void do_receive_stuff();
        void run();

        enum { bufsize = 1024 };
        char recvbuf[bufsize];
        int recvpos;

        Implementation(Server::Handler h) : _handler(h), _burst(0), recvpos(0)
        {
        }

        ~Implementation()
        {
        }
    };
}

Server::Server(const Handler& handler)
    : paludis::PrivateImplementationPattern<Server>(new paludis::Implementation<Server>(handler))
{
}

Server::~Server()
{
}

void Server::connect(std::string host, std::string port)
{
    _imp->servername = host;
    _imp->port = std::atoi(port.c_str());

    if ((_imp->socketfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        throw ConnectionError(strerror(errno));

    if (! (_imp->host = gethostbyname(_imp->servername.c_str())))
    {
        if (! (_imp->host = gethostbyaddr(_imp->servername.c_str(), _imp->servername.length(), AF_INET)))
            throw ConnectionError(strerror(errno));
    }

    _imp->server.sin_family = AF_INET;
    _imp->server.sin_port = htons(_imp->port);
    memcpy(&_imp->server.sin_addr, _imp->host->h_addr, _imp->host->h_length);

    if ((::connect(_imp->socketfd, reinterpret_cast<sockaddr*>(&_imp->server), sizeof(_imp->server))) == -1)
        throw ConnectionError(strerror(errno));

    int flags = fcntl(_imp->socketfd, F_GETFL, 0);
    fcntl(_imp->socketfd, F_SETFL, flags | O_NONBLOCK);
    fcntl(_imp->socketfd, F_SETFD, FD_CLOEXEC);

}

void Server::disconnect(std::string reason)
{
    std::string line = "QUIT :" + reason + "\r\n";
    int flags = fcntl(_imp->socketfd, F_GETFL, 0);
    fcntl(_imp->socketfd, F_SETFL, flags & ~O_NONBLOCK);
    write(_imp->socketfd, line.c_str(), line.length());
    close(_imp->socketfd);
    _imp->socketfd = -1;
}

void Server::purge()
{
    while (! _imp->_send_queue.empty())
        _imp->_send_queue.pop();
}

void Server::send(std::string line)
{
    Context c("Sending line " + line);
    std::string::size_type p;

    p = line.rfind("\r\n");
    if(p != std::string::npos)
        line = line.substr(0, p);
    else
    {
        p = line.rfind("\n");
        if (p != std::string::npos)
            line = line.substr(0, p);
    }
    line += "\r\n";

    _imp->_send_queue.push(line);

    _imp->maybe_send_stuff();
}

void Implementation<Server>::maybe_send_stuff()
{
    while(_burst < 4 && ! _send_queue.empty())
    {
        std::string line = _send_queue.front();
        Logger::get_instance()->Log(NULL, NULL, Logger::Raw, "--> " + line);
        write(socketfd, line.c_str(), line.size());
        _send_queue.pop();
        ++_burst;
    }
}

void Implementation<Server>::io_event()
{
    if (_burst > 0)
        --_burst;
    maybe_send_stuff();
}

void Implementation<Server>::do_receive_stuff()
{
    std::queue<std::string> recv_lines;

    while(true)
    {
        int error;
        int r = read(socketfd, recvbuf + recvpos, bufsize - recvpos);

        if (r == 0)
            break;
        else if (r == -1)
        {
            error = errno;
            if (error == EAGAIN)
                break;
            else
                throw ConnectionError(strerror(error));
        }

        recvpos += r;

        int start = 0, end = 0;

        while(true)
        {
            while (end < recvpos && recvbuf[end] != '\n')
                ++end;

            if (end == recvpos)
                break;

            ++end;
            std::string line(recvbuf + start, end - start);
            recv_lines.push(line);

            start = end;
        }

        memmove(recvbuf, recvbuf + start, end - start);
        recvpos -= start;

    }

    while (! recv_lines.empty())
    {
        _handler(recv_lines.front());
        recv_lines.pop();
    }
}

void Server::run()
{
    _imp->run();
}

void Implementation<Server>::run()
{
    Context c("In main message loop");

    EventManager::id _send_id = EventManager::get_instance()->add_recurring_event(2,
                                    std::tr1::bind(&Implementation<Server>::io_event, this));

    do_receive_stuff();

    while(true)
    {
        timeval timeout;
        timeout.tv_sec = timeout.tv_usec = 0;

        fd_set read, write, except;
        FD_ZERO(&read);
        FD_ZERO(&write);
        FD_ZERO(&except);
        FD_SET(socketfd, &read);
        FD_SET(socketfd, &write);
        FD_SET(socketfd, &except);

        timeout.tv_sec = static_cast<EventManagerImpl*>(EventManager::get_instance())->next_event_time() - time(NULL);

        if (timeout.tv_sec < 0)
            timeout.tv_sec = 0;

        select(1, &read, NULL, NULL, &timeout);

        do_receive_stuff();

        static_cast<EventManagerImpl*>(EventManager::get_instance())->run_events();
    }

    EventManager::get_instance()->remove_event(_send_id);
}
