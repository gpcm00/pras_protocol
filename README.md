# PRAS Protocol (Process Relay and Streaming)
This protocol was developed to work on a Raspberry Pi.

The device acts as the server and currently allows only one client.

The client can write to named FIFOs as specified in the config file. I will add a tutorial in the wiki page on how to use the protocol eventually.

Other processes can stream data to the client over UDP by writing to the FIFO file.

I am adding the option to support multiple clients. I also want to test Mbed TLS instead of OpenSSL to see if I can reduce the memory footprint. This app uses about 8 MB of RAM.
