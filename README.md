portall
=======

Multiport netcat/socat


purpose
-------

Portforward multiple ports through a single (SSH) tunnel.

How do we do it?
----------------

1. Read the config file to find out which inputs, outputs, and tunnel we need
   to configure.
2. Create the output channels.
3. Connect to the tunnel (or listen for incoming traffic from the tunnel).
4. Create the input channels.
5. Start forwarding from input to tunnel, and from tunnel to output.

