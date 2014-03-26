portall
=======

Multiport netcat/socat


purpose
-------

Portforward multiple ports through a single (SSH) tunnel.

How do we do it?
----------------

1. Create multiple listeners (UDP and TCP) from the commandline (t1234, u4321, etc)
2. Create one output stream socket to the SSH tunnel
3. All traffic received from the listeners is sent through the one output socket, with a header to let the other side know which port was used
4. The other side creates one listener from the SSH tunnel
5. This side creates multiple outputs from the commandline (t127.0.0.1:1234, u10.0.0.1:4321, etc)
6. This side receives traffic through the SSH tunnel, strips the special header, and sends the payload as original UDP or TCP packets to the outputs
