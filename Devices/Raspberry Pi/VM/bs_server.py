import SocketServer
import botspeak
import time

class BotSpeakTCPHandler(SocketServer.BaseRequestHandler):
    """
    The RequestHandler class for our server.

    It is instantiated once per connection to the server, and must
    override the handle() method to implement communication to the
    client.
    """
    def handle(self):
        while 1:
            # self.request is the TCP socket connected to the client
            self.data = self.request.recv(1024)
        
            if not self.data:
                break;

            # print out the command
            # print "{} wrote:".format(self.client_address[0]) 
            # print self.data
            
            self.data = self.data.strip()

            try:
                val = botspeak.BotSpeak().eval(self.data)
            except:
                val = -1
                
            val = str(val).strip('[\"\']') + '\r\n'
            self.request.sendall(val)

if __name__ == "__main__":
    HOST, PORT = '', 9999

    print 'Starting Botspeak TCP server...'

    # Create the server, binding to localhost on port 9999
    SocketServer.TCPServer.allow_reuse_address = True
    server = SocketServer.TCPServer((HOST, PORT), BotSpeakTCPHandler)

    print 'Botspeak server initiated'
    print 'Listening on port:' + str(PORT)

    # Activate the server; this will keep running until you
    # interrupt the program with Ctrl-C
    try:
        botspeak.initBrickPi()    
        server.serve_forever()
    except KeyboardInterrupt:
        botspeak.haltBSP()
        server.shutdown()
        server.socket.close()
        print "Quiting botspeak server"

