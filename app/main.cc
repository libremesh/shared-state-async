#include "io_context.hh"
#include "socket.hh"
#include "task.hh"
#include "sharedstate.hh"
#include <iostream>
#include <array>
#include <unistd.h>
#include "async_command.hh"
#include "piped_async_command.hh"


#define BUFFSIZE 256

/// @brief coro in charge of information handling. It takes the received states, merges it and return the updated status using the socket.
/// @param socket 
/// @return true if everything goes fine
std::task<bool> inside_loop(Socket &socket)
{
    char socbuffer[BUFFSIZE] = {0};
    //TODO: lo que no entra en el buffer se procesa como otro mensaje... 
    ssize_t nbRecv = co_await socket.recv(socbuffer, (sizeof socbuffer)-1);
    //ssize_t nbSend = 0;
    // TODO: crear una task que invoque al shstate empezar por invocar echo.????
    std::cout << "RECIVING (" << socbuffer << "):" << '\n';
    //std::string merged = SharedState::mergestate(buffer,&socket);
    std::array<char, BUFFSIZE> buffer;
    std::string merged;
    std::string cmd = "sleep 1 && echo '" + std::string(socbuffer) + "'";
    //std::unique_ptr<AsyncCommand> filesocket = std::make_unique<AsyncCommand>(cmd,&socket);
    std::unique_ptr<PipedAsyncCommand> asyncecho = std::make_unique<PipedAsyncCommand>("cat",&socket);
    std::cout << "asyncecho created (" << socbuffer << "):" << '\n';
    //PipedAsyncCommand * asyncecho = new PipedAsyncCommand("cat",&socket);
    //AsyncCommand* filesocket = new AsyncCommand (pipe,&socket);
    //co_await filesocket->recvfile(buffer.data(),BUFFSIZE);
    co_await asyncecho->writepipe(socbuffer,nbRecv);
    std::cout << "writepipe (" << socbuffer << "):" << '\n';
    co_await asyncecho->readpipe(buffer.data(),BUFFSIZE);
    
    merged=buffer.data();
    std::cout << "readpipe (" << merged << "):" << '\n';
    //filesocket=nullptr;
    //filesocket.reset(nullptr);
    asyncecho.reset(nullptr);
    //problemade manejode errores... que pasa cuando se cuelgan los endpoints y ya no reciben.
    //sin esta linea se genera un enter que no se recibe y el programa explota
    merged.erase(std::remove(merged.begin(), merged.end(), '\n'), merged.cend());

    // esto no parece necesario, podria quedarse aqui para siempre  ? 
    // while (nbSend < nbRecv)
    //{
    std::cout << "SENDING (" << merged << "):" << '\n';
    ssize_t res = co_await socket.send(merged.data(), merged.size());
    if (res <= 0)
        co_return false;
    //nbSend += res;
    //}
    //TODO: esto va al std error ?? SERA QUE PODEMOS USAR UNA LIBRERIA DE LOGGFILE 
    std::cerr << "DONE (" << nbRecv << "):" << '\n';
    if (nbRecv <= 0)
        co_return false;
    co_return true;
}

std::task<> echo_socket(std::shared_ptr<Socket> socket)
{
    bool run = true;
    while (run)
    {
        std::cout << "BEGIN\n";
        run = co_await inside_loop(*socket);
        std::cout << "END\n";
    }
}

std::task<> accept(Socket &listen)
{
    while (true)
    {
        auto socket = co_await listen.accept();
        auto t = echo_socket(socket);
        t.resume();
    }
}

int main()
{
    IOContext io_context{};
    Socket listen{"3490", io_context};
    auto t = accept(listen);
    t.resume();
    io_context.run();
}
