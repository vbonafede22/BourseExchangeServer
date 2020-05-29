#include "protocol.h"
#include "debug.h"
#include "csapp.h"

#include <unistd.h>
#include <stdlib.h>

int proto_send_packet(int fd, BRS_PACKET_HEADER *hdr, void *payload){
    int my_size = ntohs(hdr -> size);
    debug("send size: %d", my_size);

    //write the packet header to the "wire"
    if(write(fd, hdr, sizeof(BRS_PACKET_HEADER)) == 0){
        debug("error writing 1");
        return -1;
    }

    if(my_size > 0 && payload != NULL){
        //write the payload data to the "wire"
        if(write(fd, payload, my_size) == 0){
            debug("error writing 2");
            return -1;
        }
    }
    return 0;
}

int proto_recv_packet(int fd, BRS_PACKET_HEADER *hdr, void **payloadp){
    //read packet from the "wire"
    if(read(fd, hdr, sizeof(BRS_PACKET_HEADER)) == 0){
        debug("error reading 1");
        return -1;
    }

    if(hdr -> size != 0 && payloadp != NULL){
        //malloc payload
        *payloadp = malloc(hdr -> size);         //needs to be freed at some point
        //read the payload from the "wire"
        if(read(fd, *payloadp, hdr -> size) == 0){
            debug("error reading 2");
            return -1;
        }
    }
    return 0;
}
