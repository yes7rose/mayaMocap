#include <maya/MGlobal.h>


#include "tcpSocket.h"
#include <stdio.h>

#ifdef _WIN32
#include <Windows.h>
#define THREAD_TYPE HANDLE
#else
#include <pthread.h>
#define THREAD_TYPE pthread_t
#endif

#include <list>
#include <iostream>
#include <string>

class ListenThread
{
public:
    ListenThread() : 
		mPort(-1), 
		server(NULL), 
		running(false) 
		 {}

    int       mPort;
    TcpSocket *server;
    bool      running;
	THREAD_TYPE    mThread;
	MString        mModule;

#ifdef _WIN32
    static DWORD WINAPI commandThread(VOID *ptr)
#else
    static void* commandThread(void *ptr)
#endif
	{
        ListenThread *t = (ListenThread*)ptr;
        t->commandThread_();
	return NULL;
    }

    void commandThread_()
    {

        std::list<TcpSocket *> connections;


        while( running ) 
        {

            // Non-blocking, will return null if there is not a new connection
            TcpSocket *con = server->accept(0);

            bool action = false;

            if(con != NULL)
            {
                // New connection
                printf("New connection\n");
                connections.push_back(con);
                action = true;
            }

            // For each active connection
            for( std::list<TcpSocket*>::iterator it = connections.begin(); it != connections.end(); it++)
            {
                TcpSocket *eachSocket = *it;
                // Non blocking - will return -1 on error, 0 for no data or >0 with data size
                int ret = eachSocket->receive(0);
                if( ret == -1 )
                {
                    // Connection closed - remove it from the list
                    printf("Connection closed\n");
                    it = connections.erase(it);
                    delete eachSocket;

                }
                if( ret > 0 )
                {
                    // We got ourself some data!  Yee-har!
                    action = true;

                    std::string data;
                    while( eachSocket->get( data ) ) 
                    {

						MString cmd("import ");
						cmd += mModule;
						cmd += ";";
						cmd += mModule;
						cmd += ".execute('";
                        cmd += data.c_str();
                        cmd += "');";
                        MGlobal::executePythonCommandOnIdle(cmd, true);
                        printf("%s\n", cmd.asChar());
                    }
                    
                }
            }

#ifdef _WIN32
			if(!action) Sleep( 500 );
#else
            if(!action) { usleep( 500000) ; }
#endif
        }

        // Close open connections
        for( std::list<TcpSocket*>::iterator i = connections.begin(); i != connections.end(); i++)
        {
            (*i)->close();
            delete *i;
        }
        delete server;
        server = NULL;
        printf("Thread function complete\n");
    }

    int start(int port, MString module)
    {
		mModule = module;

        if( server != NULL)
        {
            MGlobal::displayError("Server already running.");
            return 0;
        }

        server = new TcpSocket();
        
        if(!server->bind( port ))
        {
            delete server;
            server = NULL;
            return -1;    
        }

        if(!server->listen())
        {
            delete server;
            server = NULL;
            return -1;
        }

        mPort = port;

        running = true;
#ifdef _WIN32
		mThread = CreateThread( NULL, 0, &commandThread, (void*)this, NULL, NULL);
#else
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        int ret = pthread_create( &mThread, &attr, &commandThread, (void*)this );
#endif

        return 0;
    }

    bool stop()
    {
        running = false;
	server->close();
	return true;
    }
};
