# PRAS Protocol (Process Relay and Streaming)
This protocol was developed to work on a Raspberry Pi. 

The device acts as the server, and currently only allows one client.

The client is able to write to named FIFOs as in the config file. I will add more info on the protocol evenrually.

Other process can stream data to client over UDP by writing to the FIFO file.

I am adding the option to have multiple clients. I also want to test Mbed TLS instead of OpenSSL to see if I can reduce the memory footprint. This app uses about 8MB of RAM.
