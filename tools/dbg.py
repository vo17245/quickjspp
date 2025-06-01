import socket
import threading
def tcp_connect(host:str,port:int)->socket.socket:
    """
    Connect to a TCP server at the specified host and port.
    
    Args:
        host (str): The hostname or IP address of the server.
        port (int): The port number to connect to.
        
    Returns:
        socket.socket: A socket object connected to the server.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    return sock

class TcpTunnel:
    def __init__(self,sock:socket.socket):
        self.sock= sock
    def read(self,size:int)->bytes:
        """
        Read a specified number of bytes from the socket.
        
        Args:
            size (int): The number of bytes to read.
            
        Returns:
            bytes: The data read from the socket.
        """
        return self.sock.recv(size)
    def write(self,buf:bytes):
        """
        Write data to the socket.
        
        Args:
            buf (bytes): The data to write to the socket.
        """
        self.sock.sendall(buf)

class LineTunnel:
    def __init__(self,tcp_tunnel:TcpTunnel):
        self.tcp_tunnel = tcp_tunnel
    def read_line(self)->str:
        b:bytes=self.tcp_tunnel.read(1)
        buf:bytearray=bytearray()
        while b[0]!=ord('\n'):
            buf.append(b[0])
            b=self.tcp_tunnel.read(1)
        return buf.decode('utf-8')
    def write_line(self,msg:str):
        """
        Write a line of text to the tunnel.
            This method appends a newline character to the message before sending it.
        
        Args:
            msg (str): The message to write.
        """
        self.tcp_tunnel.write((msg + '\n').encode('utf-8'))            

def print_message(tunnel:LineTunnel,stop:bool):
    while not stop:
        line=tunnel.read_line()
        print(line)

if __name__=="__main__":
    sock = tcp_connect('localhost', 8173)
    tunnel = TcpTunnel(sock)
    line_tunnel = LineTunnel(tunnel)
    stop=False
    thread = threading.Thread(target=print_message, args=(line_tunnel, stop))
    thread.start()
    s=""
    while s!='exit':
        s=input()
        line_tunnel.write_line(s)
    
    sock.close()
            

    
